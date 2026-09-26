// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#include "pch.h"
#include "LfoGenerator.h"

#include <algorithm>
#include <chrono>
#include <cmath>

namespace glass
{
    namespace
    {
        // Long enough that an idle generator costs nothing, short enough that Stop is never
        // left waiting on a thread that is asleep for a minute.
        constexpr uint32_t IdleWaitMilliseconds = 1000;

        // Below this the thread hands the rest of the wait to a spin, because sleeping for two
        // milliseconds routinely overshoots by one.
        constexpr int64_t SpinThresholdMicroseconds = 2000;
    }

    std::shared_ptr<LfoGenerator> LfoGenerator::Create()
    {
        return std::shared_ptr<LfoGenerator>(new LfoGenerator());
    }

    LfoGenerator::~LfoGenerator() noexcept
    {
        Stop();
    }

    uint64_t LfoGenerator::NowMicroseconds() noexcept
    {
        return static_cast<uint64_t>(
            std::chrono::duration_cast<std::chrono::microseconds>(
                std::chrono::steady_clock::now().time_since_epoch()).count());
    }

    _Use_decl_annotations_
    uint64_t LfoGenerator::IntervalOf(RunningLfo const& lfo) noexcept
    {
        auto const milliseconds = std::clamp(
            lfo.Spec.UpdateIntervalMilliseconds,
            MinimumLfoIntervalMilliseconds,
            MaximumLfoIntervalMilliseconds);

        return static_cast<uint64_t>(milliseconds) * 1000ull;
    }

    _Use_decl_annotations_
    uint64_t LfoGenerator::DueMicrosecondsOf(RunningLfo const& lfo) noexcept
    {
        return lfo.OriginMicroseconds + lfo.SamplesSent * IntervalOf(lfo);
    }

    _Use_decl_annotations_
    double LfoGenerator::PhaseOf(RunningLfo const& lfo, uint64_t sampleIndex) noexcept
    {
        auto const beats = std::clamp(
            lfo.Spec.BeatsPerCycle, MinimumBeatsPerCycle, MaximumBeatsPerCycle);

        auto const bpm = std::clamp(
            lfo.BeatsPerMinute, MinimumBeatsPerMinute, MaximumBeatsPerMinute);

        auto const cycleMicroseconds = 60.0 * 1000000.0 * beats / bpm;

        if (cycleMicroseconds <= 0.0)
        {
            return 0.0;
        }

        auto const elapsed = static_cast<double>(sampleIndex * IntervalOf(lfo));
        auto const turns = lfo.PhaseAtOrigin + elapsed / cycleMicroseconds;

        return turns - std::floor(turns);
    }

    _Use_decl_annotations_
    void LfoGenerator::Start(winrt::Microsoft::UI::Dispatching::DispatcherQueue const& dispatcher)
    {
        std::lock_guard guard{ m_lock };

        if (m_thread.joinable())
        {
            return;
        }

        m_dispatcher = dispatcher;
        m_stopping = false;

        m_thread = std::thread([weak = std::weak_ptr<LfoGenerator>{ shared_from_this() }]()
            {
                if (auto strong = weak.lock())
                {
                    strong->SweepLoop();
                }
            });
    }

    void LfoGenerator::Stop() noexcept
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

                m_sweeps.clear();
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

    void LfoGenerator::Wake() noexcept
    {
        m_wakeup.notify_all();
    }

    _Use_decl_annotations_
    void LfoGenerator::RaiseOnDispatcher(std::function<void(LfoGenerator&)> const& work)
    {
        if (m_dispatcher == nullptr)
        {
            return;
        }

        // Already the right thread every time a finger presses the control and every time a
        // window is closing. Queuing from here would put the last value behind the teardown.
        if (m_dispatcher.HasThreadAccess())
        {
            work(*this);
            return;
        }

        m_dispatcher.TryEnqueue(
            [weak = std::weak_ptr<LfoGenerator>{ shared_from_this() }, work]()
            {
                if (auto strong = weak.lock())
                {
                    work(*strong);
                }
            });
    }

    _Use_decl_annotations_
    void LfoGenerator::Run(uint32_t controlIndex, LfoSpec const& spec, double beatsPerMinute)
    {
        try
        {
            {
                std::lock_guard guard{ m_lock };

                m_sweeps.erase(
                    std::remove_if(
                        m_sweeps.begin(),
                        m_sweeps.end(),
                        [controlIndex](RunningLfo const& existing)
                        { return existing.ControlIndex == controlIndex; }),
                    m_sweeps.end());

                RunningLfo lfo{};

                lfo.ControlIndex = controlIndex;
                lfo.Spec = spec;
                lfo.BeatsPerMinute = std::clamp(
                    beatsPerMinute, MinimumBeatsPerMinute, MaximumBeatsPerMinute);
                lfo.OriginMicroseconds = NowMicroseconds();
                lfo.SamplesSent = 0;
                lfo.PhaseAtOrigin = 0.0;

                m_sweeps.push_back(std::move(lfo));

                m_signaled = true;
            }

            Wake();
        }
        catch (...)
        {
        }
    }

    _Use_decl_annotations_
    void LfoGenerator::CancelFor(uint32_t controlIndex) noexcept
    {
        try
        {
            auto found = false;
            auto restValue = 0.0;

            {
                std::lock_guard guard{ m_lock };

                for (auto const& lfo : m_sweeps)
                {
                    if (lfo.ControlIndex != controlIndex)
                    {
                        continue;
                    }

                    found = true;

                    // Where it parks. A tremolo left at the bottom of its sweep is a muted
                    // channel, so unless the customer asked otherwise it goes back to the
                    // middle of its own range.
                    restValue = lfo.Spec.ReturnsToRestWhenStopped
                        ? std::clamp((lfo.Spec.Lowest + lfo.Spec.Highest) * 0.5, 0.0, 1.0)
                        : LfoValueAt(lfo.Spec, PhaseOf(lfo, lfo.SamplesSent), 0.5);

                    break;
                }

                m_sweeps.erase(
                    std::remove_if(
                        m_sweeps.begin(),
                        m_sweeps.end(),
                        [controlIndex](RunningLfo const& existing)
                        { return existing.ControlIndex == controlIndex; }),
                    m_sweeps.end());

                m_signaled = true;
            }

            if (found)
            {
                RaiseOnDispatcher([controlIndex, restValue](LfoGenerator& self)
                    {
                        if (self.ValueMoved)
                        {
                            self.ValueMoved(controlIndex, restValue, 0.0, false);
                        }
                    });
            }

            Wake();
        }
        catch (...)
        {
        }
    }

    void LfoGenerator::CancelAll() noexcept
    {
        try
        {
            std::vector<uint32_t> running{};

            {
                std::lock_guard guard{ m_lock };

                for (auto const& lfo : m_sweeps)
                {
                    running.push_back(lfo.ControlIndex);
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
    bool LfoGenerator::IsRunning(uint32_t controlIndex) const noexcept
    {
        try
        {
            std::lock_guard guard{ m_lock };

            for (auto const& lfo : m_sweeps)
            {
                if (lfo.ControlIndex == controlIndex)
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
    void LfoGenerator::SetTempo(double beatsPerMinute) noexcept
    {
        try
        {
            auto const wanted = std::clamp(
                beatsPerMinute, MinimumBeatsPerMinute, MaximumBeatsPerMinute);

            std::lock_guard guard{ m_lock };

            for (auto& lfo : m_sweeps)
            {
                if (std::abs(wanted - lfo.BeatsPerMinute) < 0.01)
                {
                    continue;
                }

                // Rebase on the sample that has just gone out, keeping the phase it had reached.
                // Re-timing the whole history at the new rate would jump the wave.
                lfo.PhaseAtOrigin = PhaseOf(lfo, lfo.SamplesSent);
                lfo.OriginMicroseconds = DueMicrosecondsOf(lfo);
                lfo.SamplesSent = 0;
                lfo.BeatsPerMinute = wanted;

                m_signaled = true;
            }

            Wake();
        }
        catch (...)
        {
        }
    }

    size_t LfoGenerator::RunningCount() const noexcept
    {
        try
        {
            std::lock_guard guard{ m_lock };

            return m_sweeps.size();
        }
        catch (...)
        {
            return 0;
        }
    }

    void LfoGenerator::SweepLoop()
    {
        for (;;)
        {
            struct Sample
            {
                uint32_t ControlIndex{ 0 };
                double Value{ 0.0 };
                double Phase{ 0.0 };
            };

            std::vector<Sample> due{};

            uint64_t nextDue{ 0 };

            {
                std::unique_lock guard{ m_lock };

                if (m_stopping)
                {
                    return;
                }

                auto const now = NowMicroseconds();

                for (auto& lfo : m_sweeps)
                {
                    auto const at = DueMicrosecondsOf(lfo);

                    if (at <= now)
                    {
                        // A wakeup that arrived late catches up on the next sample rather than
                        // dragging the phase along behind it. Far enough behind and the sweep
                        // is rebased instead: a page fault must not turn into a burst of forty
                        // messages at once.
                        auto const interval = IntervalOf(lfo);

                        if (now - at > interval * 8)
                        {
                            lfo.PhaseAtOrigin = PhaseOf(lfo, lfo.SamplesSent);
                            lfo.OriginMicroseconds = now;
                            lfo.SamplesSent = 0;
                        }

                        Sample sample{};

                        sample.ControlIndex = lfo.ControlIndex;
                        sample.Phase = PhaseOf(lfo, lfo.SamplesSent);
                        sample.Value = LfoValueAt(
                            lfo.Spec,
                            sample.Phase,
                            LfoWaveRepeats(lfo.Spec.Wave) ? 0.5 : lfo.Noise.Next(lfo.Spec.Wave));

                        due.push_back(sample);

                        lfo.SamplesSent++;
                    }

                    auto const after = DueMicrosecondsOf(lfo);

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

            for (auto const& sample : due)
            {
                m_dispatcher.TryEnqueue(
                    [weak = std::weak_ptr<LfoGenerator>{ shared_from_this() }, sample]()
                    {
                        auto strong = weak.lock();

                        if (strong != nullptr && strong->ValueMoved)
                        {
                            strong->ValueMoved(
                                sample.ControlIndex, sample.Value, sample.Phase, true);
                        }
                    });
            }
        }
    }
}
