// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

// The analyzer's time-ordered pass: which audio threads were kept waiting, and what ran instead.
// See LatencyAnalyzer.h.

#include "LatencyAnalyzer.h"
#include "LatencyDriverNames.h"
#include "LatencyKernelEvents.h"

#include <algorithm>
#include <limits>
#include <map>
#include <string_view>
#include <tuple>

namespace miditroubleshooter::latency
{
    namespace
    {
        // Windows' own real-time threads, which draw the screen and handle input. They aren't
        // audio, so they don't count.
        bool IsWindowsInterfaceProcess(_In_ std::wstring_view const name) noexcept
        {
            if (name.empty())
            {
                return false;
            }

            // the kernel's own processes, such as Registry and MemCompression, have no .exe
            if (name.size() < 4 || !names::EqualsNoCase(name.substr(name.size() - 4), L".exe"))
            {
                return true;
            }

            return names::EqualsNoCase(name, L"csrss.exe") || names::EqualsNoCase(name, L"dwm.exe");
        }
        // MMCSS puts audio threads in the real-time range, 16 and up. A thread there in an
        // ordinary process can only have been put there by MMCSS, or lifted there for a moment
        // because a real-time thread was waiting on a lock it held.
        constexpr int32_t RealTimePriority{ 16 };

        // A thread that ran at real-time priority at least this often, and at least this share of
        // the times it ran, is an audio thread. One that was only lifted for a moment isn't.
        constexpr uint32_t MinimumRealTimeRuns{ 20 };
        constexpr uint32_t MinimumRealTimeSharePercent{ 50 };

        // Threads seen at real-time priority, kept until Finish decides. A busy PC has a few
        // hundred, and a damaged trace can't make more than this.
        constexpr size_t MaximumCandidates{ 4096 };

        // the System and Idle processes, whose threads are the kernel's own
        constexpr uint32_t IdleProcessId{ 0 };
        constexpr uint32_t SystemProcessId{ 4 };

        // A wait longer than this means an event went missing, not that a thread waited.
        constexpr uint64_t MaximumWaitMicroseconds{ 1'000'000 };

        // waits shorter than this aren't worth a line in the list of longest waits
        constexpr uint64_t ListFloorMicroseconds{ 100 };

        constexpr size_t MaximumCauses{ 3 };

        // A thread is only blamed for holding a processor where timers wait when at least this
        // many profile samples caught it there, all in kernel code. With one, it's too easily a
        // thread that happened to run while the timer was only slow.
        constexpr uint32_t MinimumStallSamples{ 2 };

        // A thread that ran at least this long, and twice the time between samples, without one
        // profile sample means the processor wasn't answering at all.
        constexpr uint64_t UnseenStallMicroseconds{ 2'000 };

        uint64_t ExplainedTicks(_In_ std::vector<WaitCause> const& causes) noexcept
        {
            uint64_t ticks{ 0 };

            for (auto const& cause : causes)
            {
                if (cause.Kind == CauseKind::Dpc || cause.Kind == CauseKind::Isr || cause.Kind == CauseKind::Thread ||
                    cause.Kind == CauseKind::RaisedIrql)
                {
                    ticks += cause.Ticks;
                }
            }

            return ticks;
        }
    }

    _Use_decl_annotations_
    void Analyzer::OnOrdered(OrderedEvent const& event) noexcept
    {
        try
        {
            switch (event.Kind)
            {
            case OrderedKind::ContextSwitch:
                OnContextSwitch(event);
                break;

            case OrderedKind::ReadyThread:
            {
                auto& thread = Thread(event.ThreadId);

                // the first time it's made ready counts; a second one while it waits is the
                // kernel bringing its stack back in
                if (!thread.ReadyPending)
                {
                    thread.ReadyPending = true;
                    thread.ReadyTime = event.Time;
                    thread.ReadyProcessor = event.Processor;
                }
                break;
            }

            case OrderedKind::Interval:
                OnInterval(event);
                break;

            case OrderedKind::HardFault:
                OnHardFault(event);
                break;

            case OrderedKind::ProbeLateWake:
                OnProbeLateWake(event);
                break;

            case OrderedKind::Sample:
                Processor(event.Processor).OrderedSamples.Push(TimedSample{ event.Time, event.Address, event.ThreadId });
                break;
            }
        }
        catch (...)
        {
            SkipEvent();
        }
    }

    _Use_decl_annotations_
    AudioThreadStats* Analyzer::AudioThread(uint32_t const threadId, ThreadState& state, int32_t const priority)
    {
        // a thread past the limit is never tracked
        if (&state == &m_overflowThread)
        {
            return nullptr;
        }

        state.HighestPriority = (std::max)(state.HighestPriority, priority);

        if (state.AudioIndex < 0)
        {
            auto const isProbe = m_settings.ProbeThreadId != 0 && threadId == m_settings.ProbeThreadId;
            auto const isRealTime = priority >= RealTimePriority && state.ProcessKnown &&
                state.ProcessId != IdleProcessId && state.ProcessId != SystemProcessId;

            if (!isProbe && (!isRealTime || m_results.AudioThreads.size() >= MaximumCandidates))
            {
                return nullptr;
            }

            AudioThreadStats stats{};
            stats.ThreadId = threadId;
            stats.ProcessId = state.ProcessKnown ? state.ProcessId : m_settings.ProbeProcessId;
            stats.ThreadName = state.Name;
            stats.Kind = isProbe ? AudioThreadKind::Probe : AudioThreadKind::Audio;

            m_results.AudioThreads.push_back(std::move(stats));

            // The two are indexed together, so they stay the same length.
            try
            {
                m_candidates.emplace_back();
            }
            catch (...)
            {
                m_results.AudioThreads.pop_back();
                throw;
            }

            state.AudioIndex = static_cast<int32_t>(m_results.AudioThreads.size() - 1);
        }

        auto& audio = m_results.AudioThreads[static_cast<size_t>(state.AudioIndex)];
        audio.HighestPriority = (std::max)(audio.HighestPriority, priority);

        ++m_candidates[static_cast<size_t>(state.AudioIndex)].PriorityRuns[static_cast<size_t>(priority) & 31];

        return &audio;
    }

    _Use_decl_annotations_
    bool Analyzer::CountsAsAudio(AudioThreadStats const& audio) const
    {
        if (audio.IsProbe())
        {
            return true;
        }

        auto const found = m_threads.find(audio.ThreadId);

        if (found == m_threads.end())
        {
            return false;
        }

        auto const& state = found->second;

        if (state.RealTimeRuns < MinimumRealTimeRuns ||
            static_cast<uint64_t>(state.RealTimeRuns) * 100 < static_cast<uint64_t>(state.Runs) * MinimumRealTimeSharePercent)
        {
            return false;
        }

        auto const name = m_processNames.find(audio.ProcessId);

        return name == m_processNames.end() || !IsWindowsInterfaceProcess(name->second);
    }

    _Use_decl_annotations_
    void Analyzer::OnContextSwitch(OrderedEvent const& event)
    {
        auto& processor = Processor(event.Processor);

        // the thread that was running stops here
        if (processor.RunningKnown && processor.RunningSince <= event.Time)
        {
            ThreadRun run{};
            run.Start = processor.RunningSince;
            run.End = event.Time;
            run.ThreadId = event.OtherThreadId;
            run.Priority = processor.RunningPriority;

            // switching away from the idle thread says how deeply the processor was asleep
            run.IdleState = event.OtherThreadId == 0 ? event.IdleState : 0;

            processor.Runs.Push(run);
        }

        auto& thread = Thread(event.ThreadId);
        AudioThreadStats* audio{ nullptr };

        if (event.ThreadId != 0)
        {
            ++thread.Runs;

            if (event.NewPriority >= RealTimePriority)
            {
                ++thread.RealTimeRuns;
            }

            audio = AudioThread(event.ThreadId, thread, event.NewPriority);
        }

        if (audio != nullptr && thread.ReadyPending && thread.ReadyTime <= event.Time)
        {
            auto const wait = event.Time - thread.ReadyTime;
            auto const microseconds = TicksToMicroseconds(wait, m_settings.Frequency);

            if (microseconds <= MaximumWaitMicroseconds)
            {
                audio->Waits.Add(wait, microseconds, event.Time, event.Processor);

                if (microseconds >= ListFloorMicroseconds)
                {
                    LateMoment moment{};
                    moment.Kind = MomentKind::WaitedToRun;
                    moment.StartTime = thread.ReadyTime;
                    moment.WaitTicks = wait;
                    moment.Processor = event.Processor;
                    moment.ReadyProcessor = thread.ReadyProcessor;
                    moment.ThreadId = event.ThreadId;
                    moment.ProcessId = audio->ProcessId;
                    moment.Causes = Blame(event.Processor, thread.ReadyTime, event.Time, event.ThreadId);

                    // When this processor was mostly idle, the thread was waiting somewhere else,
                    // most likely on the processor that made it ready.
                    if (ExplainedTicks(moment.Causes) * 2 < wait && thread.ReadyProcessor != event.Processor)
                    {
                        auto other = Blame(thread.ReadyProcessor, thread.ReadyTime, event.Time, event.ThreadId);

                        if (ExplainedTicks(other) > ExplainedTicks(moment.Causes))
                        {
                            moment.Causes = std::move(other);
                        }
                    }

                    RecordMoment(thread.AudioIndex, std::move(moment));
                }
            }
        }

        thread.ReadyPending = false;

        processor.RunningKnown = true;
        processor.RunningThread = event.ThreadId;
        processor.RunningPriority = event.NewPriority;
        processor.RunningSince = event.Time;
    }

    _Use_decl_annotations_
    void Analyzer::OnInterval(OrderedEvent const& event)
    {
        auto& processor = Processor(event.Processor);

        processor.Intervals.Push(Interval{ event.Start, event.Time, event.Routine, event.IsIsr });

        // A DPC or an interrupt that stopped an audio thread while it was running. No context
        // switch shows this, because the thread never stops being the one running.
        if (!processor.RunningKnown || processor.RunningThread == 0 || processor.RunningSince > event.Start)
        {
            return;
        }

        auto const found = m_threads.find(processor.RunningThread);

        if (found == m_threads.end() || found->second.AudioIndex < 0)
        {
            return;
        }

        auto& audio = m_results.AudioThreads[static_cast<size_t>(found->second.AudioIndex)];

        auto const ticks = event.Time - event.Start;
        auto const microseconds = TicksToMicroseconds(ticks, m_settings.Frequency);

        audio.Interruptions.Add(ticks, microseconds, event.Time, event.Processor);

        if (microseconds >= ListFloorMicroseconds)
        {
            LateMoment moment{};
            moment.Kind = MomentKind::Interrupted;
            moment.StartTime = event.Start;
            moment.WaitTicks = ticks;
            moment.Processor = event.Processor;
            moment.ReadyProcessor = event.Processor;
            moment.ThreadId = processor.RunningThread;
            moment.ProcessId = audio.ProcessId;
            moment.Causes = Blame(event.Processor, event.Start, event.Time, processor.RunningThread);

            RecordMoment(found->second.AudioIndex, std::move(moment));
        }
    }

    _Use_decl_annotations_
    void Analyzer::OnHardFault(OrderedEvent const& event)
    {
        auto const found = m_threads.find(event.ThreadId);

        if (found == m_threads.end() || found->second.AudioIndex < 0 || event.Start > event.Time)
        {
            return;
        }

        auto const ticks = event.Time - event.Start;

        m_results.AudioThreads[static_cast<size_t>(found->second.AudioIndex)].HardFaults.Add(
            ticks, TicksToMicroseconds(ticks, m_settings.Frequency), event.Time, event.Processor);
    }

    _Use_decl_annotations_
    void Analyzer::OnProbeLateWake(OrderedEvent const& event)
    {
        auto const found = m_threads.find(event.ThreadId);

        if (found == m_threads.end() || found->second.AudioIndex < 0)
        {
            return;
        }

        auto& thread = found->second;

        // The wake this is about was made ready after its deadline and before it ran. Made ready
        // before the deadline, the timer was on time and the wait to run says the rest.
        if (thread.ReadyTime <= event.Start || thread.ReadyTime > event.Time)
        {
            return;
        }

        // The processor that made it ready is the one that ran the timer. The DPCs and interrupts
        // there could hold the timer up, and so could a thread that held that processor where
        // timers wait. The rest of the time is the timer's own slack, which every timer has and
        // an audio device's interrupt doesn't.
        auto causes = Blame(thread.ReadyProcessor, event.Start, thread.ReadyTime, event.ThreadId, true);

        auto const window = thread.ReadyTime - event.Start;
        auto const interrupts = ExplainedTicks(causes);

        if (window > interrupts)
        {
            if (auto stall = FindStall(thread.ReadyProcessor, event.Start, thread.ReadyTime))
            {
                stall->Ticks = (std::min)(stall->Ticks, window - interrupts);

                auto const stallMicroseconds = TicksToMicroseconds(stall->Ticks, m_settings.Frequency);

                if (stall->Kind == CauseKind::RaisedIrql)
                {
                    m_raisedIrql[stall->Driver].Add(stall->Ticks, stallMicroseconds, thread.ReadyTime, thread.ReadyProcessor);

                    causes.push_back(*stall);

                    std::sort(causes.begin(), causes.end(), [](WaitCause const& left, WaitCause const& right)
                        {
                            return left.Ticks > right.Ticks;
                        });
                }
                else
                {
                    m_results.AudioThreads[static_cast<size_t>(thread.AudioIndex)].UnseenStalls.Add(
                        stall->Ticks, stallMicroseconds, thread.ReadyTime, thread.ReadyProcessor);
                }
            }
        }

        auto const blocked = ExplainedTicks(causes);

        if (blocked == 0)
        {
            return;
        }

        auto const microseconds = TicksToMicroseconds(blocked, m_settings.Frequency);

        auto& audio = m_results.AudioThreads[static_cast<size_t>(thread.AudioIndex)];
        audio.LateWakes.Add(blocked, microseconds, thread.ReadyTime, thread.ReadyProcessor);

        if (microseconds < ListFloorMicroseconds)
        {
            return;
        }

        LateMoment moment{};
        moment.Kind = MomentKind::WokeLate;
        moment.StartTime = event.Start;
        moment.WaitTicks = blocked;
        moment.Processor = thread.ReadyProcessor;
        moment.ReadyProcessor = thread.ReadyProcessor;
        moment.ThreadId = event.ThreadId;
        moment.ProcessId = audio.ProcessId;
        moment.Causes = std::move(causes);

        RecordMoment(thread.AudioIndex, std::move(moment));
    }

    _Use_decl_annotations_
    std::optional<WaitCause> Analyzer::FindStall(uint32_t const processorIndex, uint64_t const from, uint64_t const to)
    {
        if (processorIndex >= m_processors.size() || to <= from)
        {
            return std::nullopt;
        }

        auto const& processor = m_processors[processorIndex];

        // the longest stretch one thread ran there in the window, without a switch
        ThreadRun longest{};
        uint64_t longestTicks{ 0 };

        auto const consider = [&longest, &longestTicks, from, to](ThreadRun const& run)
            {
                auto const start = (std::max)(run.Start, from);
                auto const end = (std::min)(run.End, to);

                if (end > start && end - start > longestTicks)
                {
                    longest = run;
                    longestTicks = end - start;
                }
            };

        for (size_t index = 0; index < processor.Runs.Count(); ++index)
        {
            auto const& run = processor.Runs.FromNewest(index);

            if (run.End <= from)
            {
                break;
            }

            consider(run);
        }

        // whatever is running there right now has been running since then
        if (processor.RunningKnown && processor.RunningSince < to)
        {
            consider(ThreadRun{ processor.RunningSince, (std::numeric_limits<uint64_t>::max)(),
                processor.RunningThread, processor.RunningPriority, 0 });
        }

        // an idle processor that fired its timer late, which is the timer's own slack
        if (longestTicks == 0 || longest.ThreadId == 0)
        {
            return std::nullopt;
        }

        auto const stretchStart = (std::max)(longest.Start, from);
        auto const stretchEnd = (std::min)(longest.End, to);

        // where the profile samples caught that thread, and how often this processor takes them
        std::vector<std::pair<int32_t, uint32_t>> modules{};
        uint32_t samples{ 0 };
        bool appCode{ false };
        uint64_t interval{ 0 };
        uint64_t newer{ 0 };

        for (size_t index = 0; index < processor.OrderedSamples.Count(); ++index)
        {
            auto const& sample = processor.OrderedSamples.FromNewest(index);

            if (newer > sample.Time && (interval == 0 || newer - sample.Time < interval))
            {
                interval = newer - sample.Time;
            }

            newer = sample.Time;

            if (sample.Time < stretchStart || sample.Time > stretchEnd || sample.ThreadId != longest.ThreadId)
            {
                continue;
            }

            ++samples;

            if (!kernel::IsKernelAddress(sample.InstructionPointer, m_settings.PointerSize))
            {
                appCode = true;
                continue;
            }

            auto const module = FindModule(sample.InstructionPointer);
            auto const counted = std::find_if(modules.begin(), modules.end(), [module](std::pair<int32_t, uint32_t> const& entry)
                {
                    return entry.first == module;
                });

            if (counted == modules.end())
            {
                modules.emplace_back(module, 1u);
            }
            else
            {
                ++counted->second;
            }
        }

        WaitCause cause{};
        cause.Ticks = stretchEnd - stretchStart;
        cause.ThreadId = longest.ThreadId;
        cause.Priority = longest.Priority;

        if (auto const thread = m_threads.find(longest.ThreadId); thread != m_threads.end())
        {
            cause.ProcessId = thread->second.ProcessId;
        }

        // not even the profile interrupt got in, so the processor wasn't answering at all
        if (samples == 0)
        {
            if (interval == 0 || cause.Ticks < (std::max)(MicrosecondsToTicks(UnseenStallMicroseconds, m_settings.Frequency), interval * 2))
            {
                return std::nullopt;
            }

            cause.Kind = CauseKind::Unknown;
            return cause;
        }

        // A thread caught running an app's code was where timers still go off, so the timer was
        // only slow. A thread that held the processor where timers wait shows only kernel code.
        if (appCode || samples < MinimumStallSamples)
        {
            return std::nullopt;
        }

        auto const best = std::max_element(modules.begin(), modules.end(), [](std::pair<int32_t, uint32_t> const& left, std::pair<int32_t, uint32_t> const& right)
            {
                return left.second < right.second;
            });

        cause.Kind = CauseKind::RaisedIrql;
        cause.Driver = best != modules.end() ? best->first : -1;

        return cause;
    }

    _Use_decl_annotations_
    std::vector<WaitCause> Analyzer::Blame(
        uint32_t const processorIndex,
        uint64_t const from,
        uint64_t const to,
        uint32_t const waitingThread,
        bool const interruptsOnly) const
    {
        std::vector<WaitCause> causes{};

        if (processorIndex >= m_processors.size() || to <= from)
        {
            return causes;
        }

        auto const& processor = m_processors[processorIndex];

        // Each history is kept in the order things ended, so the walk stops at the first one that
        // ended before the window opened.
        std::vector<Interval> intervals{};

        for (size_t index = 0; index < processor.Intervals.Count(); ++index)
        {
            auto const& interval = processor.Intervals.FromNewest(index);

            if (interval.End <= from)
            {
                break;
            }

            if (interval.Start < to)
            {
                intervals.push_back(interval);
            }
        }

        std::vector<ThreadRun> runs{};

        for (size_t index = 0; index < processor.Runs.Count(); ++index)
        {
            auto const& run = processor.Runs.FromNewest(index);

            if (run.End <= from)
            {
                break;
            }

            if (run.Start < to)
            {
                runs.push_back(run);
            }
        }

        // whatever is running on that processor right now has been running since then
        if (processor.RunningKnown && processor.RunningSince < to)
        {
            runs.push_back(ThreadRun{ processor.RunningSince, (std::numeric_limits<uint64_t>::max)(),
                processor.RunningThread, processor.RunningPriority, 0 });
        }

        // Cut the window at every start and end, then give each piece to whatever was on top:
        // the innermost DPC or interrupt, else the thread that was running.
        std::vector<uint64_t> edges{ from, to };

        for (auto const& interval : intervals)
        {
            edges.push_back((std::max)(interval.Start, from));
            edges.push_back((std::min)(interval.End, to));
        }

        for (auto const& run : runs)
        {
            edges.push_back((std::max)(run.Start, from));
            edges.push_back((std::min)(run.End, to));
        }

        std::sort(edges.begin(), edges.end());
        edges.erase(std::unique(edges.begin(), edges.end()), edges.end());

        // kind, routine or thread -> ticks
        std::map<std::tuple<CauseKind, int64_t>, WaitCause> totals{};

        auto const add = [&totals](WaitCause const& cause, int64_t const key)
            {
                auto& total = totals[{ cause.Kind, key }];

                if (total.Ticks == 0)
                {
                    total = cause;
                    total.Ticks = 0;
                }

                total.Ticks += cause.Ticks;
            };

        for (size_t edge = 0; edge + 1 < edges.size(); ++edge)
        {
            auto const pieceStart = edges[edge];
            auto const pieceEnd = edges[edge + 1];

            if (pieceStart < from || pieceEnd > to || pieceEnd <= pieceStart)
            {
                continue;
            }

            WaitCause cause{};
            cause.Ticks = pieceEnd - pieceStart;

            // the innermost one is the one that started last
            Interval const* innermost{ nullptr };

            for (auto const& interval : intervals)
            {
                if (interval.Start <= pieceStart && interval.End >= pieceEnd &&
                    (innermost == nullptr || interval.Start > innermost->Start))
                {
                    innermost = &interval;
                }
            }

            if (innermost != nullptr)
            {
                cause.Kind = innermost->IsIsr ? CauseKind::Isr : CauseKind::Dpc;
                cause.Routine = innermost->Routine;
                add(cause, innermost->Routine);
                continue;
            }

            if (interruptsOnly)
            {
                continue;
            }

            ThreadRun const* running{ nullptr };

            for (auto const& run : runs)
            {
                if (run.Start <= pieceStart && run.End >= pieceEnd)
                {
                    running = &run;
                    break;
                }
            }

            if (running == nullptr || running->ThreadId == waitingThread)
            {
                cause.Kind = CauseKind::Unknown;
                add(cause, 0);
            }
            else if (running->ThreadId == 0)
            {
                cause.Kind = CauseKind::ProcessorIdle;
                cause.IdleState = running->IdleState;
                add(cause, 0);
            }
            else
            {
                cause.Kind = CauseKind::Thread;
                cause.ThreadId = running->ThreadId;
                cause.Priority = running->Priority;

                if (auto const thread = m_threads.find(running->ThreadId); thread != m_threads.end())
                {
                    cause.ProcessId = thread->second.ProcessId;
                }

                add(cause, static_cast<int64_t>(running->ThreadId));
            }
        }

        causes.reserve(totals.size());

        for (auto& [key, cause] : totals)
        {
            causes.push_back(cause);
        }

        std::sort(causes.begin(), causes.end(), [](WaitCause const& left, WaitCause const& right)
            {
                return left.Ticks > right.Ticks;
            });

        return causes;
    }

    _Use_decl_annotations_
    void Analyzer::RecordMoment(int32_t const audioIndex, LateMoment&& moment)
    {
        if (audioIndex < 0 || static_cast<size_t>(audioIndex) >= m_candidates.size())
        {
            return;
        }

        // the few biggest causes say what happened; the rest are crumbs
        if (moment.Causes.size() > MaximumCauses)
        {
            moment.Causes.resize(MaximumCauses);
        }

        auto& audio = m_results.AudioThreads[static_cast<size_t>(audioIndex)];
        auto& candidate = m_candidates[static_cast<size_t>(audioIndex)];

        auto const microseconds = TicksToMicroseconds(moment.WaitTicks, m_settings.Frequency);
        auto const late = microseconds >= LateMomentMicroseconds;
        auto const end = moment.StartTime + moment.WaitTicks;

        if (m_incidents.size() <= moment.Processor)
        {
            m_incidents.resize(m_processors.size());
        }

        // An interrupt inside a DPC that both stopped the same thread is one incident, seen twice.
        auto& incident = m_incidents[moment.Processor];

        auto const sameIncident = incident.Valid && incident.Kind == moment.Kind && incident.ThreadId == moment.ThreadId &&
            moment.StartTime <= incident.End && end >= incident.Start;

        if (!sameIncident)
        {
            incident = Incident{ true, false, moment.Kind, moment.ThreadId, moment.StartTime, end };
        }
        else
        {
            incident.Start = (std::min)(incident.Start, moment.StartTime);
            incident.End = (std::max)(incident.End, end);
        }

        if (late)
        {
            audio.WorstLateTicks = (std::max)(audio.WorstLateTicks, moment.WaitTicks);
        }

        if (late && !incident.Counted)
        {
            incident.Counted = true;
            ++audio.LateMoments;

            // the biggest cause only
            if (!moment.Causes.empty())
            {
                auto const& cause = moment.Causes.front();

                if ((cause.Kind == CauseKind::Dpc || cause.Kind == CauseKind::Isr) && cause.Routine >= 0)
                {
                    auto& blame = candidate.Blame[cause.Routine];

                    ++blame.Count;
                    blame.WorstTicks = (std::max)(blame.WorstTicks, moment.WaitTicks);
                }
                else if (cause.Kind == CauseKind::RaisedIrql)
                {
                    auto& blame = candidate.ModuleBlame[cause.Driver];

                    ++blame.Count;
                    blame.WorstTicks = (std::max)(blame.WorstTicks, moment.WaitTicks);
                }
            }
        }

        auto& list = candidate.LongestWaits;

        // the same incident already listed: keep the longer view of it
        for (auto& listed : list)
        {
            if (listed.Processor == moment.Processor && listed.ThreadId == moment.ThreadId && listed.Kind == moment.Kind &&
                moment.StartTime <= listed.StartTime + listed.WaitTicks && end >= listed.StartTime)
            {
                if (moment.WaitTicks > listed.WaitTicks)
                {
                    listed = std::move(moment);
                }

                return;
            }
        }

        if (list.size() < MaximumLongestWaits)
        {
            list.push_back(std::move(moment));
            return;
        }

        auto shortest = std::min_element(list.begin(), list.end(), [](LateMoment const& left, LateMoment const& right)
            {
                return left.WaitTicks < right.WaitTicks;
            });

        if (shortest != list.end() && shortest->WaitTicks < moment.WaitTicks)
        {
            *shortest = std::move(moment);
        }
    }
}
