// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#include "pch.h"
#include "ClockGenerator.h"

#include <algorithm>
#include <chrono>

namespace glass
{
    namespace
    {
        // Long enough that an idle generator costs nothing, short enough that Stop is never
        // left waiting on a thread that is asleep for a minute.
        constexpr uint32_t IdleWaitMilliseconds = 1000;

        // Below this the thread hands the rest of the wait to a spin, because sleeping for two
        // milliseconds routinely overshoots by one and a tick at 300 bpm is only 8.3 ms long.
        constexpr int64_t SpinThresholdMicroseconds = 2000;
    }

    std::shared_ptr<ClockGenerator> ClockGenerator::Create()
    {
        return std::shared_ptr<ClockGenerator>(new ClockGenerator());
    }

    ClockGenerator::~ClockGenerator() noexcept
    {
        Stop();
    }

    uint64_t ClockGenerator::NowMicroseconds() noexcept
    {
        return static_cast<uint64_t>(
            std::chrono::duration_cast<std::chrono::microseconds>(
                std::chrono::steady_clock::now().time_since_epoch()).count());
    }

    _Use_decl_annotations_
    uint64_t ClockGenerator::DueMicrosecondsOf(RunningClock const& clock) const noexcept
    {
        // The tick after the last one that went out, placed against the origin rather than
        // against the previous tick. That is the whole reason this does not drift.
        auto const perTick =
            60.0 * 1000000.0 / (clock.BeatsPerMinute * TicksPerQuarterNote);

        return clock.OriginMicroseconds +
            static_cast<uint64_t>(static_cast<double>(clock.TicksSent) * perTick);
    }

    _Use_decl_annotations_
    void ClockGenerator::Start(winrt::Microsoft::UI::Dispatching::DispatcherQueue const& dispatcher)
    {
        std::lock_guard guard{ m_lock };

        if (m_thread.joinable())
        {
            return;
        }

        m_dispatcher = dispatcher;
        m_stopping = false;

        // A joined thread rather than a detached one: a tick must never fire into a window that
        // has already gone.
        m_thread = std::thread([weak = std::weak_ptr<ClockGenerator>{ shared_from_this() }]()
            {
                if (auto strong = weak.lock())
                {
                    strong->ClockLoop();
                }
            });
    }

    void ClockGenerator::Stop() noexcept
    {
        try
        {
            std::thread worker{};

            {
                std::lock_guard guard{ m_lock };

                if (!m_thread.joinable())
                {
                    return;
                }

                m_clocks.clear();
                m_stopping = true;
                m_signaled = true;

                worker = std::move(m_thread);
            }

            m_wakeup.notify_all();

            if (worker.joinable())
            {
                worker.join();
            }
        }
        catch (...)
        {
        }
    }

    void ClockGenerator::Wake() noexcept
    {
        m_wakeup.notify_all();
    }

    _Use_decl_annotations_
    void ClockGenerator::RaiseOnDispatcher(std::function<void(ClockGenerator&)> const& work)
    {
        if (m_dispatcher == nullptr)
        {
            return;
        }

        // Already the right thread, which it is every time a finger presses the control and
        // every time a window is closing. Queuing from here would put a stop message behind
        // the teardown that is about to throw the connections away.
        if (m_dispatcher.HasThreadAccess())
        {
            work(*this);
            return;
        }

        m_dispatcher.TryEnqueue(
            [weak = std::weak_ptr<ClockGenerator>{ shared_from_this() }, work]()
            {
                if (auto strong = weak.lock())
                {
                    work(*strong);
                }
            });
    }

    _Use_decl_annotations_
    void ClockGenerator::Run(uint32_t controlIndex, double beatsPerMinute, bool sendsTransport)
    {
        try
        {
            {
                std::lock_guard guard{ m_lock };

                m_clocks.erase(
                    std::remove_if(
                        m_clocks.begin(),
                        m_clocks.end(),
                        [controlIndex](RunningClock const& existing)
                        { return existing.ControlIndex == controlIndex; }),
                    m_clocks.end());

                RunningClock clock{};

                clock.ControlIndex = controlIndex;
                clock.BeatsPerMinute = std::clamp(
                    beatsPerMinute, MinimumBeatsPerMinuteForClock, MaximumBeatsPerMinuteForClock);
                clock.SendsTransport = sendsTransport;
                clock.OriginMicroseconds = NowMicroseconds();
                clock.TicksSent = 0;

                m_clocks.push_back(clock);

                m_signaled = true;
            }

            // Start goes out before the first tick, so a drum machine begins on the downbeat
            // rather than wherever the next clock byte happened to land.
            //
            // Raised directly when this is already the dispatcher's thread, which it is every
            // time a finger presses the control. Queuing it would put it behind whatever else
            // is on the queue, and at shutdown behind the teardown itself.
            if (sendsTransport)
            {
                RaiseOnDispatcher([controlIndex](ClockGenerator& self)
                    {
                        if (self.SendRealTime)
                        {
                            self.SendRealTime(controlIndex, StatusStart);
                        }
                    });
            }

            Wake();
        }
        catch (...)
        {
        }
    }

    _Use_decl_annotations_
    void ClockGenerator::CancelFor(uint32_t controlIndex) noexcept
    {
        try
        {
            auto sendStop = false;

            {
                std::lock_guard guard{ m_lock };

                for (auto const& clock : m_clocks)
                {
                    if (clock.ControlIndex == controlIndex)
                    {
                        sendStop = clock.SendsTransport;
                        break;
                    }
                }

                m_clocks.erase(
                    std::remove_if(
                        m_clocks.begin(),
                        m_clocks.end(),
                        [controlIndex](RunningClock const& existing)
                        { return existing.ControlIndex == controlIndex; }),
                    m_clocks.end());

                m_signaled = true;
            }

            RaiseOnDispatcher([controlIndex, sendStop](ClockGenerator& self)
                {
                    if (sendStop && self.SendRealTime)
                    {
                        self.SendRealTime(controlIndex, StatusStop);
                    }

                    if (self.BeatMoved)
                    {
                        self.BeatMoved(controlIndex, 0, 0.0, false);
                    }
                });

            Wake();
        }
        catch (...)
        {
        }
    }

    void ClockGenerator::CancelAll() noexcept
    {
        try
        {
            std::vector<uint32_t> running{};

            {
                std::lock_guard guard{ m_lock };

                for (auto const& clock : m_clocks)
                {
                    running.push_back(clock.ControlIndex);
                }
            }

            for (auto const controlIndex : running)
            {
                CancelFor(controlIndex);
            }
        }
        catch (...)
        {
        }
    }

    _Use_decl_annotations_
    bool ClockGenerator::IsRunning(uint32_t controlIndex) const noexcept
    {
        try
        {
            std::lock_guard guard{ m_lock };

            for (auto const& clock : m_clocks)
            {
                if (clock.ControlIndex == controlIndex)
                {
                    return true;
                }
            }
        }
        catch (...)
        {
        }

        return false;
    }

    _Use_decl_annotations_
    void ClockGenerator::SetTempo(uint32_t controlIndex, double beatsPerMinute) noexcept
    {
        try
        {
            std::lock_guard guard{ m_lock };

            for (auto& clock : m_clocks)
            {
                if (clock.ControlIndex != controlIndex)
                {
                    continue;
                }

                auto const wanted = std::clamp(
                    beatsPerMinute, MinimumBeatsPerMinuteForClock, MaximumBeatsPerMinuteForClock);

                if (std::abs(wanted - clock.BeatsPerMinute) < 0.01)
                {
                    return;
                }

                // Rebase on the tick that has just been sent rather than restarting the bar.
                // Riding the tempo during a set must not throw the downbeat away, and without
                // this the whole history of the run is re-timed at the new rate.
                clock.OriginMicroseconds = DueMicrosecondsOf(clock);
                clock.TicksSent = 0;
                clock.BeatsPerMinute = wanted;

                m_signaled = true;

                Wake();
                return;
            }
        }
        catch (...)
        {
        }
    }

    _Use_decl_annotations_
    double ClockGenerator::TempoOf(uint32_t controlIndex) const noexcept
    {
        try
        {
            std::lock_guard guard{ m_lock };

            for (auto const& clock : m_clocks)
            {
                if (clock.ControlIndex == controlIndex)
                {
                    return clock.BeatsPerMinute;
                }
            }
        }
        catch (...)
        {
        }

        return 0.0;
    }

    size_t ClockGenerator::RunningCount() const noexcept
    {
        try
        {
            std::lock_guard guard{ m_lock };

            return m_clocks.size();
        }
        catch (...)
        {
            return 0;
        }
    }

    void ClockGenerator::ClockLoop()
    {
        for (;;)
        {
            std::vector<std::pair<uint32_t, uint64_t>> due{};

            uint64_t nextDue{ 0 };

            {
                std::unique_lock guard{ m_lock };

                if (m_stopping)
                {
                    return;
                }

                auto const now = NowMicroseconds();

                for (auto& clock : m_clocks)
                {
                    auto const at = DueMicrosecondsOf(clock);

                    if (at <= now)
                    {
                        // A wakeup that arrived late catches up rather than dropping ticks, so
                        // the count stays right even when the machine was busy. More than a
                        // beat behind is thrown away instead: a page fault must not turn into
                        // a burst of twenty four messages at once.
                        auto const behind = now - at;
                        auto const perTick =
                            60.0 * 1000000.0 / (clock.BeatsPerMinute * TicksPerQuarterNote);

                        if (behind > static_cast<uint64_t>(perTick * TicksPerQuarterNote))
                        {
                            clock.OriginMicroseconds = now;
                            clock.TicksSent = 0;
                        }

                        due.emplace_back(clock.ControlIndex, clock.TicksSent);

                        clock.TicksSent++;
                    }

                    auto const after = DueMicrosecondsOf(clock);

                    if (nextDue == 0 || after < nextDue)
                    {
                        nextDue = after;
                    }
                }

                if (due.empty())
                {
                    auto wait = std::chrono::milliseconds{ IdleWaitMilliseconds };

                    if (nextDue != 0)
                    {
                        auto const gap = static_cast<int64_t>(nextDue) - static_cast<int64_t>(now);

                        if (gap <= SpinThresholdMicroseconds)
                        {
                            // Close enough that sleeping would overshoot the tick. Give the
                            // rest of the wait away rather than holding the processor.
                            guard.unlock();

                            while (NowMicroseconds() < nextDue)
                            {
                                std::this_thread::yield();
                            }

                            continue;
                        }

                        wait = std::chrono::milliseconds{
                            std::max<int64_t>(1, (gap - SpinThresholdMicroseconds) / 1000) };
                    }

                    m_wakeup.wait_for(guard, wait, [this]() { return m_signaled || m_stopping; });

                    m_signaled = false;

                    continue;
                }

                m_signaled = false;
            }

            if (m_dispatcher == nullptr)
            {
                continue;
            }

            for (auto const& [controlIndex, tick] : due)
            {
                auto const beatInBar =
                    static_cast<int32_t>((tick / TicksPerQuarterNote) % QuarterNotesPerBar);

                auto const phase =
                    static_cast<double>(tick % TicksPerQuarterNote) / TicksPerQuarterNote;

                m_dispatcher.TryEnqueue(
                    [weak = std::weak_ptr<ClockGenerator>{ shared_from_this() },
                     controlIndex,
                     beatInBar,
                     phase]()
                    {
                        auto strong = weak.lock();

                        if (strong == nullptr)
                        {
                            return;
                        }

                        if (strong->SendRealTime)
                        {
                            strong->SendRealTime(controlIndex, StatusTimingClock);
                        }

                        if (strong->BeatMoved)
                        {
                            strong->BeatMoved(controlIndex, beatInBar, phase, true);
                        }
                    });
            }
        }
    }
}
