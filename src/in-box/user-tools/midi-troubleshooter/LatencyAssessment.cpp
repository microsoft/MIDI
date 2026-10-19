// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#include "LatencyAssessment.h"

#include <algorithm>

namespace miditroubleshooter::latency
{
    namespace
    {
        // A late moment is put down to its biggest cause when that was at least this much of it.
        constexpr uint64_t MainCauseSharePercent{ 40 };

        uint64_t LongestHoldUpTicks(_In_ AudioThreadStats const& audio) noexcept
        {
            return (std::max)({ audio.Waits.MaxTicks, audio.Interruptions.MaxTicks, audio.LateWakes.MaxTicks });
        }

        int Rank(_In_ Verdict const verdict) noexcept
        {
            switch (verdict)
            {
            case Verdict::Problem: return 2;
            case Verdict::Caution: return 1;
            case Verdict::Good: return 0;
            case Verdict::NotEnoughData:
            default: return -1;
            }
        }
    }

    _Use_decl_annotations_
    Verdict VerdictFor(uint64_t const microseconds) noexcept
    {
        if (microseconds >= ProblemMicroseconds)
        {
            return Verdict::Problem;
        }

        if (microseconds >= CautionMicroseconds)
        {
            return Verdict::Caution;
        }

        return Verdict::Good;
    }

    _Use_decl_annotations_
    Assessment Assess(Results const& results)
    {
        Assessment assessment{};
        assessment.Seconds = results.Seconds();

        auto const& trace = results.Trace;

        assessment.MissingDpcs = !trace.HasDpcs();
        assessment.MissingScheduling = !trace.HasScheduling();
        assessment.MissingSamples = trace.Samples == 0;
        assessment.LostEvents = trace.EventsLost > 0 || trace.BuffersLost > 0 || trace.RealTimeBuffersLost > 0;
        assessment.AnalysisIncomplete = trace.AnalysisIncomplete;

        // Results with parts missing can't be trusted to say a PC is fine.
        if (assessment.MissingDpcs || assessment.AnalysisIncomplete)
        {
            assessment.Overall = Verdict::NotEnoughData;
            return assessment;
        }

        for (auto const& audio : results.AudioThreads)
        {
            auto const longest = results.Microseconds(LongestHoldUpTicks(audio));

            if (audio.Kind == AudioThreadKind::MidiService)
            {
                assessment.HadMidiService = true;
                assessment.MidiLateMoments += audio.LateMoments;
                assessment.MidiLongestHoldUpMicroseconds = (std::max)(assessment.MidiLongestHoldUpMicroseconds, longest);

                continue;
            }

            if (audio.IsProbe())
            {
                assessment.HadProbe = true;
                assessment.UnseenStalls += audio.UnseenStalls.Count;
                assessment.LongestUnseenStallMicroseconds = (std::max)(assessment.LongestUnseenStallMicroseconds,
                    results.Microseconds(audio.UnseenStalls.MaxTicks));
            }
            else
            {
                assessment.HadAudioApp = true;
            }

            assessment.LateMoments += audio.LateMoments;
            assessment.LongestHoldUpMicroseconds = (std::max)(assessment.LongestHoldUpMicroseconds, longest);

            if (results.Microseconds(audio.HardFaults.MaxTicks) >= ProblemMicroseconds)
            {
                assessment.AdviseDiskWaits = true;
            }
        }

        std::vector<Culprit> culprits{};
        std::vector<Culprit> ranLong{};

        for (size_t index = 0; index < results.Drivers.size(); ++index)
        {
            auto const& driver = results.Drivers[index];

            Culprit culprit{};
            culprit.Driver = index;

            // the longest of its DPCs, its interrupts and its holds of a processor outside them
            auto const* longest = &driver.Dpc;
            culprit.LongestKind = CauseKind::Dpc;

            if (driver.Isr.MaxTicks > longest->MaxTicks)
            {
                longest = &driver.Isr;
                culprit.LongestKind = CauseKind::Isr;
            }

            if (driver.RaisedIrql.MaxTicks > longest->MaxTicks)
            {
                longest = &driver.RaisedIrql;
                culprit.LongestKind = CauseKind::RaisedIrql;
            }

            culprit.LongestMicroseconds = results.Microseconds(longest->MaxTicks);
            culprit.LongestProcessor = longest->MaxProcessor;
            culprit.LateMoments = driver.LateMoments;
            culprit.WorstWaitMicroseconds = results.Microseconds(driver.WorstWaitTicks);

            assessment.LongestRoutineMicroseconds = (std::max)(assessment.LongestRoutineMicroseconds,
                results.Microseconds((std::max)(driver.Dpc.MaxTicks, driver.Isr.MaxTicks)));

            // A driver only counts against the PC when something the check watched was waiting for
            // it. A long DPC often runs on a processor that audio isn't using at that moment.
            if (culprit.LateMoments > 0)
            {
                culprit.Severity = VerdictFor(culprit.WorstWaitMicroseconds);
                culprits.push_back(culprit);
            }
            else if (culprit.LongestMicroseconds >= CautionMicroseconds)
            {
                ranLong.push_back(culprit);
            }
        }

        std::stable_sort(culprits.begin(), culprits.end(), [](Culprit const& left, Culprit const& right)
            {
                if (Rank(left.Severity) != Rank(right.Severity))
                {
                    return Rank(left.Severity) > Rank(right.Severity);
                }

                if (left.LateMoments != right.LateMoments)
                {
                    return left.LateMoments > right.LateMoments;
                }

                return left.LongestMicroseconds > right.LongestMicroseconds;
            });

        if (culprits.size() > MaximumCulprits)
        {
            culprits.resize(MaximumCulprits);
        }

        std::stable_sort(ranLong.begin(), ranLong.end(), [](Culprit const& left, Culprit const& right)
            {
                return left.LongestMicroseconds > right.LongestMicroseconds;
            });

        if (ranLong.size() > MaximumCulprits)
        {
            ranLong.resize(MaximumCulprits);
        }

        for (auto const& culprit : culprits)
        {
            auto const* const module = results.Module(results.Drivers[culprit.Driver].Module);
            auto const kind = module != nullptr ? module->Kind : DriverKind::Unknown;

            if (std::find(assessment.DriverAdvice.begin(), assessment.DriverAdvice.end(), kind) == assessment.DriverAdvice.end())
            {
                assessment.DriverAdvice.push_back(kind);
            }
        }

        assessment.Culprits = std::move(culprits);
        assessment.RanLong = std::move(ranLong);

        // what else held audio up, from the late moments it was mostly to blame for
        for (auto const& moment : results.LongestWaits)
        {
            if (results.Microseconds(moment.WaitTicks) < ProblemMicroseconds || moment.Causes.empty())
            {
                continue;
            }

            auto const& cause = moment.Causes.front();

            if (cause.Ticks * 100 < moment.WaitTicks * MainCauseSharePercent)
            {
                continue;
            }

            if (cause.Kind == CauseKind::ProcessorIdle)
            {
                assessment.AdvisePowerMode = true;
            }
            else if (cause.Kind == CauseKind::Thread)
            {
                assessment.AdviseBusyPrograms = true;
            }
        }

        // Only what the check saw being held up decides the answer. With nothing to watch, it
        // can't tell, however long a DPC ran.
        assessment.NothingToWatch = results.AudioThreads.empty();

        if (assessment.MissingScheduling || assessment.NothingToWatch)
        {
            assessment.Overall = Verdict::NotEnoughData;
            return assessment;
        }

        assessment.Overall = VerdictFor((std::max)(assessment.LongestHoldUpMicroseconds, assessment.MidiLongestHoldUpMicroseconds));

        // buffer size is about audio, so this follows audio and the test thread, not MIDI
        assessment.RiskyBufferMicroseconds = assessment.LongestHoldUpMicroseconds >= CautionMicroseconds ?
            assessment.LongestHoldUpMicroseconds : 0;

        return assessment;
    }
}
