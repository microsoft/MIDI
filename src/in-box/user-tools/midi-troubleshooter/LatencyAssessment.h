// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

// What a DPC latency check means for the person who ran it: is this PC fine for real-time audio,
// which drivers held it up, and what's worth trying. This decides which of those things to say
// and works out the numbers. The words themselves come from the app's resources.

#include "LatencyModel.h"

#include <vector>

namespace miditroubleshooter::latency
{
    // Shorter than this, a DPC, an interrupt or a hold-up doesn't matter for audio, even with
    // the smallest buffers people use.
    inline constexpr uint64_t CautionMicroseconds{ 500 };

    // This long or longer is enough to make audio drop out at the buffer sizes people use to
    // play live, such as 64 or 128 samples.
    inline constexpr uint64_t ProblemMicroseconds{ LateMomentMicroseconds };

    // the most drivers the plain-words result names
    inline constexpr size_t MaximumCulprits{ 5 };

    enum class Verdict : uint8_t
    {
        // nothing ran long enough to hold audio up
        Good,

        // long enough to hold up audio with the very smallest buffers
        Caution,

        // long enough to make audio drop out at buffer sizes people use to play live
        Problem,

        // the trace didn't have the events the check needs
        NotEnoughData
    };

    Verdict VerdictFor(_In_ uint64_t const microseconds) noexcept;

    // A driver that held up audio, MIDI or the test thread, or one that only ran long.
    struct Culprit
    {
        // index into Results::Drivers
        size_t Driver{ 0 };

        // its longest single DPC or interrupt, or hold of a processor outside of them
        uint64_t LongestMicroseconds{ 0 };
        CauseKind LongestKind{ CauseKind::Dpc };
        uint32_t LongestProcessor{ 0 };

        // late moments it was the biggest cause of, and the longest of them
        uint64_t LateMoments{ 0 };
        uint64_t WorstWaitMicroseconds{ 0 };

        // how bad the longest hold-up it caused was
        Verdict Severity{ Verdict::Good };
    };

    struct Assessment
    {
        // Decided only by what the check saw being held up: audio, the MIDI service or the test
        // thread. A long DPC on its own doesn't count against the PC.
        Verdict Overall{ Verdict::NotEnoughData };

        double Seconds{ 0.0 };

        // Audio threads and the test thread: how often they were held up for
        // LateMomentMicroseconds or more, and the longest hold-up of any length.
        uint64_t LateMoments{ 0 };
        uint64_t LongestHoldUpMicroseconds{ 0 };

        // the same for the MIDI service's threads
        uint64_t MidiLateMoments{ 0 };
        uint64_t MidiLongestHoldUpMicroseconds{ 0 };

        // the longest single DPC or interrupt, from any driver, for the technical report
        uint64_t LongestRoutineMicroseconds{ 0 };

        // The test thread's timer went off late while the processor that runs it took no profile
        // samples, which points at firmware or virtualization. How often, and the longest.
        uint64_t UnseenStalls{ 0 };
        uint64_t LongestUnseenStallMicroseconds{ 0 };

        // drivers that held something up, worst first
        std::vector<Culprit> Culprits{};

        // Drivers that ran for CautionMicroseconds or more but held nothing up for long, longest
        // first. They're only worth a look when glitches go on.
        std::vector<Culprit> RanLong{};

        // one piece of advice for each kind of driver among the culprits, in the same order
        std::vector<DriverKind> DriverAdvice{};

        // processors were asleep when audio needed them
        bool AdvisePowerMode{ false };

        // another program kept an audio thread from running
        bool AdviseBusyPrograms{ false };

        // an audio thread waited for the disk
        bool AdviseDiskWaits{ false };

        bool HadAudioApp{ false };
        bool HadProbe{ false };
        bool HadMidiService{ false };

        // no audio app, no MIDI service and no test thread, so nothing could show a hold-up
        bool NothingToWatch{ false };

        // Audio or the test thread was held up this long, so buffers shorter than this can drop
        // out. Zero when no hold-up mattered.
        uint64_t RiskyBufferMicroseconds{ 0 };

        // what the trace was missing, which limits what the check could tell
        bool MissingDpcs{ false };
        bool MissingScheduling{ false };
        bool MissingSamples{ false };
        bool LostEvents{ false };

        // the check ran out of memory while working out the results
        bool AnalysisIncomplete{ false };
    };

    Assessment Assess(_In_ Results const& results);

    // The number of samples a buffer this long holds at 48 kHz, the rate most audio apps use.
    inline uint64_t SamplesAt48Kilohertz(_In_ uint64_t const microseconds) noexcept
    {
        return (microseconds * 48 + 999) / 1000;
    }
}
