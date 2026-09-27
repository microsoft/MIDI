// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#include "pch.h"
#include "StepSequencer.h"
#include "StepPattern.h"

#include <algorithm>
#include <chrono>
#include <cmath>

namespace glass
{
    namespace
    {
        // Long enough that an idle sequencer costs nothing, short enough that Stop is never left
        // waiting on a thread that is asleep for a minute.
        constexpr uint32_t IdleWaitMilliseconds = 1000;

        // Below this the thread hands the rest of the wait to a spin, because sleeping for two
        // milliseconds routinely overshoots by one, and a step late by a millisecond is a flam.
        constexpr int64_t SpinThresholdMicroseconds = 2000;

        // A wakeup this many steps late gives up on the steps it missed and carries on from now,
        // so a page fault leaves a gap in the pattern rather than firing every missed note at once.
        constexpr double StepsBehindBeforeCatchingUp = 4.0;
    }

    std::shared_ptr<StepSequencer> StepSequencer::Create()
    {
        return std::shared_ptr<StepSequencer>(new StepSequencer());
    }

    StepSequencer::~StepSequencer() noexcept
    {
        Stop();
    }

    uint64_t StepSequencer::NowMicroseconds() noexcept
    {
        return static_cast<uint64_t>(
            std::chrono::duration_cast<std::chrono::microseconds>(
                std::chrono::steady_clock::now().time_since_epoch()).count());
    }

    _Use_decl_annotations_
    uint64_t StepSequencer::NextStepMicrosecondsOf(RunningSequence const& sequence) noexcept
    {
        return sequence.OriginMicroseconds + static_cast<uint64_t>(std::llround(
            StepStartMicroseconds(sequence.Spec, sequence.StepsStarted, sequence.StepMicroseconds)));
    }

    _Use_decl_annotations_
    void StepSequencer::Start(winrt::Microsoft::UI::Dispatching::DispatcherQueue const& dispatcher)
    {
        std::lock_guard guard{ m_lock };

        if (m_thread.joinable())
        {
            return;
        }

        m_dispatcher = dispatcher;
        m_stopping = false;

        m_thread = std::thread([weak = std::weak_ptr<StepSequencer>{ shared_from_this() }]()
            {
                if (auto strong = weak.lock())
                {
                    strong->TimeLoop();
                }
            });
    }

    void StepSequencer::Stop() noexcept
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

                m_sequences.clear();
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

    void StepSequencer::Wake() noexcept
    {
        m_wakeup.notify_all();
    }

    _Use_decl_annotations_
    uint64_t StepSequencer::Run(uint32_t controlIndex, StepsSpec const& spec, double beatsPerMinute)
    {
        try
        {
            // Nothing to play is not a sequence that runs silently: the control would look
            // live and do nothing, which reads as broken.
            if (SequencerStepCount(spec) == 0)
            {
                return 0;
            }

            uint64_t run{ 0 };

            {
                std::lock_guard guard{ m_lock };

                m_sequences.erase(
                    std::remove_if(
                        m_sequences.begin(),
                        m_sequences.end(),
                        [controlIndex](RunningSequence const& existing)
                        { return existing.ControlIndex == controlIndex; }),
                    m_sequences.end());

                RunningSequence sequence{};

                sequence.ControlIndex = controlIndex;
                sequence.Run = m_nextRun++;
                sequence.Spec = spec;
                sequence.StepMicroseconds = StepMicroseconds(spec, beatsPerMinute);
                sequence.OriginMicroseconds = NowMicroseconds();
                sequence.StepsStarted = 0;

                // Different every run, so two random sequences started together do not play
                // the same walk.
                sequence.RandomState = static_cast<uint32_t>(
                    sequence.OriginMicroseconds ^ (sequence.OriginMicroseconds >> 32) ^ (controlIndex * 0x9E3779B9u)) | 1u;

                run = sequence.Run;

                m_sequences.push_back(std::move(sequence));

                m_signaled = true;
            }

            Wake();

            return run;
        }
        catch (...)
        {
            return 0;
        }
    }

    _Use_decl_annotations_
    void StepSequencer::CancelFor(uint32_t controlIndex) noexcept
    {
        try
        {
            {
                std::lock_guard guard{ m_lock };

                m_sequences.erase(
                    std::remove_if(
                        m_sequences.begin(),
                        m_sequences.end(),
                        [controlIndex](RunningSequence const& existing)
                        { return existing.ControlIndex == controlIndex; }),
                    m_sequences.end());

                m_signaled = true;
            }

            Wake();
        }
        catch (...)
        {
        }
    }

    void StepSequencer::CancelAll() noexcept
    {
        try
        {
            {
                std::lock_guard guard{ m_lock };

                m_sequences.clear();
                m_signaled = true;
            }

            Wake();
        }
        catch (...)
        {
        }
    }

    _Use_decl_annotations_
    bool StepSequencer::IsRunning(uint32_t controlIndex) const noexcept
    {
        try
        {
            std::lock_guard guard{ m_lock };

            for (auto const& sequence : m_sequences)
            {
                if (sequence.ControlIndex == controlIndex)
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
    bool StepSequencer::IsCurrent(uint32_t controlIndex, uint64_t run) const noexcept
    {
        try
        {
            std::lock_guard guard{ m_lock };

            for (auto const& sequence : m_sequences)
            {
                if (sequence.ControlIndex == controlIndex)
                {
                    return sequence.Run == run;
                }
            }
        }
        catch (...)
        {
        }

        return false;
    }

    size_t StepSequencer::RunningCount() const noexcept
    {
        try
        {
            std::lock_guard guard{ m_lock };

            return m_sequences.size();
        }
        catch (...)
        {
            return 0;
        }
    }

    void StepSequencer::TimeLoop()
    {
        for (;;)
        {
            std::vector<Due> due{};

            uint64_t nextDue{ 0 };

            try
            {
                std::unique_lock guard{ m_lock };

                if (m_stopping)
                {
                    return;
                }

                auto const now = NowMicroseconds();

                for (auto& sequence : m_sequences)
                {
                    // The note of the step before goes first, so an ending and a start that fall
                    // due together leave in that order and the new note is not cut off.
                    if (sequence.NoteEndPending && sequence.NoteEndMicroseconds <= now)
                    {
                        due.push_back({ sequence.ControlIndex, sequence.Run, sequence.SoundingStep, false });
                        sequence.NoteEndPending = false;
                    }

                    auto at = NextStepMicrosecondsOf(sequence);

                    if (at <= now)
                    {
                        if (static_cast<double>(now - at) > sequence.StepMicroseconds * StepsBehindBeforeCatchingUp)
                        {
                            // Moved so the step that is due now lands now. The count is kept, so
                            // the pattern carries on from where it was and swing keeps its feel.
                            sequence.OriginMicroseconds = now - static_cast<uint64_t>(std::llround(
                                StepStartMicroseconds(sequence.Spec, sequence.StepsStarted, sequence.StepMicroseconds)));

                            at = NextStepMicrosecondsOf(sequence);
                        }

                        if (sequence.NoteEndPending)
                        {
                            due.push_back({ sequence.ControlIndex, sequence.Run, sequence.SoundingStep, false });
                            sequence.NoteEndPending = false;
                        }

                        auto const index = StepIndexAt(sequence.Spec, sequence.StepsStarted, sequence.RandomState);
                        auto const gate = GateMicroseconds(sequence.Spec, sequence.StepsStarted, sequence.StepMicroseconds);

                        due.push_back({ sequence.ControlIndex, sequence.Run, index, true });

                        sequence.SoundingStep = index;
                        sequence.NoteEndPending = true;
                        sequence.NoteEndMicroseconds = at + static_cast<uint64_t>(std::llround(gate));
                        sequence.StepsStarted++;
                    }

                    auto next = NextStepMicrosecondsOf(sequence);

                    if (sequence.NoteEndPending && sequence.NoteEndMicroseconds < next)
                    {
                        next = sequence.NoteEndMicroseconds;
                    }

                    if (nextDue == 0 || next < nextDue)
                    {
                        nextDue = next;
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
            catch (...)
            {
                // An exception leaving a thread body ends the process. Losing one pass of the
                // loop is the better failure.
                continue;
            }

            if (m_dispatcher == nullptr)
            {
                continue;
            }

            for (auto const& event : due)
            {
                try
                {
                    m_dispatcher.TryEnqueue(
                        [weak = std::weak_ptr<StepSequencer>{ shared_from_this() }, event]()
                        {
                            auto strong = weak.lock();

                            if (strong != nullptr && strong->StepChanged)
                            {
                                strong->StepChanged(event.ControlIndex, event.Run, event.StepIndex, event.Starts);
                            }
                        });
                }
                catch (...)
                {
                }
            }
        }
    }
}
