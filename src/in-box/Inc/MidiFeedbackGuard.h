// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

#include <functional>
#include <utility>
#include <vector>

#include "MidiFeedbackDetector.h"

namespace WindowsMidiServicesInternal
{
    enum class MidiFeedbackEvent : uint8_t
    {
        // traffic was held to test it, and it was not a loop
        NotALoop = 0,

        Tripped = 1,
    };

    // One direction of one loopback. Sits between the sender and the destination callback while
    // feedback protection is on, and is not called at all while it is off.
    //
    // Deliveries never happen under the lock. Held messages are released by exactly one thread at
    // a time, and anything sent while that is happening queues behind them, so order is kept.
    //
    // The timer callback uses a raw pointer and never takes a reference to the guard or to its
    // owner, so the destructor can never run on the timer thread and wait for itself.
    class MidiFeedbackGuard
    {
    public:
        // Runs outside the lock, on a send thread or a threadpool thread. Keep it short: a trip
        // should hand the real work (endpoint properties, notifications) to another thread.
        using EventHandler = std::function<void(MidiFeedbackEvent, MidiFeedbackTripInfo const&)>;

        static constexpr size_t MaximumHeldMessages{ 16'384 };
        static constexpr size_t MaximumHeldWords{ 65'536 };

        explicit MidiFeedbackGuard(_In_ EventHandler handler) noexcept
        {
            try
            {
                m_handler = std::move(handler);
            }
            catch (...)
            {
            }

            m_timer = ::CreateThreadpoolTimer(&MidiFeedbackGuard::TimerCallback, this, nullptr);
        }

        MidiFeedbackGuard(_In_ MidiFeedbackGuard const&) = delete;
        MidiFeedbackGuard& operator=(_In_ MidiFeedbackGuard const&) = delete;

        ~MidiFeedbackGuard() noexcept
        {
            if (m_timer != nullptr)
            {
                ::SetThreadpoolTimer(m_timer, nullptr, 0, 0);
                ::WaitForThreadpoolTimerCallbacks(m_timer, TRUE);
                ::CloseThreadpoolTimer(m_timer);
                m_timer = nullptr;
            }
        }

        // Returns what the destination returned when the message went straight through, and S_OK
        // when it was held or dropped.
        HRESULT Send(
            _In_opt_ IMidiCallback* const destination,
            _In_ MessageOptionFlags const optionFlags,
            _In_reads_bytes_(size) PVOID const message,
            _In_ UINT const size,
            _In_ LONGLONG const position,
            _In_ LONGLONG const context,
            _Out_opt_ bool* const dropped = nullptr) noexcept
        {
            if (dropped != nullptr)
            {
                *dropped = false;
            }

            if (m_timer == nullptr)
            {
                // Without a timer a pause could never end, so there is no test at all.
                return destination == nullptr ? S_OK : destination->Callback(optionFlags, message, size, position, context);
            }

            auto const words = static_cast<uint32_t const*>(message);
            auto const wordCount = static_cast<uint32_t>(size / sizeof(uint32_t));

            std::vector<HeldMessage> discardedMessages{};
            std::vector<uint32_t> discardedWords{};

            bool releaseHere{ false };
            bool deliverHere{ false };
            bool tripped{ false };
            uint32_t notALoop{ 0 };
            MidiFeedbackTripInfo info{};

            {
                auto lock = m_lock.lock_exclusive();

                auto const nowUs = NowMicroseconds();
                auto const notALoopBefore = m_detector.NotALoopCount();

                // A sender that kept going ends its own pause as soon as the time is up, rather
                // than waiting on the timer, which can be late.
                auto const timerAction = m_detector.OnTimer(nowUs);

                if (timerAction == MidiFeedbackTimerAction::Release)
                {
                    releaseHere = BeginReleaseLocked();
                }
                else if (timerAction == MidiFeedbackTimerAction::Trip)
                {
                    TripLocked(discardedMessages, discardedWords);
                    tripped = true;
                }

                auto const action = m_detector.OnSend(words, wordCount, nowUs);

                if (action == MidiFeedbackSendAction::Drop && dropped != nullptr)
                {
                    *dropped = true;
                }

                if (action == MidiFeedbackSendAction::Hold)
                {
                    if (!TryHoldLocked(destination, optionFlags, words, wordCount, position, context))
                    {
                        if (m_detector.OnHoldQueueFull())
                        {
                            releaseHere = BeginReleaseLocked() || releaseHere;
                        }

                        // Past every limit. Going straight through out of order is better than
                        // losing it or making the sender wait.
                        deliverHere = !TryHoldLocked(destination, optionFlags, words, wordCount, position, context);
                    }
                }
                else if (action == MidiFeedbackSendAction::Deliver)
                {
                    deliverHere = true;
                }

                notALoop = m_detector.NotALoopCount() - notALoopBefore;

                if (tripped || notALoop != 0)
                {
                    info = m_detector.TripInfo();
                }

                ArmTimerLocked();
            }

            discardedMessages.clear();
            discardedWords.clear();

            if (tripped)
            {
                RaiseEvent(MidiFeedbackEvent::Tripped, info);
            }

            if (notALoop != 0)
            {
                RaiseEvent(MidiFeedbackEvent::NotALoop, info);
            }

            if (releaseHere)
            {
                Release();
            }

            if (deliverHere && destination != nullptr)
            {
                return destination->Callback(optionFlags, message, size, position, context);
            }

            return S_OK;
        }

        // Back to a clean start. With releaseHeld, a test in progress ends and what it held goes
        // out in order; without it, anything held is discarded, which is right when the loopback
        // is being muted or unmuted anyway.
        void Reset(_In_ bool const releaseHeld) noexcept
        {
            std::vector<HeldMessage> discardedMessages{};
            std::vector<uint32_t> discardedWords{};

            bool releaseHere{ false };

            {
                auto lock = m_lock.lock_exclusive();

                if (releaseHeld && m_detector.EndTestAndRelease())
                {
                    releaseHere = BeginReleaseLocked();
                }
                else if (m_detector.Phase() != MidiFeedbackPhase::Releasing)
                {
                    discardedMessages.swap(m_heldMessages);
                    discardedWords.swap(m_heldWords);

                    m_detector.Reset();
                }

                m_trippedTime = {};

                ArmTimerLocked();
            }

            discardedMessages.clear();
            discardedWords.clear();

            if (releaseHere)
            {
                Release();
            }
        }

        bool IsTripped() const noexcept
        {
            auto lock = m_lock.lock_shared();

            return m_detector.Phase() == MidiFeedbackPhase::Tripped;
        }

        // Zero until the guard trips.
        FILETIME TrippedTime() const noexcept
        {
            auto lock = m_lock.lock_shared();

            return m_trippedTime;
        }

        MidiFeedbackTripInfo TripInfo() const noexcept
        {
            auto lock = m_lock.lock_shared();

            return m_detector.TripInfo();
        }

        MidiFeedbackPhase Phase() const noexcept
        {
            auto lock = m_lock.lock_shared();

            return m_detector.Phase();
        }

        static uint64_t NowMicroseconds() noexcept
        {
            static LARGE_INTEGER const frequency = []() noexcept
                {
                    LARGE_INTEGER value{};
                    ::QueryPerformanceFrequency(&value);
                    return value;
                }();

            LARGE_INTEGER counter{};
            ::QueryPerformanceCounter(&counter);

            auto const ticks = static_cast<uint64_t>(counter.QuadPart);
            auto const ticksPerSecond = static_cast<uint64_t>(frequency.QuadPart);

            if (ticksPerSecond == 0)
            {
                return 0;
            }

            return (ticks / ticksPerSecond) * 1'000'000 + ((ticks % ticksPerSecond) * 1'000'000) / ticksPerSecond;
        }

    private:
        struct HeldMessage
        {
            wil::com_ptr_nothrow<IMidiCallback> Destination{};
            LONGLONG Position{ 0 };
            LONGLONG Context{ 0 };
            MessageOptionFlags OptionFlags{ MessageOptionFlags_None };
            uint32_t FirstWord{ 0 };
            uint32_t WordCount{ 0 };
        };

        static VOID CALLBACK TimerCallback(
            _Inout_ PTP_CALLBACK_INSTANCE,
            _Inout_opt_ PVOID context,
            _Inout_ PTP_TIMER) noexcept
        {
            if (context != nullptr)
            {
                static_cast<MidiFeedbackGuard*>(context)->OnTimer();
            }
        }

        void OnTimer() noexcept
        {
            std::vector<HeldMessage> discardedMessages{};
            std::vector<uint32_t> discardedWords{};

            bool releaseHere{ false };
            bool tripped{ false };
            uint32_t notALoop{ 0 };
            MidiFeedbackTripInfo info{};

            {
                auto lock = m_lock.lock_exclusive();

                m_timerDeadlineUs = 0;

                auto const notALoopBefore = m_detector.NotALoopCount();
                auto const action = m_detector.OnTimer(NowMicroseconds());

                if (action == MidiFeedbackTimerAction::Release)
                {
                    releaseHere = BeginReleaseLocked();
                }
                else if (action == MidiFeedbackTimerAction::Trip)
                {
                    TripLocked(discardedMessages, discardedWords);
                    tripped = true;
                }

                notALoop = m_detector.NotALoopCount() - notALoopBefore;
                info = m_detector.TripInfo();

                ArmTimerLocked();
            }

            discardedMessages.clear();
            discardedWords.clear();

            if (tripped)
            {
                RaiseEvent(MidiFeedbackEvent::Tripped, info);
            }

            if (notALoop != 0)
            {
                RaiseEvent(MidiFeedbackEvent::NotALoop, info);
            }

            if (releaseHere)
            {
                Release();
            }
        }

        // True when the caller is now the one thread releasing.
        bool BeginReleaseLocked() noexcept
        {
            if (m_releasing)
            {
                return false;
            }

            m_releasing = true;

            return true;
        }

        void TripLocked(
            _Inout_ std::vector<HeldMessage>& discardedMessages,
            _Inout_ std::vector<uint32_t>& discardedWords) noexcept
        {
            // Dropping what was held is what finally breaks the circle.
            discardedMessages.swap(m_heldMessages);
            discardedWords.swap(m_heldWords);

            ::GetSystemTimeAsFileTime(&m_trippedTime);
        }

        bool TryHoldLocked(
            _In_opt_ IMidiCallback* const destination,
            _In_ MessageOptionFlags const optionFlags,
            _In_reads_(wordCount) uint32_t const* const words,
            _In_ uint32_t const wordCount,
            _In_ LONGLONG const position,
            _In_ LONGLONG const context) noexcept
        {
            // A release in progress may run past the limits, so what arrives meanwhile can still
            // queue behind it instead of jumping ahead.
            auto const limitMessages = m_releasing ? MaximumHeldMessages * 2 : MaximumHeldMessages;
            auto const limitWords = m_releasing ? MaximumHeldWords * 2 : MaximumHeldWords;

            if (m_heldMessages.size() >= limitMessages || m_heldWords.size() + wordCount > limitWords)
            {
                return false;
            }

            try
            {
                HeldMessage held{};
                held.Destination = destination;
                held.Position = position;
                held.Context = context;
                held.OptionFlags = optionFlags;
                held.FirstWord = static_cast<uint32_t>(m_heldWords.size());
                held.WordCount = wordCount;

                m_heldWords.insert(m_heldWords.end(), words, words + wordCount);

                try
                {
                    m_heldMessages.push_back(std::move(held));
                }
                catch (...)
                {
                    m_heldWords.resize(held.FirstWord);
                    throw;
                }
            }
            catch (...)
            {
                return false;
            }

            return true;
        }

        void Release() noexcept
        {
            std::vector<HeldMessage> messages{};
            std::vector<uint32_t> words{};

            uint32_t notALoop{ 0 };
            MidiFeedbackTripInfo info{};

            for (;;)
            {
                {
                    auto lock = m_lock.lock_exclusive();

                    if (m_heldMessages.empty())
                    {
                        auto const notALoopBefore = m_detector.NotALoopCount();

                        m_releasing = false;
                        m_detector.OnReleaseComplete(NowMicroseconds());

                        notALoop = m_detector.NotALoopCount() - notALoopBefore;
                        info = m_detector.TripInfo();

                        // A flood can leave megabytes of queue behind. Nothing needs it until the
                        // next test, which is at least a minute away.
                        if (m_heldMessages.capacity() > 1'024)
                        {
                            std::vector<HeldMessage>{}.swap(m_heldMessages);
                            std::vector<uint32_t>{}.swap(m_heldWords);
                        }

                        ArmTimerLocked();

                        break;
                    }

                    messages.swap(m_heldMessages);
                    words.swap(m_heldWords);
                }

                for (auto const& held : messages)
                {
                    if (held.Destination != nullptr)
                    {
                        // A client that has gone away is not a reason to stop releasing to the rest.
                        held.Destination->Callback(
                            held.OptionFlags,
                            words.data() + held.FirstWord,
                            held.WordCount * sizeof(uint32_t),
                            held.Position,
                            held.Context);
                    }
                }

                messages.clear();
                words.clear();
            }

            if (notALoop != 0)
            {
                RaiseEvent(MidiFeedbackEvent::NotALoop, info);
            }
        }

        void ArmTimerLocked() noexcept
        {
            auto const deadlineUs = m_detector.NextDeadlineMicroseconds();

            if (deadlineUs == m_timerDeadlineUs)
            {
                return;
            }

            m_timerDeadlineUs = deadlineUs;

            if (deadlineUs == 0)
            {
                // A callback already queued still runs, and finds nothing due.
                ::SetThreadpoolTimer(m_timer, nullptr, 0, 0);
                return;
            }

            auto const nowUs = NowMicroseconds();
            auto const delayUs = deadlineUs > nowUs ? deadlineUs - nowUs : 0;

            // negative means relative, in 100 nanosecond units
            ULARGE_INTEGER due{};
            due.QuadPart = static_cast<ULONGLONG>(-static_cast<LONGLONG>(delayUs * 10));

            FILETIME dueTime{};
            dueTime.dwLowDateTime = due.LowPart;
            dueTime.dwHighDateTime = due.HighPart;

            ::SetThreadpoolTimer(m_timer, &dueTime, 0, 0);
        }

        void RaiseEvent(_In_ MidiFeedbackEvent const feedbackEvent, _In_ MidiFeedbackTripInfo const& info) noexcept
        {
            if (!m_handler)
            {
                return;
            }

            try
            {
                m_handler(feedbackEvent, info);
            }
            catch (...)
            {
                // An exception escaping here would end the MIDI service for the whole machine.
            }
        }

        mutable wil::srwlock m_lock{};

        MidiFeedbackDetector m_detector{};

        std::vector<HeldMessage> m_heldMessages{};
        std::vector<uint32_t> m_heldWords{};
        bool m_releasing{ false };

        FILETIME m_trippedTime{};

        EventHandler m_handler{};

        uint64_t m_timerDeadlineUs{ 0 };
        PTP_TIMER m_timer{ nullptr };
    };

    // Runs work on a threadpool thread with no object anyone has to wait on, which is what a trip
    // needs: the device that tripped may be removed, on any thread, while the work is running.
    // The module holding the code stays loaded until the work returns.
    class MidiFeedbackDetachedWork
    {
    public:
        static bool Submit(_In_ std::function<void()> work) noexcept
        {
            std::unique_ptr<Context> context{ new (std::nothrow) Context{} };

            if (context == nullptr)
            {
                return false;
            }

            try
            {
                context->Work = std::move(work);
            }
            catch (...)
            {
                return false;
            }

            if (!::GetModuleHandleExW(
                GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS,
                reinterpret_cast<LPCWSTR>(&MidiFeedbackDetachedWork::Callback),
                &context->Module))
            {
                context->Module = nullptr;
            }

            if (!::TrySubmitThreadpoolCallback(&MidiFeedbackDetachedWork::Callback, context.get(), nullptr))
            {
                if (context->Module != nullptr)
                {
                    ::FreeLibrary(context->Module);
                }

                return false;
            }

            context.release();

            return true;
        }

    private:
        struct Context
        {
            std::function<void()> Work{};
            HMODULE Module{ nullptr };
        };

        static VOID CALLBACK Callback(_Inout_ PTP_CALLBACK_INSTANCE instance, _Inout_opt_ PVOID parameter) noexcept
        {
            std::unique_ptr<Context> context{ static_cast<Context*>(parameter) };

            if (context == nullptr)
            {
                return;
            }

            try
            {
                if (context->Work)
                {
                    context->Work();
                }
            }
            catch (...)
            {
            }

            auto const module = context->Module;

            // the work and everything it captured goes before the module can
            context.reset();

            if (module != nullptr)
            {
                ::FreeLibraryWhenCallbackReturns(instance, module);
            }
        }
    };
}
