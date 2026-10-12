// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#include "pch.h"
#include "ControllerHub.h"

#include <xinput.h>
#include <midi_timestamp.h>

namespace internal = ::WindowsMidiServicesInternal;

namespace midipatchbay
{
    namespace
    {
        // A connected controller is read every 4 ms, in units of 100 ns.
        constexpr int64_t ReadIntervalHundredNanoseconds = 40000;
        constexpr DWORD ReadIntervalMilliseconds = 4;

        // Reading an empty slot costs more than reading a controller, so one is looked at again
        // only this often.
        constexpr uint64_t EmptySlotMilliseconds = 2000;

        using XInputGetStateFunction = DWORD(WINAPI*)(DWORD, XINPUT_STATE*);

        // Never the copy windowsapp.lib links to, XINPUTUAP.dll, which only reads for the app in
        // front.
        XInputGetStateFunction XInputGetStateFromSystem() noexcept
        {
            static XInputGetStateFunction const function = []() noexcept -> XInputGetStateFunction
                {
                    auto const module = ::LoadLibraryExW(L"xinput1_4.dll", nullptr, LOAD_LIBRARY_SEARCH_SYSTEM32);

                    return module == nullptr
                        ? nullptr
                        : reinterpret_cast<XInputGetStateFunction>(::GetProcAddress(module, "XInputGetState"));
                }();

            return function;
        }

        ControllerReading ReadPlayer(_In_opt_ XInputGetStateFunction getState, _In_ uint8_t player) noexcept
        {
            XINPUT_STATE state{};

            if (getState == nullptr || getState(player, &state) != ERROR_SUCCESS)
            {
                return ControllerReading{};
            }

            auto const& pad = state.Gamepad;

            return ControllerReadingFromXInput(pad.wButtons, pad.bLeftTrigger, pad.bRightTrigger,
                pad.sThumbLX, pad.sThumbLY, pad.sThumbRX, pad.sThumbRY);
        }
    }

    struct ControllerHub::Subscription
    {
        uint64_t Token{ 0 };
        int32_t Player{ AnyPlayer };
        Listener Callback{};

        // Held while the listener runs, so StopListening can wait it out.
        std::mutex CallbackLock{};
        bool Active{ true };
    };

    ControllerHub::ControllerHub() noexcept
    {
        // Without them Listen says no, which leaves every controller step quiet rather than
        // taking the app down.
        (void)m_wake.create(wil::EventOptions::None);
        (void)m_stop.create(wil::EventOptions::ManualReset);
    }

    ControllerHub& ControllerHub::Current()
    {
        static ControllerHub hub{};
        return hub;
    }

    ControllerHub::~ControllerHub()
    {
        if (m_stop)
        {
            m_stop.SetEvent();
        }

        if (m_thread.joinable())
        {
            m_thread.join();
        }
    }

    bool ControllerHub::IsAvailable() noexcept
    {
        return XInputGetStateFromSystem() != nullptr;
    }

    _Use_decl_annotations_
    uint64_t ControllerHub::Listen(int32_t player, Listener listener) noexcept
    {
        try
        {
            if (!m_wake || !m_stop || listener == nullptr || XInputGetStateFromSystem() == nullptr)
            {
                return 0;
            }

            auto subscription = std::make_shared<Subscription>();
            subscription->Player = player;
            subscription->Callback = std::move(listener);

            std::lock_guard lock{ m_lock };

            if (!m_thread.joinable())
            {
                m_thread = std::thread([this]() { ThreadWorker(); });
            }

            subscription->Token = m_nextToken++;
            m_subscriptions.push_back(subscription);

            m_wake.SetEvent();

            return subscription->Token;
        }
        MIDI_PATCHBAY_CATCH_AND_LOG(L"Unable to listen to game controllers.")

        return 0;
    }

    _Use_decl_annotations_
    void ControllerHub::StopListening(uint64_t token) noexcept
    {
        if (token == 0)
        {
            return;
        }

        std::shared_ptr<Subscription> found{};

        {
            std::lock_guard lock{ m_lock };

            auto const it = std::find_if(m_subscriptions.begin(), m_subscriptions.end(),
                [token](std::shared_ptr<Subscription> const& subscription) { return subscription->Token == token; });

            if (it != m_subscriptions.end())
            {
                found = *it;
                m_subscriptions.erase(it);
            }
        }

        if (found != nullptr)
        {
            std::lock_guard lock{ found->CallbackLock };
            found->Active = false;
        }
    }

    _Use_decl_annotations_
    ControllerReading ControllerHub::Read(uint8_t player) noexcept
    {
        return ReadPlayer(XInputGetStateFromSystem(), player);
    }

    void ControllerHub::ThreadWorker() noexcept
    {
        try
        {
            ::SetThreadDescription(::GetCurrentThread(), L"Patchbay game controllers");
            ::SetThreadPriority(::GetCurrentThread(), THREAD_PRIORITY_ABOVE_NORMAL);

            wil::unique_handle timer{ ::CreateWaitableTimerExW(nullptr, nullptr,
                CREATE_WAITABLE_TIMER_HIGH_RESOLUTION, TIMER_ALL_ACCESS) };

            auto const getState = XInputGetStateFromSystem();
            auto const frequency = internal::GetMidiTimestampFrequency();

            std::array<bool, ControllerPlayerCount> connected{};
            std::array<bool, ControllerPlayerCount> looked{};
            std::array<uint64_t, ControllerPlayerCount> lastLooked{};
            std::vector<std::shared_ptr<Subscription>> listening{};

            while (true)
            {
                {
                    std::lock_guard lock{ m_lock };
                    listening = m_subscriptions;
                }

                if (listening.empty())
                {
                    HANDLE idle[] = { m_stop.get(), m_wake.get() };

                    if (::WaitForMultipleObjects(2, idle, FALSE, INFINITE) != WAIT_OBJECT_0 + 1)
                    {
                        return;
                    }

                    // Whoever listens next sees every controller fresh.
                    looked.fill(false);
                    continue;
                }

                auto const timestamp = internal::GetCurrentMidiTimestamp();
                auto const now = frequency == 0 ? 0 : internal::ConvertTimestampToWholeMilliseconds(timestamp, frequency);

                for (uint8_t player = 0; player < ControllerPlayerCount; player++)
                {
                    auto const wanted = std::any_of(listening.begin(), listening.end(),
                        [player](std::shared_ptr<Subscription> const& subscription)
                        {
                            return subscription->Player == AnyPlayer || subscription->Player == player;
                        });

                    if (!wanted)
                    {
                        continue;
                    }

                    ControllerReading reading{};

                    if (connected[player] || !looked[player] || now - lastLooked[player] >= EmptySlotMilliseconds)
                    {
                        reading = ReadPlayer(getState, player);
                        connected[player] = reading.Connected;
                        looked[player] = true;
                        lastLooked[player] = now;
                    }

                    for (auto const& subscription : listening)
                    {
                        if (subscription->Player != AnyPlayer && subscription->Player != player)
                        {
                            continue;
                        }

                        std::lock_guard lock{ subscription->CallbackLock };

                        if (subscription->Active)
                        {
                            // One listener's trouble must not stop the others hearing anything.
                            try
                            {
                                subscription->Callback(player, now, timestamp, reading);
                            }
                            catch (...)
                            {
                                MIDI_PATCHBAY_LOG_GENERAL_EXCEPTION(L"A game controller listener failed.");
                            }
                        }
                    }
                }

                listening.clear();

                if (timer)
                {
                    LARGE_INTEGER due{};
                    due.QuadPart = -ReadIntervalHundredNanoseconds;

                    ::SetWaitableTimer(timer.get(), &due, 0, nullptr, nullptr, FALSE);

                    HANDLE waits[] = { m_stop.get(), timer.get() };

                    if (::WaitForMultipleObjects(2, waits, FALSE, INFINITE) != WAIT_OBJECT_0 + 1)
                    {
                        return;
                    }
                }
                else if (::WaitForSingleObject(m_stop.get(), ReadIntervalMilliseconds) != WAIT_TIMEOUT)
                {
                    return;
                }
            }
        }
        catch (...)
        {
            MIDI_PATCHBAY_LOG_GENERAL_EXCEPTION(L"Game controllers stopped being read.");
        }
    }
}
