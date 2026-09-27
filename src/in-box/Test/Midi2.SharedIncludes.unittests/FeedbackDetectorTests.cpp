// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

#include "pch.h"

#include "FeedbackDetectorTests.h"

#include <atomic>
#include <condition_variable>
#include <deque>
#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <thread>
#include <vector>

using namespace WindowsMidiServicesInternal;

namespace
{
    constexpr uint64_t OneSecondUs{ 1'000'000 };

    struct SimMessage
    {
        uint32_t Words[16]{};
        uint32_t WordCount{ 0 };
        uint64_t Sequence{ 0 };
        bool FromLoop{ false };
    };

    SimMessage MakeMessage(_In_ std::initializer_list<uint32_t> words)
    {
        SimMessage message{};

        for (auto const word : words)
        {
            if (message.WordCount < ARRAYSIZE(message.Words))
            {
                message.Words[message.WordCount++] = word;
            }
        }

        return message;
    }

    using Generator = std::function<bool(uint64_t& atUs, SimMessage& message)>;

    // One message every intervalNs from startUs until endUs, numbered in order.
    Generator Periodic(
        _In_ uint64_t const startUs,
        _In_ uint64_t const endUs,
        _In_ uint64_t const intervalNs,
        _In_ std::function<SimMessage(uint64_t)> make)
    {
        auto index = std::make_shared<uint64_t>(0);

        return [=](uint64_t& atUs, SimMessage& message) -> bool
            {
                auto const atNs = startUs * 1'000 + (*index) * intervalNs;

                if (atNs >= endUs * 1'000)
                {
                    return false;
                }

                atUs = atNs / 1'000;
                message = make(*index);
                message.Sequence = *index;
                message.FromLoop = false;

                (*index)++;

                return true;
            };
    }

    Generator Once(_In_ uint64_t const atUs, _In_ SimMessage const message)
    {
        return Periodic(atUs, atUs + 1, 1'000, [message](uint64_t) { return message; });
    }

    // Plays the part of the loopback that owns the detector, and of whatever sits on the far side
    // of it. A loop is modeled as every delivered message coming back after a lap.
    class Simulator
    {
    public:
        uint32_t FanOut{ 0 };
        uint64_t LapUs{ 100 };

        // A real loop cannot outrun the pipes it runs through. Zero means no limit.
        uint64_t MaximumLoopRatePerSecond{ 0 };
        size_t MaximumLoopInFlight{ 2'048 };

        size_t HoldCapacity{ MidiFeedbackGuard::MaximumHeldMessages };

        Generator External{};
        std::function<void(Simulator&)> OnStep{};

        MidiFeedbackDetector Detector{};
        uint64_t NowUs{ 0 };

        uint64_t ExternalSent{ 0 };
        uint64_t ExternalDelivered{ 0 };
        uint64_t LoopDelivered{ 0 };
        uint64_t Dropped{ 0 };
        uint64_t Discarded{ 0 };
        uint32_t Trips{ 0 };
        uint64_t TripAtUs{ 0 };
        std::vector<uint64_t> PauseStartsUs{};
        MidiFeedbackTest LastPauseTest{ MidiFeedbackTest::None };
        bool ExternalOrderKept{ true };
        bool StopExternal{ false };

        void Run(_In_ uint64_t const untilUs)
        {
            bool haveExternal{ false };
            uint64_t externalAtUs{ 0 };
            SimMessage external{};

            auto pullExternal = [&]()
                {
                    haveExternal = !StopExternal && External && External(externalAtUs, external);
                };

            pullExternal();

            for (;;)
            {
                if (StopExternal)
                {
                    haveExternal = false;
                }

                auto const loopAtUs = m_loop.empty() ? UINT64_MAX : m_loop.begin()->first;
                auto const nextExternalUs = haveExternal ? externalAtUs : UINT64_MAX;
                auto const deadlineUs = Detector.NextDeadlineMicroseconds() == 0 ? UINT64_MAX : Detector.NextDeadlineMicroseconds();

                auto next = loopAtUs < nextExternalUs ? loopAtUs : nextExternalUs;
                next = deadlineUs < next ? deadlineUs : next;

                if (next == UINT64_MAX || next > untilUs)
                {
                    NowUs = untilUs;
                    break;
                }

                NowUs = next > NowUs ? next : NowUs;

                auto const phaseBefore = Detector.Phase();

                if (deadlineUs <= loopAtUs && deadlineUs <= nextExternalUs)
                {
                    HandleTimerAction(Detector.OnTimer(NowUs));
                }
                else if (loopAtUs <= nextExternalUs)
                {
                    auto node = m_loop.extract(m_loop.begin());
                    m_loopInFlight--;

                    Send(node.mapped());
                }
                else
                {
                    auto const message = external;
                    ExternalSent++;

                    pullExternal();

                    Send(message);
                }

                if (phaseBefore != MidiFeedbackPhase::FirstPause && Detector.Phase() == MidiFeedbackPhase::FirstPause)
                {
                    PauseStartsUs.push_back(NowUs);
                    LastPauseTest = Detector.ActiveTest();
                }

                if (OnStep)
                {
                    OnStep(*this);
                }
            }
        }

        void ReleaseHeld()
        {
            // the owner delivers everything it held, in order, before anything new
            while (!m_held.empty())
            {
                auto message = m_held.front();
                m_held.pop_front();

                Deliver(message);
            }

            Detector.OnReleaseComplete(NowUs);
        }

        size_t HeldCount() const { return m_held.size(); }

    private:
        void HandleTimerAction(_In_ MidiFeedbackTimerAction const action)
        {
            if (action == MidiFeedbackTimerAction::Release)
            {
                ReleaseHeld();
            }
            else if (action == MidiFeedbackTimerAction::Trip)
            {
                Trips++;
                TripAtUs = NowUs;
                Discarded += m_held.size();
                m_held.clear();
            }
        }

        void Send(_In_ SimMessage const& message)
        {
            switch (Detector.OnSend(message.Words, message.WordCount, NowUs))
            {
            case MidiFeedbackSendAction::Deliver:
                Deliver(message);
                break;

            case MidiFeedbackSendAction::Hold:
                if (m_held.size() >= HoldCapacity)
                {
                    if (Detector.OnHoldQueueFull())
                    {
                        m_held.push_back(message);
                        ReleaseHeld();
                    }
                    else
                    {
                        Deliver(message);
                    }
                }
                else
                {
                    m_held.push_back(message);
                }
                break;

            case MidiFeedbackSendAction::Drop:
                Dropped++;
                break;
            }
        }

        void Deliver(_In_ SimMessage const& message)
        {
            if (message.FromLoop)
            {
                LoopDelivered++;
            }
            else
            {
                if (ExternalDelivered > 0 && message.Sequence <= m_lastExternalSequence)
                {
                    ExternalOrderKept = false;
                }

                m_lastExternalSequence = message.Sequence;
                ExternalDelivered++;
            }

            for (uint32_t copy = 0; copy < FanOut; copy++)
            {
                // a full pipe drops what it cannot take, as a stalled client buffer does
                if (m_loopInFlight >= MaximumLoopInFlight)
                {
                    break;
                }

                auto atNs = (NowUs + LapUs) * 1'000;

                if (MaximumLoopRatePerSecond != 0)
                {
                    auto const spacingNs = 1'000'000'000 / MaximumLoopRatePerSecond;

                    if (atNs < m_lastEchoNs + spacingNs)
                    {
                        atNs = m_lastEchoNs + spacingNs;
                    }

                    m_lastEchoNs = atNs;
                }

                auto echo = message;
                echo.FromLoop = true;

                m_loop.emplace(atNs / 1'000, echo);
                m_loopInFlight++;
            }
        }

        std::multimap<uint64_t, SimMessage> m_loop{};
        size_t m_loopInFlight{ 0 };
        uint64_t m_lastEchoNs{ 0 };

        std::deque<SimMessage> m_held{};
        uint64_t m_lastExternalSequence{ 0 };
    };

    SimMessage BenchmarkNote(_In_ uint64_t const index)
    {
        return MakeMessage({ static_cast<uint32_t>(0x20901500 + (index & 0x7F)) });
    }

    // Records what arrives, in the order it arrives, from whichever thread delivers it.
    class RecordingDestination : public IMidiCallback
    {
    public:
        STDMETHOD(QueryInterface)(REFIID riid, void** object) override
        {
            if (object == nullptr)
            {
                return E_POINTER;
            }

            if (riid == __uuidof(IUnknown) || riid == __uuidof(IMidiCallback))
            {
                *object = static_cast<IMidiCallback*>(this);
                AddRef();
                return S_OK;
            }

            *object = nullptr;
            return E_NOINTERFACE;
        }

        // lives on the stack of the test, so the count is only for form
        STDMETHOD_(ULONG, AddRef)() override { return ++m_references; }
        STDMETHOD_(ULONG, Release)() override { return --m_references; }

        STDMETHOD(Callback)(
            _In_ MessageOptionFlags,
            _In_ PVOID message,
            _In_ UINT size,
            _In_ LONGLONG position,
            _In_ LONGLONG) override
        {
            std::vector<uint32_t> words{};

            if (message != nullptr && size >= sizeof(uint32_t))
            {
                auto const first = static_cast<uint32_t const*>(message);
                words.assign(first, first + size / sizeof(uint32_t));
            }

            {
                std::lock_guard<std::mutex> lock{ m_mutex };

                m_positions.push_back(position);

                if (Echo)
                {
                    m_pending.push_back(std::move(words));
                }
            }

            if (Echo)
            {
                m_pendingChanged.notify_one();
            }

            return S_OK;
        }

        size_t DeliveredCount()
        {
            std::lock_guard<std::mutex> lock{ m_mutex };
            return m_positions.size();
        }

        std::vector<LONGLONG> Positions()
        {
            std::lock_guard<std::mutex> lock{ m_mutex };
            return m_positions;
        }

        bool WaitForEcho(_Out_ std::vector<uint32_t>& words, _In_ std::chrono::milliseconds const timeout)
        {
            std::unique_lock<std::mutex> lock{ m_mutex };

            if (!m_pendingChanged.wait_for(lock, timeout, [this]() { return !m_pending.empty(); }))
            {
                return false;
            }

            words = std::move(m_pending.front());
            m_pending.pop_front();

            return true;
        }

        // When set, everything delivered is queued so a pump can send it straight back in.
        bool Echo{ false };

    private:
        std::atomic<ULONG> m_references{ 1 };

        std::mutex m_mutex{};
        std::condition_variable m_pendingChanged{};
        std::vector<LONGLONG> m_positions{};
        std::deque<std::vector<uint32_t>> m_pending{};
    };

    bool WaitUntil(_In_ std::function<bool()> const& condition, _In_ std::chrono::milliseconds const timeout)
    {
        auto const end = std::chrono::steady_clock::now() + timeout;

        while (std::chrono::steady_clock::now() < end)
        {
            if (condition())
            {
                return true;
            }

            std::this_thread::sleep_for(std::chrono::milliseconds(5));
        }

        return condition();
    }
}


void FeedbackDetectorTests::TestFastNoteLoopTrips()
{
    Simulator sim{};
    sim.FanOut = 1;
    sim.LapUs = 100;
    sim.External = Once(0, MakeMessage({ 0x20904060 }));

    sim.Run(5 * OneSecondUs);

    VERIFY_ARE_EQUAL(1u, sim.Trips);
    VERIFY_IS_LESS_THAN(sim.TripAtUs, 2 * OneSecondUs);
    VERIFY_IS_TRUE(sim.Detector.TripInfo().Test == MidiFeedbackTest::Repeat);
    VERIFY_IS_TRUE(sim.Detector.Phase() == MidiFeedbackPhase::Tripped);

    // the circle is broken: nothing is left going around
    auto const deliveredAtTrip = sim.LoopDelivered;
    sim.Run(8 * OneSecondUs);
    VERIFY_ARE_EQUAL(deliveredAtTrip, sim.LoopDelivered);
}

void FeedbackDetectorTests::TestSaturatedStormTrips()
{
    // Each message comes back twice, so the loop fills the pipes within a few laps and stays full.
    Simulator sim{};
    sim.FanOut = 2;
    sim.LapUs = 50;
    sim.MaximumLoopRatePerSecond = 300'000;
    sim.MaximumLoopInFlight = 2'048;
    sim.External = Once(0, MakeMessage({ 0x20904060 }));

    sim.Run(5 * OneSecondUs);

    VERIFY_ARE_EQUAL(1u, sim.Trips);
    VERIFY_IS_LESS_THAN(sim.TripAtUs, 2 * OneSecondUs);
}

void FeedbackDetectorTests::TestClockLoopTrips()
{
    // A clock sent into an app that echoes it: every tick stays in the loop forever. Real time
    // messages have to be held and counted, or this could never be caught.
    Simulator sim{};
    sim.FanOut = 1;
    sim.LapUs = 200;
    sim.External = Periodic(0, 10 * OneSecondUs, 20'833'333, [](uint64_t) { return MakeMessage({ 0x10F80000 }); });

    sim.Run(10 * OneSecondUs);

    VERIFY_ARE_EQUAL(1u, sim.Trips);
    VERIFY_IS_LESS_THAN(sim.TripAtUs, 3 * OneSecondUs);
}

void FeedbackDetectorTests::TestSystemExclusiveLoopTrips()
{
    // start, continue and end packets of one message, going around together
    Simulator sim{};
    sim.FanOut = 1;
    sim.LapUs = 300;
    sim.External = Once(0, MakeMessage({
        0x30160001, 0x02030405,
        0x30260607, 0x08090A0B,
        0x30320C0D, 0x00000000 }));

    sim.Run(5 * OneSecondUs);

    VERIFY_ARE_EQUAL(1u, sim.Trips);
    VERIFY_IS_TRUE(sim.Detector.TripInfo().Test == MidiFeedbackTest::Repeat);
}

void FeedbackDetectorTests::TestSlowDawStyleLoopTrips()
{
    // twenty different notes going around an app with a five millisecond buffer
    Simulator sim{};
    sim.FanOut = 1;
    sim.LapUs = 5'000;
    sim.External = Periodic(0, 5'000, 250'000, [](uint64_t index)
        {
            return MakeMessage({ static_cast<uint32_t>(0x20903064 + (index << 8)) });
        });

    sim.Run(5 * OneSecondUs);

    VERIFY_ARE_EQUAL(1u, sim.Trips);
    VERIFY_IS_LESS_THAN(sim.TripAtUs, 2 * OneSecondUs);
}

void FeedbackDetectorTests::TestQuietTrafficIsNeverHeld()
{
    Simulator sim{};
    sim.External = Periodic(0, 5 * OneSecondUs, 1'000'000, [](uint64_t index)
        {
            return MakeMessage({ static_cast<uint32_t>(0x20900064 + ((index % 88) << 8)) });
        });

    sim.Run(6 * OneSecondUs);

    VERIFY_ARE_EQUAL(0u, sim.Trips);
    VERIFY_ARE_EQUAL(size_t{ 0 }, sim.PauseStartsUs.size());
    VERIFY_ARE_EQUAL(sim.ExternalSent, sim.ExternalDelivered);
}

void FeedbackDetectorTests::TestBenchmarkPatternIsLossless()
{
    // the pattern the loopback benchmarks send: 128 notes over and over, as fast as possible
    Simulator sim{};
    sim.External = Periodic(0, 1'500'000, 5'000, BenchmarkNote);

    sim.Run(3 * OneSecondUs);

    VERIFY_ARE_EQUAL(0u, sim.Trips);
    VERIFY_IS_GREATER_THAN_OR_EQUAL(sim.PauseStartsUs.size(), size_t{ 1 });
    VERIFY_ARE_EQUAL(uint64_t{ 300'000 }, sim.ExternalSent);
    VERIFY_ARE_EQUAL(sim.ExternalSent, sim.ExternalDelivered);
    VERIFY_IS_TRUE(sim.ExternalOrderKept);
    VERIFY_IS_GREATER_THAN_OR_EQUAL(sim.Detector.NotALoopCount(), 1u);
}

void FeedbackDetectorTests::TestBatchedBenchmarkPatternIsLossless()
{
    // the batched benchmark sends the same ten notes in every buffer
    Simulator sim{};
    sim.External = Periodic(0, 1'500'000, 50'000, [](uint64_t)
        {
            return MakeMessage({
                0x20901500, 0x20901501, 0x20901502, 0x20901503, 0x20901504,
                0x20901505, 0x20901506, 0x20901507, 0x20901508, 0x20901509 });
        });

    sim.Run(3 * OneSecondUs);

    VERIFY_ARE_EQUAL(0u, sim.Trips);
    VERIFY_IS_GREATER_THAN_OR_EQUAL(sim.PauseStartsUs.size(), size_t{ 1 });
    VERIFY_ARE_EQUAL(sim.ExternalSent, sim.ExternalDelivered);
    VERIFY_IS_TRUE(sim.ExternalOrderKept);
}

void FeedbackDetectorTests::TestRepeatingAutomationIsLossless()
{
    // 64 controllers held at the same value and sent again every 10 ms
    Simulator sim{};
    sim.External = Periodic(0, 5 * OneSecondUs, 156'250, [](uint64_t index)
        {
            return MakeMessage({ static_cast<uint32_t>(0x20B00064 + ((index % 64) << 8)) });
        });

    sim.Run(6 * OneSecondUs);

    VERIFY_ARE_EQUAL(0u, sim.Trips);
    VERIFY_IS_GREATER_THAN_OR_EQUAL(sim.PauseStartsUs.size(), size_t{ 1 });
    VERIFY_ARE_EQUAL(sim.ExternalSent, sim.ExternalDelivered);
    VERIFY_IS_TRUE(sim.ExternalOrderKept);
}

void FeedbackDetectorTests::TestDistinctSystemExclusiveDumpIsLossless()
{
    // a large dump with different data in every message: too fast to be ordinary, and never repeating
    Simulator sim{};
    sim.External = Periodic(0, 2 * OneSecondUs, 50'000, [](uint64_t index)
        {
            auto const low = static_cast<uint32_t>(index & 0x7F7F);
            return MakeMessage({ 0x30160000 | low, 0x01020304, 0x30320000 | low, 0x00000000 });
        });

    sim.Run(3 * OneSecondUs);

    VERIFY_ARE_EQUAL(0u, sim.Trips);
    VERIFY_IS_GREATER_THAN_OR_EQUAL(sim.PauseStartsUs.size(), size_t{ 1 });
    VERIFY_IS_TRUE(sim.LastPauseTest == MidiFeedbackTest::Runaway);
    VERIFY_ARE_EQUAL(sim.ExternalSent, sim.ExternalDelivered);
    VERIFY_IS_TRUE(sim.ExternalOrderKept);
}

void FeedbackDetectorTests::TestSenderThatStopsDuringPauseDoesNotTrip()
{
    // A sender that happens to go quiet the moment the first pause starts looks exactly like a
    // collapsing loop. It must not start again by itself when released, so the second check clears it.
    Simulator sim{};
    sim.External = Periodic(0, 10 * OneSecondUs, 100'000, [](uint64_t) { return MakeMessage({ 0x20904060 }); });
    sim.OnStep = [](Simulator& s)
        {
            if (s.Detector.Phase() == MidiFeedbackPhase::FirstPause)
            {
                s.StopExternal = true;
            }
        };

    sim.Run(3 * OneSecondUs);

    VERIFY_ARE_EQUAL(0u, sim.Trips);
    VERIFY_ARE_EQUAL(size_t{ 1 }, sim.PauseStartsUs.size());
    VERIFY_ARE_EQUAL(1u, sim.Detector.NotALoopCount());
    VERIFY_ARE_EQUAL(sim.ExternalSent, sim.ExternalDelivered);
}

void FeedbackDetectorTests::TestFullHoldQueueMeansNotALoop()
{
    Simulator sim{};
    sim.HoldCapacity = 100;
    sim.External = Periodic(0, 1'500'000, 5'000, BenchmarkNote);

    sim.Run(3 * OneSecondUs);

    VERIFY_ARE_EQUAL(0u, sim.Trips);
    VERIFY_ARE_EQUAL(size_t{ 1 }, sim.PauseStartsUs.size());
    VERIFY_ARE_EQUAL(1u, sim.Detector.NotALoopCount());
    VERIFY_ARE_EQUAL(sim.ExternalSent, sim.ExternalDelivered);
    VERIFY_IS_TRUE(sim.ExternalOrderKept);
}

void FeedbackDetectorTests::TestBackoffDoublesAfterEachFalseAlarm()
{
    // eight identical notes in every buffer, 2,500 a second, for longer than three backoffs
    Simulator sim{};
    sim.External = Periodic(0, 400 * OneSecondUs, 3'200'000, [](uint64_t)
        {
            return MakeMessage({
                0x20904060, 0x20904060, 0x20904060, 0x20904060,
                0x20904060, 0x20904060, 0x20904060, 0x20904060 });
        });

    sim.Run(400 * OneSecondUs);

    VERIFY_ARE_EQUAL(0u, sim.Trips);
    VERIFY_ARE_EQUAL(size_t{ 3 }, sim.PauseStartsUs.size());

    auto const firstGap = sim.PauseStartsUs[1] - sim.PauseStartsUs[0];
    auto const secondGap = sim.PauseStartsUs[2] - sim.PauseStartsUs[1];

    VERIFY_IS_GREATER_THAN_OR_EQUAL(firstGap, 60 * OneSecondUs);
    VERIFY_IS_LESS_THAN(firstGap, 62 * OneSecondUs);
    VERIFY_IS_GREATER_THAN_OR_EQUAL(secondGap, 120 * OneSecondUs);
    VERIFY_IS_LESS_THAN(secondGap, 122 * OneSecondUs);

    VERIFY_ARE_EQUAL(sim.ExternalSent, sim.ExternalDelivered);
}

void FeedbackDetectorTests::TestResetAfterTripDeliversAgain()
{
    Simulator sim{};
    sim.FanOut = 1;
    sim.LapUs = 100;
    sim.External = Once(0, MakeMessage({ 0x20904060 }));

    sim.Run(5 * OneSecondUs);

    VERIFY_ARE_EQUAL(1u, sim.Trips);

    uint32_t const note{ 0x20904060 };
    VERIFY_IS_TRUE(sim.Detector.OnSend(&note, 1, sim.NowUs) == MidiFeedbackSendAction::Drop);

    sim.Detector.Reset();

    VERIFY_IS_TRUE(sim.Detector.Phase() == MidiFeedbackPhase::Watching);
    VERIFY_IS_TRUE(sim.Detector.OnSend(&note, 1, sim.NowUs + 1) == MidiFeedbackSendAction::Deliver);
    VERIFY_ARE_EQUAL(uint32_t{ 0 }, sim.Detector.TripInfo().MessagesPerSecond);
}

void FeedbackDetectorTests::TestEndTestReleasesWithoutBackoff()
{
    // Turning protection off mid-test releases what was held. Turning it back on must not have to
    // wait out a backoff that was never earned.
    Simulator sim{};
    sim.External = Periodic(0, 3 * OneSecondUs, 5'000, BenchmarkNote);

    bool ended{ false };

    sim.OnStep = [&ended](Simulator& s)
        {
            if (!ended && s.Detector.Phase() == MidiFeedbackPhase::FirstPause)
            {
                ended = true;

                VERIFY_IS_TRUE(s.Detector.EndTestAndRelease());
                s.ReleaseHeld();

                VERIFY_IS_TRUE(s.Detector.Phase() == MidiFeedbackPhase::Watching);
            }
        };

    sim.Run(3 * OneSecondUs);

    VERIFY_IS_TRUE(ended);
    VERIFY_IS_GREATER_THAN_OR_EQUAL(sim.PauseStartsUs.size(), size_t{ 2 });
    VERIFY_IS_LESS_THAN(sim.PauseStartsUs[1] - sim.PauseStartsUs[0], 2 * OneSecondUs);
    VERIFY_ARE_EQUAL(sim.ExternalSent, sim.ExternalDelivered);
    VERIFY_IS_TRUE(sim.ExternalOrderKept);
}

void FeedbackDetectorTests::TestSlowLoopIsBelowDetection()
{
    // One message going around every 10 ms is a hundred a second. That is audible but harmless to
    // the PC, and far too close to ordinary traffic to call. This documents the limit.
    Simulator sim{};
    sim.FanOut = 1;
    sim.LapUs = 10'000;
    sim.External = Once(0, MakeMessage({ 0x20904060 }));

    sim.Run(10 * OneSecondUs);

    VERIFY_ARE_EQUAL(0u, sim.Trips);
    VERIFY_ARE_EQUAL(size_t{ 0 }, sim.PauseStartsUs.size());
}

void FeedbackDetectorTests::TestMalformedBufferIsSafe()
{
    MidiFeedbackDetector detector{};

    VERIFY_IS_TRUE(detector.OnSend(nullptr, 0, 1) == MidiFeedbackSendAction::Deliver);
    VERIFY_IS_TRUE(detector.OnSend(nullptr, 4, 2) == MidiFeedbackSendAction::Deliver);

    // a 64 bit message with its second word missing, and a 128 bit one cut short
    uint32_t const truncated64{ 0x40904060 };
    VERIFY_IS_TRUE(detector.OnSend(&truncated64, 1, 3) == MidiFeedbackSendAction::Deliver);

    uint32_t const truncated128[]{ 0xF0000000, 0x00000000 };
    VERIFY_IS_TRUE(detector.OnSend(truncated128, ARRAYSIZE(truncated128), 4) == MidiFeedbackSendAction::Deliver);

    // system exclusive continue and end packets with no start
    uint32_t const orphans[]{ 0x30260607, 0x08090A0B, 0x30320C0D, 0x00000000 };
    VERIFY_IS_TRUE(detector.OnSend(orphans, ARRAYSIZE(orphans), 5) == MidiFeedbackSendAction::Deliver);

    VERIFY_IS_TRUE(detector.OnTimer(6) == MidiFeedbackTimerAction::None);
    VERIFY_IS_FALSE(detector.OnHoldQueueFull());
    VERIFY_IS_FALSE(detector.EndTestAndRelease());
}

void FeedbackDetectorTests::TestGuardKeepsEveryMessageInOrder()
{
    RecordingDestination destination{};

    std::atomic<uint32_t> notALoop{ 0 };
    std::atomic<uint32_t> trips{ 0 };

    constexpr uint32_t messageCount{ 260'000 };

    {
        MidiFeedbackGuard guard{ [&](MidiFeedbackEvent feedbackEvent, MidiFeedbackTripInfo const&)
            {
                if (feedbackEvent == MidiFeedbackEvent::Tripped)
                {
                    trips++;
                }
                else
                {
                    notALoop++;
                }
            } };

        // paced at 200,000 a second, long enough for a test to start and finish
        auto const startUs = MidiFeedbackGuard::NowMicroseconds();

        uint32_t failedSends{ 0 };

        for (uint32_t index = 0; index < messageCount; index++)
        {
            auto const targetUs = startUs + static_cast<uint64_t>(index) * 5;

            while (MidiFeedbackGuard::NowMicroseconds() < targetUs)
            {
                YieldProcessor();
            }

            uint32_t word{ 0x20901500 + (index & 0x7F) };

            if (FAILED(guard.Send(&destination, MessageOptionFlags_None, &word, sizeof(word), index, 0)))
            {
                failedSends++;
            }
        }

        VERIFY_ARE_EQUAL(0u, failedSends);
        VERIFY_IS_TRUE(WaitUntil([&]() { return destination.DeliveredCount() == messageCount; }, std::chrono::seconds(5)));
    }

    auto const positions = destination.Positions();

    VERIFY_ARE_EQUAL(size_t{ messageCount }, positions.size());

    bool inOrder{ true };

    for (size_t index = 0; index < positions.size(); index++)
    {
        if (positions[index] != static_cast<LONGLONG>(index))
        {
            inOrder = false;
            break;
        }
    }

    VERIFY_IS_TRUE(inOrder);
    VERIFY_ARE_EQUAL(0u, trips.load());
    VERIFY_IS_GREATER_THAN_OR_EQUAL(notALoop.load(), 1u);
}

void FeedbackDetectorTests::TestGuardTripsOnLiveLoop()
{
    RecordingDestination destination{};
    destination.Echo = true;

    std::atomic<uint32_t> trips{ 0 };
    std::atomic<bool> stop{ false };

    {
        MidiFeedbackGuard guard{ [&](MidiFeedbackEvent feedbackEvent, MidiFeedbackTripInfo const&)
            {
                if (feedbackEvent == MidiFeedbackEvent::Tripped)
                {
                    trips++;
                }
            } };

        // plays an app that sends everything it receives straight back
        std::thread pump([&]()
            {
                std::vector<uint32_t> words{};

                while (!stop)
                {
                    if (destination.WaitForEcho(words, std::chrono::milliseconds(10)) && !words.empty())
                    {
                        guard.Send(&destination, MessageOptionFlags_None, words.data(),
                            static_cast<UINT>(words.size() * sizeof(uint32_t)), 0, 0);
                    }
                }
            });

        uint32_t seed{ 0x20904060 };
        VERIFY_SUCCEEDED(guard.Send(&destination, MessageOptionFlags_None, &seed, sizeof(seed), 0, 0));

        auto const tripped = WaitUntil([&]() { return trips.load() > 0; }, std::chrono::seconds(10));

        auto const deliveredAtTrip = destination.DeliveredCount();
        std::this_thread::sleep_for(std::chrono::milliseconds(300));
        auto const deliveredLater = destination.DeliveredCount();

        stop = true;
        pump.join();

        VERIFY_IS_TRUE(tripped);
        VERIFY_ARE_EQUAL(1u, trips.load());
        VERIFY_IS_TRUE(guard.IsTripped());

        // at most the few that were already on their way back when it tripped
        VERIFY_IS_LESS_THAN_OR_EQUAL(deliveredLater, deliveredAtTrip + 2);

        auto const trippedTime = guard.TrippedTime();
        VERIFY_IS_TRUE(trippedTime.dwHighDateTime != 0 || trippedTime.dwLowDateTime != 0);
    }
}

void FeedbackDetectorTests::TestGuardResetDiscardsAndResumes()
{
    RecordingDestination destination{};
    destination.Echo = true;

    std::atomic<uint32_t> trips{ 0 };
    std::atomic<bool> stop{ false };

    MidiFeedbackGuard guard{ [&](MidiFeedbackEvent feedbackEvent, MidiFeedbackTripInfo const&)
        {
            if (feedbackEvent == MidiFeedbackEvent::Tripped)
            {
                trips++;
            }
        } };

    std::thread pump([&]()
        {
            std::vector<uint32_t> words{};

            while (!stop)
            {
                if (destination.WaitForEcho(words, std::chrono::milliseconds(10)) && !words.empty())
                {
                    guard.Send(&destination, MessageOptionFlags_None, words.data(),
                        static_cast<UINT>(words.size() * sizeof(uint32_t)), 0, 0);
                }
            }
        });

    uint32_t seed{ 0x20904060 };
    VERIFY_SUCCEEDED(guard.Send(&destination, MessageOptionFlags_None, &seed, sizeof(seed), 0, 0));

    auto const tripped = WaitUntil([&]() { return trips.load() > 0; }, std::chrono::seconds(10));

    stop = true;
    pump.join();

    VERIFY_IS_TRUE(tripped);
    VERIFY_IS_TRUE(guard.IsTripped());

    guard.Reset(false);

    VERIFY_IS_FALSE(guard.IsTripped());

    auto const trippedTime = guard.TrippedTime();
    VERIFY_IS_TRUE(trippedTime.dwHighDateTime == 0 && trippedTime.dwLowDateTime == 0);

    destination.Echo = false;

    auto const before = destination.DeliveredCount();

    uint32_t note{ 0x20904060 };
    VERIFY_SUCCEEDED(guard.Send(&destination, MessageOptionFlags_None, &note, sizeof(note), 0, 0));

    VERIFY_ARE_EQUAL(before + 1, destination.DeliveredCount());
}
