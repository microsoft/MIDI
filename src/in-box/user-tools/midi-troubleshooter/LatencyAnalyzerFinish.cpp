// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

// The analyzer's last step: charge each DPC and interrupt to a driver, and tidy the results.
// See LatencyAnalyzer.h.

#include "LatencyAnalyzer.h"
#include "LatencyDriverNames.h"

#include <algorithm>
#include <unordered_map>

namespace miditroubleshooter::latency
{
    namespace
    {
        // Samples only take over from a framework when there are enough of them to mean something,
        // and the driver they point to has a fair share of them.
        constexpr uint32_t MinimumSamplesToCharge{ 3 };
        constexpr uint32_t MinimumSharePercentToCharge{ 25 };

        // threads that ran at real-time priority only now and then, kept for the report
        constexpr size_t MaximumOtherRealTimeThreads{ 20 };
    }

    void Analyzer::Finish() noexcept
    {
        try
        {
            if (m_finished)
            {
                return;
            }

            DrainOrdered(true);
            m_finished = true;

            auto& trace = m_results.Trace;
            trace.Frequency = m_settings.Frequency;
            trace.ProcessorCount = (std::max)(m_settings.ProcessorCount, static_cast<uint32_t>(m_processors.size()));
            trace.PointerSize = m_settings.PointerSize;
            trace.ClockType = m_settings.ClockType;
            trace.StartTime = m_firstTime;
            trace.EndTime = m_latestTime;

            if (m_results.Processors.size() < trace.ProcessorCount)
            {
                Processor(trace.ProcessorCount - 1);
            }

            for (auto& module : m_results.Modules)
            {
                module.IsFramework = names::IsFramework(module.FileName);
                module.Kind = names::KindFromFileName(module.FileName);
            }

            // lookups that came before the module list did
            for (auto& routine : m_results.Routines)
            {
                if (routine.Module < 0)
                {
                    routine.Module = FindModule(routine.Routine);
                }

                if (routine.ClientRoutine != 0 && routine.ClientModule < 0)
                {
                    routine.ClientModule = FindModule(routine.ClientRoutine);
                }
            }

            for (auto const& [index, addresses] : m_unresolvedSamples)
            {
                auto& routine = m_results.Routines[static_cast<size_t>(index)];

                for (auto const address : addresses)
                {
                    AddSampledModule(routine, FindModule(address));
                }
            }

            ClassifyAudioThreads();
            ChargeRoutinesToDrivers();
            BuildDrivers();

            // names for the threads and processes the results mention
            auto const nameThread = [this](AudioThreadStats& audio)
                {
                    if (auto const found = m_threads.find(audio.ThreadId); found != m_threads.end())
                    {
                        if (audio.ThreadName.empty())
                        {
                            audio.ThreadName = found->second.Name;
                        }

                        if (audio.ProcessId == 0 && found->second.ProcessKnown)
                        {
                            audio.ProcessId = found->second.ProcessId;
                        }
                    }
                };

            std::for_each(m_results.AudioThreads.begin(), m_results.AudioThreads.end(), nameThread);
            std::for_each(m_results.OtherRealTimeThreads.begin(), m_results.OtherRealTimeThreads.end(), nameThread);

            std::vector<uint32_t> mentioned{};

            for (auto const& audio : m_results.AudioThreads)
            {
                mentioned.push_back(audio.ProcessId);
            }

            for (auto const& other : m_results.OtherRealTimeThreads)
            {
                mentioned.push_back(other.ProcessId);
            }

            for (auto& moment : m_results.LongestWaits)
            {
                mentioned.push_back(moment.ProcessId);

                for (auto& cause : moment.Causes)
                {
                    if (cause.Routine >= 0 && static_cast<size_t>(cause.Routine) < m_results.Routines.size())
                    {
                        cause.Driver = m_results.Routines[static_cast<size_t>(cause.Routine)].Driver;
                    }

                    if (cause.Kind == CauseKind::Thread || cause.Kind == CauseKind::RaisedIrql)
                    {
                        mentioned.push_back(cause.ProcessId);
                    }
                }
            }

            std::sort(mentioned.begin(), mentioned.end());
            mentioned.erase(std::unique(mentioned.begin(), mentioned.end()), mentioned.end());

            for (auto const processId : mentioned)
            {
                ProcessInfo process{};
                process.ProcessId = processId;

                if (auto const found = m_processNames.find(processId); found != m_processNames.end())
                {
                    process.Name = found->second;
                }
                else if (processId == 0)
                {
                    process.Name = L"Idle";
                }
                else if (processId == 4)
                {
                    process.Name = L"System";
                }

                m_results.Processes.push_back(std::move(process));
            }

            std::sort(m_results.LongestWaits.begin(), m_results.LongestWaits.end(), [](LateMoment const& left, LateMoment const& right)
                {
                    return left.WaitTicks > right.WaitTicks;
                });

            // the threads held up longest first
            std::sort(m_results.AudioThreads.begin(), m_results.AudioThreads.end(), [](AudioThreadStats const& left, AudioThreadStats const& right)
                {
                    return (std::max)(left.Waits.MaxTicks, left.Interruptions.MaxTicks) > (std::max)(right.Waits.MaxTicks, right.Interruptions.MaxTicks);
                });

            m_results.Probe.ThreadId = m_settings.ProbeThreadId;

            UpdateSnapshot(true);
        }
        catch (...)
        {
            // what was worked out so far is kept, and the report says it's missing parts
            m_finished = true;
            m_results.Trace.AnalysisIncomplete = true;
        }
    }

    Results Analyzer::TakeResults() noexcept
    {
        return std::move(m_results);
    }

    void Analyzer::ClassifyAudioThreads()
    {
        std::vector<AudioThreadStats> audioThreads{};
        std::vector<AudioThreadStats> others{};

        for (size_t index = 0; index < m_results.AudioThreads.size() && index < m_candidates.size(); ++index)
        {
            auto& audio = m_results.AudioThreads[index];
            auto& candidate = m_candidates[index];

            auto const found = m_threads.find(audio.ThreadId);
            auto const* const state = found != m_threads.end() ? &found->second : nullptr;

            if (state != nullptr)
            {
                audio.Runs = state->Runs;
                audio.RealTimeRuns = state->RealTimeRuns;
            }

            // the priority it ran at most often once it had been seen at real-time priority
            auto const typical = std::max_element(candidate.PriorityRuns.begin(), candidate.PriorityRuns.end());
            audio.TypicalPriority = static_cast<int32_t>(typical - candidate.PriorityRuns.begin());

            if (!CountsAsAudio(audio))
            {
                others.push_back(std::move(audio));
                continue;
            }

            // MIDI messages pass through the MIDI service's threads, so they're shown on their own
            if (!audio.IsProbe())
            {
                if (auto const name = m_processNames.find(audio.ProcessId);
                    name != m_processNames.end() && names::EqualsNoCase(name->second, L"midisrv.exe"))
                {
                    audio.Kind = AudioThreadKind::MidiService;
                }
            }

            for (auto& moment : candidate.LongestWaits)
            {
                m_results.LongestWaits.push_back(std::move(moment));
            }

            for (auto const& [routine, blame] : candidate.Blame)
            {
                auto& total = m_routineBlame[routine];

                total.Count += blame.Count;
                total.WorstTicks = (std::max)(total.WorstTicks, blame.WorstTicks);
            }

            for (auto const& [module, blame] : candidate.ModuleBlame)
            {
                auto& total = m_moduleBlame[module];

                total.Count += blame.Count;
                total.WorstTicks = (std::max)(total.WorstTicks, blame.WorstTicks);
            }

            m_results.LateMomentCount += audio.LateMoments;
            m_results.AllWaits.Merge(audio.Waits);
            m_results.AllInterruptions.Merge(audio.Interruptions);

            audioThreads.push_back(std::move(audio));
        }

        m_candidates.clear();

        std::sort(m_results.LongestWaits.begin(), m_results.LongestWaits.end(), [](LateMoment const& left, LateMoment const& right)
            {
                return left.WaitTicks > right.WaitTicks;
            });

        if (m_results.LongestWaits.size() > MaximumLongestWaits)
        {
            m_results.LongestWaits.resize(MaximumLongestWaits);
        }

        // the ones that ran at real-time priority most often, which is what an expert checks first
        std::sort(others.begin(), others.end(), [](AudioThreadStats const& left, AudioThreadStats const& right)
            {
                return left.RealTimeRuns > right.RealTimeRuns;
            });

        m_results.OtherRealTimeThreadCount = others.size();

        if (others.size() > MaximumOtherRealTimeThreads)
        {
            others.resize(MaximumOtherRealTimeThreads);
        }

        m_results.AudioThreads = std::move(audioThreads);
        m_results.OtherRealTimeThreads = std::move(others);
    }

    void Analyzer::ChargeRoutinesToDrivers()
    {
        for (auto& routine : m_results.Routines)
        {
            routine.Driver = routine.Module;
            routine.DriverFromSamples = false;

            // the framework said which driver routine it called
            if (routine.ClientModule >= 0)
            {
                routine.Driver = routine.ClientModule;
                continue;
            }

            auto const* const module = m_results.Module(routine.Module);

            if (module == nullptr || !module->IsFramework)
            {
                continue;
            }

            // Otherwise, whose code the samples caught running. Windows' own stacks, such as
            // TCP/IP inside a network driver's DPC, don't count against the device driver.
            uint32_t total{ 0 };
            int32_t best{ -1 };
            uint32_t bestCount{ 0 };

            for (auto const& [candidate, count] : routine.SampledModules)
            {
                total += count;

                auto const* const candidateModule = m_results.Module(candidate);

                if (candidateModule == nullptr || names::IsInfrastructure(candidateModule->FileName))
                {
                    continue;
                }

                if (count > bestCount)
                {
                    best = candidate;
                    bestCount = count;
                }
            }

            if (best >= 0 && bestCount >= MinimumSamplesToCharge && bestCount * 100 >= total * MinimumSharePercentToCharge)
            {
                routine.Driver = best;
                routine.DriverFromSamples = true;
            }
        }
    }

    void Analyzer::BuildDrivers()
    {
        std::unordered_map<int32_t, size_t> positions{};
        auto& drivers = m_results.Drivers;

        for (size_t index = 0; index < m_results.Routines.size(); ++index)
        {
            auto const& routine = m_results.Routines[index];

            auto [position, added] = positions.try_emplace(routine.Driver, drivers.size());

            if (added)
            {
                DriverStats driver{};
                driver.Module = routine.Driver;
                drivers.push_back(std::move(driver));
            }

            auto& driver = drivers[position->second];

            if (routine.Kind == InterruptKind::Isr)
            {
                driver.Isr.Merge(routine.Durations);
            }
            else
            {
                driver.Dpc.Merge(routine.Durations);
            }

            for (auto const& [processor, ticks] : routine.TicksPerProcessor)
            {
                if (auto* const existing = FindProcessorTicks(driver.TicksPerProcessor, processor))
                {
                    *existing += ticks;
                }
                else
                {
                    driver.TicksPerProcessor.emplace_back(processor, ticks);
                }
            }

            if (routine.Module >= 0 && routine.Module != routine.Driver &&
                std::find(driver.Frameworks.begin(), driver.Frameworks.end(), routine.Module) == driver.Frameworks.end())
            {
                driver.Frameworks.push_back(routine.Module);
            }

            if (auto const blame = m_routineBlame.find(static_cast<int32_t>(index)); blame != m_routineBlame.end())
            {
                driver.LateMoments += blame->second.Count;
                driver.WorstWaitTicks = (std::max)(driver.WorstWaitTicks, blame->second.WorstTicks);
            }
        }

        // threads that held a processor, charged to the module they were running
        auto const driverFor = [&positions, &drivers](int32_t const module) -> DriverStats&
            {
                auto [position, added] = positions.try_emplace(module, drivers.size());

                if (added)
                {
                    DriverStats driver{};
                    driver.Module = module;
                    drivers.push_back(std::move(driver));
                }

                return drivers[position->second];
            };

        for (auto const& [module, stats] : m_raisedIrql)
        {
            driverFor(module).RaisedIrql.Merge(stats);
        }

        for (auto const& [module, blame] : m_moduleBlame)
        {
            auto& driver = driverFor(module);

            driver.LateMoments += blame.Count;
            driver.WorstWaitTicks = (std::max)(driver.WorstWaitTicks, blame.WorstTicks);
        }

        for (auto& driver : drivers)
        {
            std::sort(driver.TicksPerProcessor.begin(), driver.TicksPerProcessor.end());
        }

        // the driver that held a processor longest first, then the busiest
        std::sort(drivers.begin(), drivers.end(), [](DriverStats const& left, DriverStats const& right)
            {
                auto const leftLongest = (std::max)({ left.Dpc.MaxTicks, left.Isr.MaxTicks, left.RaisedIrql.MaxTicks });
                auto const rightLongest = (std::max)({ right.Dpc.MaxTicks, right.Isr.MaxTicks, right.RaisedIrql.MaxTicks });

                if (leftLongest != rightLongest)
                {
                    return leftLongest > rightLongest;
                }

                return left.Dpc.TotalTicks + left.Isr.TotalTicks + left.RaisedIrql.TotalTicks >
                    right.Dpc.TotalTicks + right.Isr.TotalTicks + right.RaisedIrql.TotalTicks;
            });
    }
}
