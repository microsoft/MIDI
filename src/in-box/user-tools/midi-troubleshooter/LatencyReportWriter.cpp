// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#include "LatencyReportWriter.h"
#include "LatencyDriverNames.h"

#include <windows.h>

#include <algorithm>
#include <cwctype>
#include <format>
#include <string_view>

namespace miditroubleshooter::latency
{
    namespace
    {
        // the same as MIDIDIAG_MAX_FIELD_LABEL_WIDTH
        constexpr size_t LabelWidth{ 30 };

        // the drivers that get a record of their own, worst first
        constexpr size_t MaximumDriverDetails{ 15 };

        // the routines listed for one driver, longest first
        constexpr size_t MaximumRoutinesPerDriver{ 8 };

        // the modules listed for one routine's samples
        constexpr size_t MaximumSampledModules{ 6 };

        std::wstring Lower(_In_ std::wstring_view const text)
        {
            std::wstring lower{ text };

            for (auto& character : lower)
            {
                character = static_cast<wchar_t>(std::towlower(character));
            }

            return lower;
        }

        // Control characters would break the one-field-a-line layout.
        std::wstring CleanText(_In_ std::wstring_view const text)
        {
            std::wstring cleaned{ text };

            for (auto& character : cleaned)
            {
                if (character < 0x20 || character == 0x7F || (character >= 0x80 && character <= 0x9F))
                {
                    character = L'?';
                }
            }

            return cleaned;
        }

        // The customer's own folder name is usually their name, so it never goes in a report.
        std::wstring RemoveUserProfile(_In_ std::wstring_view const text)
        {
            std::wstring result{ text };

            wchar_t profile[MAX_PATH]{};
            auto const length = GetEnvironmentVariableW(L"USERPROFILE", profile, MAX_PATH);

            if (length == 0 || length >= MAX_PATH)
            {
                return result;
            }

            std::wstring_view const profileText{ profile, length };
            auto const lowerResult = Lower(result);
            auto const lowerProfile = Lower(profileText);

            if (auto const found = lowerResult.find(lowerProfile); found != std::wstring::npos)
            {
                result.replace(found, profileText.size(), L"%USERPROFILE%");
            }

            return result;
        }

        std::wstring QuoteIfNeeded(_In_ std::wstring const& value)
        {
            if (!value.empty() && value.find_first_of(L" =\"") == std::wstring::npos)
            {
                return value;
            }

            std::wstring quoted{ L"\"" };

            for (auto const character : value)
            {
                quoted += character;

                if (character == L'"')
                {
                    quoted += L'"';
                }
            }

            quoted += L'"';

            return quoted;
        }

        // key=value pairs, quoted the way the format's rules say
        class Pairs
        {
        public:
            Pairs& Add(_In_ std::wstring_view const key, _In_ std::wstring_view const value)
            {
                if (!m_text.empty())
                {
                    m_text += L' ';
                }

                m_text += key;
                m_text += L'=';
                m_text += QuoteIfNeeded(CleanText(RemoveUserProfile(value)));

                return *this;
            }

            Pairs& Number(_In_ std::wstring_view const key, _In_ uint64_t const value)
            {
                return Add(key, std::to_wstring(value));
            }

            // a table leaves the cell blank, rather than showing two quote marks
            Pairs& AddIfNotEmpty(_In_ std::wstring_view const key, _In_ std::wstring_view const value)
            {
                return value.empty() ? *this : Add(key, value);
            }

            Pairs& Bool(_In_ std::wstring_view const key, _In_ bool const value)
            {
                return Add(key, value ? L"true" : L"false");
            }

            std::wstring const& Text() const noexcept { return m_text; }

        private:
            std::wstring m_text{};
        };

        class ReportBuilder
        {
        public:
            void Line(_In_ std::wstring_view const text)
            {
                m_text += text;
                m_text += L"\r\n";
            }

            void Section(_In_ std::wstring_view const name)
            {
                if (!m_text.empty())
                {
                    Line(L"");
                }

                m_text += L"== ";
                Line(name);
            }

            // separates one record of a section from the next
            void EndRecord()
            {
                Line(L"");
            }

            void Raw(_In_ std::wstring_view const label, _In_ std::wstring_view const value)
            {
                std::wstring line{ label };

                if (line.size() < LabelWidth)
                {
                    line.append(LabelWidth - line.size(), L' ');
                }

                line += L" : ";
                line += value;

                Line(line);
            }

            // a value that came from outside, such as a file description or a thread name
            void Text(_In_ std::wstring_view const label, _In_ std::wstring_view const value)
            {
                auto cleaned = CleanText(RemoveUserProfile(value));

                // one that looks like pairs would be read back as pairs
                if (cleaned.find(L'=') != std::wstring::npos)
                {
                    cleaned = QuoteIfNeeded(cleaned);
                }

                Raw(label, cleaned);
            }

            void Number(_In_ std::wstring_view const label, _In_ uint64_t const value)
            {
                Raw(label, std::to_wstring(value));
            }

            void Bool(_In_ std::wstring_view const label, _In_ bool const value)
            {
                Raw(label, value ? L"true" : L"false");
            }

            void Fields(_In_ std::wstring_view const label, _In_ Pairs const& pairs)
            {
                Raw(label, pairs.Text());
            }

            std::wstring Take() noexcept { return std::move(m_text); }

        private:
            std::wstring m_text{};
        };

        std::wstring ModuleName(_In_ Results const& results, _In_ int32_t const index)
        {
            auto const* const module = results.Module(index);

            return module != nullptr ? module->FileName : std::wstring{ L"unknown" };
        }

        std::wstring Address(_In_ Results const& results, _In_ uint64_t const address, _In_ int32_t const moduleIndex)
        {
            if (auto const* const module = results.Module(moduleIndex); module != nullptr && address >= module->Base)
            {
                return std::format(L"{}+0x{:x}", module->FileName, address - module->Base);
            }

            return std::format(L"0x{:x}", address);
        }

        std::wstring SecondsIntoTrace(_In_ Results const& results, _In_ uint64_t const time)
        {
            if (results.Trace.Frequency == 0 || time < results.Trace.StartTime)
            {
                return L"0.000";
            }

            return std::format(L"{:.3f}", static_cast<double>(time - results.Trace.StartTime) / static_cast<double>(results.Trace.Frequency));
        }

        std::wstring ProcessName(_In_ Results const& results, _In_ uint32_t const processId)
        {
            auto const* const process = results.Process(processId);

            return process != nullptr ? process->Name : std::wstring{};
        }

        std::wstring_view InterruptKindName(_In_ InterruptKind const kind) noexcept
        {
            switch (kind)
            {
            case InterruptKind::ThreadedDpc: return L"threaded_dpc";
            case InterruptKind::TimerDpc: return L"timer_dpc";
            case InterruptKind::Isr: return L"isr";
            case InterruptKind::Dpc:
            default: return L"dpc";
            }
        }

        std::wstring_view CauseKindName(_In_ CauseKind const kind) noexcept
        {
            switch (kind)
            {
            case CauseKind::Dpc: return L"dpc";
            case CauseKind::Isr: return L"isr";
            case CauseKind::Thread: return L"thread";
            case CauseKind::ProcessorIdle: return L"processor_idle";
            case CauseKind::RaisedIrql: return L"raised_irql";
            case CauseKind::Unknown:
            default: return L"unknown";
            }
        }

        Pairs Histogram(_In_ DurationHistogram const& histogram, _In_ Pairs pairs = {})
        {
            for (size_t index = 0; index < histogram.size(); ++index)
            {
                auto const key = index < DurationBucketLimits.size() ?
                    std::format(L"up_to_{}_us", DurationBucketLimits[index]) :
                    std::format(L"over_{}_us", DurationBucketLimits.back());

                pairs.Number(key, histogram[index]);
            }

            return pairs;
        }

        Pairs Durations(_In_ Results const& results, _In_ DurationStats const& stats)
        {
            Pairs pairs{};
            pairs.Number(L"count", stats.Count)
                .Number(L"total_us", results.Microseconds(stats.TotalTicks))
                .Number(L"max_us", results.Microseconds(stats.MaxTicks));

            if (stats.Count > 0)
            {
                pairs.Number(L"max_processor", stats.MaxProcessor)
                    .Add(L"max_at_s", SecondsIntoTrace(results, stats.MaxTime));
            }

            return pairs;
        }

        void WriteCheckSection(_Inout_ ReportBuilder& report, _In_ Results const& results, _In_ ReportContext const& context)
        {
            report.Section(L"latency_check");

            report.Raw(L"source", results.Trace.Live ? L"live_check" : L"saved_trace");
            report.Text(L"trace_file", results.Trace.Live ? context.SavedTracePath : results.Trace.SourcePath);
            report.Raw(L"duration_s", std::format(L"{:.1f}", results.Seconds()));

            if (results.Trace.Live)
            {
                report.Text(L"os_version", context.OsVersion);
                report.Text(L"architecture", context.Architecture);
                report.Text(L"processor_name", context.ProcessorName);
            }

            report.Number(L"logical_processors", results.Trace.ProcessorCount);
            report.Number(L"windows_build", results.Trace.WindowsBuild);
        }

        void WriteFindings(_Inout_ ReportBuilder& report, _In_ ReportContext const& context)
        {
            report.Section(L"findings");
            report.Number(L"finding_count", context.Findings.size());

            for (auto const& finding : context.Findings)
            {
                report.Fields(L"finding", Pairs{}.Add(L"id", finding.Id).Add(L"text", finding.Text));
            }
        }

        void WriteSummary(_Inout_ ReportBuilder& report, _In_ Results const& results, _In_ Assessment const& assessment)
        {
            report.Section(L"latency_summary");

            report.Raw(L"verdict", VerdictName(assessment.Overall));
            report.Number(L"late_moment_threshold_us", LateMomentMicroseconds);
            report.Number(L"late_moments", assessment.LateMoments);
            report.Number(L"longest_hold_up_us", assessment.LongestHoldUpMicroseconds);
            report.Number(L"midi_late_moments", assessment.MidiLateMoments);
            report.Number(L"midi_longest_hold_up_us", assessment.MidiLongestHoldUpMicroseconds);
            report.Number(L"longest_dpc_or_isr_us", assessment.LongestRoutineMicroseconds);
            report.Number(L"unseen_stalls", assessment.UnseenStalls);
            report.Number(L"longest_unseen_stall_us", assessment.LongestUnseenStallMicroseconds);
            report.Number(L"risky_buffer_us", assessment.RiskyBufferMicroseconds);
            report.Number(L"risky_buffer_samples_48k", SamplesAt48Kilohertz(assessment.RiskyBufferMicroseconds));
            report.Bool(L"audio_app_checked", assessment.HadAudioApp);
            report.Bool(L"test_thread_checked", assessment.HadProbe);
            report.Bool(L"nothing_to_watch", assessment.NothingToWatch);
            report.Bool(L"midi_service_checked", assessment.HadMidiService);
            report.Bool(L"missing_scheduling_events", assessment.MissingScheduling);
            report.Bool(L"missing_samples", assessment.MissingSamples);
            report.Bool(L"events_lost", assessment.LostEvents);

            size_t rank{ 0 };

            for (auto const& culprit : assessment.Culprits)
            {
                auto const& driver = results.Drivers[culprit.Driver];
                auto const* const module = results.Module(driver.Module);

                report.Fields(L"culprit", Pairs{}
                    .Number(L"rank", ++rank)
                    .Add(L"driver", ModuleName(results, driver.Module))
                    .Add(L"kind", names::KindName(module != nullptr ? module->Kind : DriverKind::Unknown))
                    .Add(L"severity", VerdictName(culprit.Severity))
                    .Number(L"longest_us", culprit.LongestMicroseconds)
                    .Add(L"longest_type", CauseKindName(culprit.LongestKind))
                    .Number(L"processor", culprit.LongestProcessor)
                    .Number(L"late_moments", culprit.LateMoments)
                    .Number(L"worst_wait_us", culprit.WorstWaitMicroseconds)
                    .AddIfNotEmpty(L"company", module != nullptr ? module->Company : std::wstring{}));
            }

            // ran long, but held nothing up that the check watched
            for (auto const& entry : assessment.RanLong)
            {
                auto const& driver = results.Drivers[entry.Driver];
                auto const* const module = results.Module(driver.Module);

                report.Fields(L"ran_long", Pairs{}
                    .Add(L"driver", ModuleName(results, driver.Module))
                    .Add(L"kind", names::KindName(module != nullptr ? module->Kind : DriverKind::Unknown))
                    .Number(L"longest_us", entry.LongestMicroseconds)
                    .Add(L"longest_type", CauseKindName(entry.LongestKind))
                    .Number(L"processor", entry.LongestProcessor)
                    .AddIfNotEmpty(L"company", module != nullptr ? module->Company : std::wstring{}));
            }

            for (auto const kind : assessment.DriverAdvice)
            {
                report.Fields(L"advice", Pairs{}.Add(L"kind", names::KindName(kind)));
            }

            if (assessment.AdvisePowerMode)
            {
                report.Fields(L"advice", Pairs{}.Add(L"kind", L"power_mode"));
            }

            if (assessment.AdviseBusyPrograms)
            {
                report.Fields(L"advice", Pairs{}.Add(L"kind", L"busy_programs"));
            }

            if (assessment.AdviseDiskWaits)
            {
                report.Fields(L"advice", Pairs{}.Add(L"kind", L"disk_waits"));
            }
        }

        void WriteDrivers(_Inout_ ReportBuilder& report, _In_ Results const& results)
        {
            report.Section(L"latency_drivers");
            report.Number(L"driver_count", results.Drivers.size());

            for (auto const& driver : results.Drivers)
            {
                auto const* const module = results.Module(driver.Module);

                std::wstring frameworks{};

                for (auto const framework : driver.Frameworks)
                {
                    frameworks += (frameworks.empty() ? L"" : L",") + ModuleName(results, framework);
                }

                Pairs pairs{};
                pairs.Add(L"name", ModuleName(results, driver.Module))
                    .Add(L"kind", names::KindName(module != nullptr ? module->Kind : DriverKind::Unknown))
                    .Number(L"longest_us", results.Microseconds((std::max)({ driver.Dpc.MaxTicks, driver.Isr.MaxTicks, driver.RaisedIrql.MaxTicks })))
                    .Number(L"dpcs", driver.Dpc.Count)
                    .Number(L"dpc_max_us", results.Microseconds(driver.Dpc.MaxTicks))
                    .Number(L"dpc_total_us", results.Microseconds(driver.Dpc.TotalTicks))
                    .Number(L"isrs", driver.Isr.Count)
                    .Number(L"isr_max_us", results.Microseconds(driver.Isr.MaxTicks))
                    .Number(L"isr_total_us", results.Microseconds(driver.Isr.TotalTicks));

                // a thread running its code held a processor where timers and DPCs wait
                if (driver.RaisedIrql.Count > 0)
                {
                    pairs.Number(L"raised_irql_holds", driver.RaisedIrql.Count)
                        .Number(L"raised_irql_max_us", results.Microseconds(driver.RaisedIrql.MaxTicks))
                        .Number(L"raised_irql_total_us", results.Microseconds(driver.RaisedIrql.TotalTicks));
                }

                pairs.Number(L"late_moments", driver.LateMoments)
                    .Number(L"worst_wait_us", results.Microseconds(driver.WorstWaitTicks))
                    .AddIfNotEmpty(L"runs_inside", frameworks);

                report.Fields(L"driver", pairs);
            }
        }

        void WriteDriverDetails(_Inout_ ReportBuilder& report, _In_ Results const& results)
        {
            report.Section(L"latency_driver_details");

            auto const count = (std::min)(results.Drivers.size(), MaximumDriverDetails);

            for (size_t index = 0; index < count; ++index)
            {
                auto const& driver = results.Drivers[index];
                auto const* const module = results.Module(driver.Module);

                if (index > 0)
                {
                    report.EndRecord();
                }

                report.Text(L"name", ModuleName(results, driver.Module));
                report.Raw(L"kind", names::KindName(module != nullptr ? module->Kind : DriverKind::Unknown));

                if (module != nullptr)
                {
                    report.Text(L"company", module->Company);
                    report.Text(L"description", module->Description);
                    report.Text(L"file_version", module->Version);
                    report.Text(L"path", module->LocalPath);
                    report.Text(L"trace_path", module->TracePath);
                    report.Bool(L"same_file_as_trace", module->LocalFileMatches);
                    report.Bool(L"runs_other_drivers_code", module->IsFramework);
                    report.Text(L"device_class", module->DeviceClass);

                    for (auto const& device : module->Devices)
                    {
                        report.Fields(L"device", Pairs{}.Add(L"name", device));
                    }
                }

                for (auto const framework : driver.Frameworks)
                {
                    report.Fields(L"runs_inside", Pairs{}.Add(L"name", ModuleName(results, framework)));
                }

                report.Fields(L"dpc", Durations(results, driver.Dpc));
                report.Fields(L"dpc_histogram", Histogram(driver.Dpc.Histogram));
                report.Fields(L"isr", Durations(results, driver.Isr));
                report.Fields(L"isr_histogram", Histogram(driver.Isr.Histogram));
                report.Fields(L"late_moments", Pairs{}
                    .Number(L"count", driver.LateMoments)
                    .Number(L"worst_wait_us", results.Microseconds(driver.WorstWaitTicks)));

                for (auto const& [processor, ticks] : driver.TicksPerProcessor)
                {
                    report.Fields(L"processor_time", Pairs{}
                        .Number(L"processor", processor)
                        .Number(L"total_us", results.Microseconds(ticks)));
                }

                // this driver's routines, longest first
                std::vector<RoutineStats const*> routines{};

                for (auto const& routine : results.Routines)
                {
                    if (routine.Driver == driver.Module)
                    {
                        routines.push_back(&routine);
                    }
                }

                std::sort(routines.begin(), routines.end(), [](RoutineStats const* left, RoutineStats const* right)
                    {
                        return left->Durations.MaxTicks > right->Durations.MaxTicks;
                    });

                if (routines.size() > MaximumRoutinesPerDriver)
                {
                    routines.resize(MaximumRoutinesPerDriver);
                }

                for (auto const* const routine : routines)
                {
                    auto const how = routine->ClientModule >= 0 ? L"framework_event" :
                        routine->DriverFromSamples ? L"samples" : L"routine";

                    report.Fields(L"routine", Pairs{}
                        .Add(L"address", Address(results, routine->Routine, routine->Module))
                        .AddIfNotEmpty(L"client", routine->ClientRoutine != 0 ? Address(results, routine->ClientRoutine, routine->ClientModule) : std::wstring{})
                        .Add(L"type", InterruptKindName(routine->Kind))
                        .Number(L"count", routine->Durations.Count)
                        .Number(L"max_us", results.Microseconds(routine->Durations.MaxTicks))
                        .Number(L"total_us", results.Microseconds(routine->Durations.TotalTicks))
                        .Add(L"charged_by", how));
                }

                for (auto const* const routine : routines)
                {
                    auto samples = routine->SampledModules;

                    std::sort(samples.begin(), samples.end(), [](auto const& left, auto const& right) { return left.second > right.second; });

                    if (samples.size() > MaximumSampledModules)
                    {
                        samples.resize(MaximumSampledModules);
                    }

                    for (auto const& [sampledModule, sampleCount] : samples)
                    {
                        report.Fields(L"sampled_code", Pairs{}
                            .Add(L"routine", Address(results, routine->Routine, routine->Module))
                            .Add(L"module", ModuleName(results, sampledModule))
                            .Number(L"samples", sampleCount));
                    }
                }
            }
        }

        void WriteLateMoments(_Inout_ ReportBuilder& report, _In_ Results const& results)
        {
            report.Section(L"latency_late_moments");

            bool first{ true };

            for (auto const& moment : results.LongestWaits)
            {
                if (!first)
                {
                    report.EndRecord();
                }

                first = false;

                auto const microseconds = results.Microseconds(moment.WaitTicks);
                auto const at = SecondsIntoTrace(results, moment.StartTime);

                report.Raw(L"name", std::format(L"{} us at {} s", microseconds, at));
                report.Raw(L"type", MomentKindName(moment.Kind));
                report.Number(L"hold_up_us", microseconds);
                report.Raw(L"at_s", at);
                report.Bool(L"late", microseconds >= LateMomentMicroseconds);
                report.Number(L"processor", moment.Processor);
                report.Number(L"ready_processor", moment.ReadyProcessor);

                AudioThreadKind threadKind{ AudioThreadKind::Audio };

                for (auto const& audio : results.AudioThreads)
                {
                    if (audio.ThreadId == moment.ThreadId)
                    {
                        threadKind = audio.Kind;
                        break;
                    }
                }

                report.Fields(L"thread", Pairs{}
                    .Add(L"process", ProcessName(results, moment.ProcessId))
                    .Number(L"pid", moment.ProcessId)
                    .Number(L"tid", moment.ThreadId)
                    .Add(L"kind", ThreadKindName(threadKind)));

                for (auto const& cause : moment.Causes)
                {
                    Pairs pairs{};
                    pairs.Add(L"type", CauseKindName(cause.Kind))
                        .Number(L"us", results.Microseconds(cause.Ticks));

                    if (cause.Kind == CauseKind::Dpc || cause.Kind == CauseKind::Isr)
                    {
                        auto const* const routine = cause.Routine >= 0 && static_cast<size_t>(cause.Routine) < results.Routines.size() ?
                            &results.Routines[static_cast<size_t>(cause.Routine)] : nullptr;

                        pairs.Add(L"driver", ModuleName(results, cause.Driver))
                            .Add(L"routine", routine != nullptr ? Address(results, routine->Routine, routine->Module) : std::wstring{});
                    }
                    else if (cause.Kind == CauseKind::Thread)
                    {
                        pairs.Add(L"process", ProcessName(results, cause.ProcessId))
                            .Number(L"pid", cause.ProcessId)
                            .Number(L"tid", cause.ThreadId)
                            .Number(L"priority", static_cast<uint64_t>((std::max)(cause.Priority, 0)));
                    }
                    else if (cause.Kind == CauseKind::RaisedIrql)
                    {
                        pairs.Add(L"driver", ModuleName(results, cause.Driver))
                            .Add(L"process", ProcessName(results, cause.ProcessId))
                            .Number(L"pid", cause.ProcessId)
                            .Number(L"tid", cause.ThreadId);
                    }
                    else if (cause.Kind == CauseKind::ProcessorIdle)
                    {
                        pairs.Number(L"idle_state", cause.IdleState);
                    }

                    report.Fields(L"cause", pairs);
                }
            }
        }

        Pairs ThreadPairs(_In_ Results const& results, _In_ AudioThreadStats const& audio)
        {
            Pairs pairs{};

            pairs.Add(L"process", ProcessName(results, audio.ProcessId))
                .Number(L"pid", audio.ProcessId)
                .Number(L"tid", audio.ThreadId)
                .AddIfNotEmpty(L"name", audio.ThreadName)
                .Add(L"kind", ThreadKindName(audio.Kind))
                .Number(L"priority", static_cast<uint64_t>((std::max)(audio.TypicalPriority, 0)))
                .Number(L"highest_priority", static_cast<uint64_t>((std::max)(audio.HighestPriority, 0)))
                .Number(L"runs", audio.Runs)
                .Number(L"realtime_runs", audio.RealTimeRuns);

            return pairs;
        }

        void WriteAudioThreads(_Inout_ ReportBuilder& report, _In_ Results const& results)
        {
            report.Section(L"latency_audio_threads");
            report.Number(L"thread_count", results.AudioThreads.size());

            for (auto const& audio : results.AudioThreads)
            {
                auto pairs = ThreadPairs(results, audio);

                pairs.Number(L"waits", audio.Waits.Count)
                    .Number(L"wait_max_us", results.Microseconds(audio.Waits.MaxTicks))
                    .Number(L"interruptions", audio.Interruptions.Count)
                    .Number(L"interruption_max_us", results.Microseconds(audio.Interruptions.MaxTicks))
                    .Number(L"late_wakes", audio.LateWakes.Count)
                    .Number(L"late_wake_max_us", results.Microseconds(audio.LateWakes.MaxTicks));

                // the test thread's late wakes that nothing in the trace explains
                if (audio.UnseenStalls.Count > 0)
                {
                    pairs.Number(L"unseen_stalls", audio.UnseenStalls.Count)
                        .Number(L"unseen_stall_max_us", results.Microseconds(audio.UnseenStalls.MaxTicks))
                        .Number(L"unseen_stall_processor", audio.UnseenStalls.MaxProcessor);
                }

                pairs.Number(L"disk_waits", audio.HardFaults.Count)
                    .Number(L"disk_wait_max_us", results.Microseconds(audio.HardFaults.MaxTicks))
                    .Number(L"late_moments", audio.LateMoments)
                    .Number(L"worst_us", results.Microseconds(audio.WorstLateTicks));

                report.Fields(L"thread", pairs);
            }

            for (auto const& audio : results.AudioThreads)
            {
                Pairs first{};
                first.Number(L"tid", audio.ThreadId);

                report.Fields(L"wait_histogram", Histogram(audio.Waits.Histogram, std::move(first)));
            }
        }

        void WriteOtherThreads(_Inout_ ReportBuilder& report, _In_ Results const& results)
        {
            report.Section(L"latency_other_realtime_threads");
            report.Number(L"thread_count", results.OtherRealTimeThreadCount);

            for (auto const& other : results.OtherRealTimeThreads)
            {
                report.Fields(L"thread", ThreadPairs(results, other));
            }
        }

        void WriteProcessors(_Inout_ ReportBuilder& report, _In_ Results const& results)
        {
            report.Section(L"latency_processors");

            for (auto const& processor : results.Processors)
            {
                Pairs pairs{};
                pairs.Number(L"index", processor.Index);

                if (processor.EfficiencyClassKnown)
                {
                    pairs.Number(L"efficiency_class", processor.EfficiencyClass);
                }

                pairs.Number(L"dpcs", processor.Dpc.Count)
                    .Number(L"dpc_max_us", results.Microseconds(processor.Dpc.MaxTicks))
                    .Number(L"dpc_total_us", results.Microseconds(processor.Dpc.TotalTicks))
                    .Number(L"isrs", processor.Isr.Count)
                    .Number(L"isr_max_us", results.Microseconds(processor.Isr.MaxTicks))
                    .Number(L"isr_total_us", results.Microseconds(processor.Isr.TotalTicks))
                    .Number(L"samples", processor.Samples);

                report.Fields(L"processor", pairs);
            }
        }

        void WriteProbe(_Inout_ ReportBuilder& report, _In_ Results const& results)
        {
            auto const& probe = results.Probe;

            report.Section(L"latency_test_thread");
            report.Bool(L"ran", probe.Ran);

            if (probe.Error != 0)
            {
                report.Number(L"error", probe.Error);
            }

            if (!probe.Ran)
            {
                return;
            }

            report.Number(L"tid", probe.ThreadId);
            report.Bool(L"mmcss", probe.UsedMmcss);

            if (!probe.UsedMmcss)
            {
                report.Number(L"mmcss_error", probe.MmcssError);
            }

            report.Bool(L"high_resolution_timer", probe.HighResolutionTimer);
            report.Number(L"period_us", probe.PeriodMicroseconds);
            report.Number(L"wakes", probe.Wakes);
            report.Number(L"missed_wakes", probe.MissedWakes);
            report.Number(L"lateness_median_us", probe.MedianLatenessMicroseconds);
            report.Number(L"lateness_99th_percentile_us", probe.Percentile99LatenessMicroseconds);
            report.Number(L"lateness_max_us", results.Microseconds(probe.Lateness.MaxTicks));
            report.Fields(L"lateness_histogram", Histogram(probe.Lateness.Histogram));
        }

        void WriteTrace(_Inout_ ReportBuilder& report, _In_ Results const& results, _In_ ReportContext const& context)
        {
            auto const& trace = results.Trace;

            report.Section(L"latency_trace");

            report.Number(L"clock_frequency", trace.Frequency);
            report.Number(L"clock_type", trace.ClockType);
            report.Number(L"pointer_size", trace.PointerSize);
            report.Number(L"events", trace.EventsProcessed);
            report.Number(L"events_skipped", trace.EventsSkipped);
            report.Bool(L"analysis_incomplete", trace.AnalysisIncomplete);
            report.Number(L"events_lost", trace.EventsLost);
            report.Number(L"buffers_lost", trace.BuffersLost);
            report.Number(L"realtime_buffers_lost", trace.RealTimeBuffersLost);
            report.Number(L"dpcs", trace.Dpcs);
            report.Number(L"isrs", trace.Interrupts);
            report.Number(L"samples", trace.Samples);
            report.Number(L"context_switches", trace.ContextSwitches);
            report.Number(L"ready_threads", trace.ReadyThreads);
            report.Number(L"framework_events", trace.FrameworkEvents);
            report.Number(L"disk_waits", trace.HardFaults);
            report.Number(L"out_of_order", trace.OutOfOrder);
            report.Number(L"kernel_modules", results.Modules.size());
            report.Number(L"routines", results.Routines.size());

            if (context.Session)
            {
                auto const& session = *context.Session;

                report.Bool(L"samples_enabled", session.SamplesEnabled);

                if (session.ProfilePrivilegeError != 0)
                {
                    report.Number(L"profile_privilege_error", session.ProfilePrivilegeError);
                }

                report.Number(L"profile_interval_100ns", session.ProfileInterval);
                report.Bool(L"framework_events_enabled", session.FrameworkEventsEnabled);

                if (session.FrameworkEventsError != 0)
                {
                    report.Number(L"framework_events_error", session.FrameworkEventsError);
                }

                report.Bool(L"test_thread_events_enabled", session.ProbeProviderEnabled);
                report.Raw(L"group_mask", session.GroupMask);
                report.Bool(L"writing_file", session.WritingFile);
            }
        }
    }

    _Use_decl_annotations_
    std::wstring_view VerdictName(Verdict const verdict) noexcept
    {
        switch (verdict)
        {
        case Verdict::Good: return L"good";
        case Verdict::Caution: return L"caution";
        case Verdict::Problem: return L"problem";
        case Verdict::NotEnoughData:
        default: return L"not_enough_data";
        }
    }

    _Use_decl_annotations_
    std::wstring_view MomentKindName(MomentKind const kind) noexcept
    {
        switch (kind)
        {
        case MomentKind::Interrupted: return L"interrupted";
        case MomentKind::WokeLate: return L"woke_late";
        case MomentKind::WaitedToRun:
        default: return L"waited_to_run";
        }
    }

    _Use_decl_annotations_
    std::wstring_view ThreadKindName(AudioThreadKind const kind) noexcept
    {
        switch (kind)
        {
        case AudioThreadKind::Probe: return L"test_thread";
        case AudioThreadKind::MidiService: return L"midi_service";
        case AudioThreadKind::Audio:
        default: return L"audio";
        }
    }

    _Use_decl_annotations_
    std::wstring WriteReport(Results const& results, Assessment const& assessment, ReportContext const& context)
    {
        ReportBuilder report{};

        report.Line(ReportProductName);

        report.Section(L"header");
        report.Number(L"report_format_version", 2);
        report.Raw(L"report_type", ReportType);
        report.Text(L"ver_build_full", context.ToolVersion);
        report.Raw(L"current_time", context.CurrentTime);
        report.Bool(L"running_elevated", context.RunningElevated);

        WriteCheckSection(report, results, context);
        WriteFindings(report, context);
        WriteSummary(report, results, assessment);
        WriteDrivers(report, results);
        WriteDriverDetails(report, results);
        WriteLateMoments(report, results);
        WriteAudioThreads(report, results);
        WriteOtherThreads(report, results);
        WriteProcessors(report, results);
        WriteProbe(report, results);
        WriteTrace(report, results, context);

        report.Section(L"successful_run");
        report.Section(L"end_of_file");

        return report.Take();
    }
}
