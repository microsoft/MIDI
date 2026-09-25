// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <mutex>
#include <thread>
#include <vector>

namespace midiapp
{
    struct BeatClockGeneratorOptions
    {
        double BeatsPerMinute{ 120.0 };
        int PulsesPerQuarterNote{ 24 };
        std::vector<uint8_t> GroupIndexes;
        bool SendStartMessage{ false };
        bool SendStopMessage{ false };

        // The output rate against the tempo, as a ratio. 1/1 is the usual 24 pulses per quarter
        // note. 1/2 halves the rate, so the receiver runs at half speed; 2/1 doubles it. 3/2 is
        // the dotted feel and 3/1 is triplets.
        //
        // Every whole number ratio is exact here, including the odd and dotted ones, because
        // this is the clock source: pulse times are worked out from one origin in floating
        // point rather than counted out of somebody else's clock, so there is nothing to round.
        int ClockRatioNumerator{ 1 };
        int ClockRatioDenominator{ 1 };

        // 50 is straight. Higher lengthens the first of each pair of swung notes and shortens
        // the second by the same amount, so the pair, and with it the tempo, is unchanged.
        // 66.67 is the triplet feel most swing controls are calibrated against.
        double SwingPercent{ 50.0 };

        // What gets swung, as a division of the quarter note: 2 is eighth notes, 4 is sixteenths.
        int SwingSubdivision{ 2 };

        // Shifts this output against the others, for a device that answers late or a cable run
        // that costs time. Negative is earlier. Applied when the clock starts.
        double OffsetMilliseconds{ 0.0 };
    };

    // Timing accuracy comes from the service, not from this thread: pulses are sent ahead of time
    // with the timestamp they are meant to play at, and the service schedules them. All this
    // thread has to do is keep the queue topped up.
    class BeatClockGenerator
    {
    public:
        static constexpr double MinimumSwingPercent = 50.0;
        static constexpr double MaximumSwingPercent = 75.0;
        static constexpr double MaximumOffsetMilliseconds = 500.0;
        static constexpr int MaximumClockRatioPart = 64;

        BeatClockGenerator(
            _In_ winrt::Windows::Devices::Midi2::MidiEndpointConnection const& connection,
            _In_ BeatClockGeneratorOptions options);

        ~BeatClockGenerator();

        BeatClockGenerator(BeatClockGenerator const&) = delete;
        BeatClockGenerator& operator=(BeatClockGenerator const&) = delete;

        // originTimestamp is when the first pulse plays. Zero means as soon as the worker runs.
        // Handing several generators the same non-zero value is what puts them in step with
        // each other, so it has to be far enough ahead for all of them to have queued by then:
        // see SuggestedStartLeadTicks.
        void Start(_In_ uint64_t originTimestamp = 0);

        // Returns the timestamp of the last message scheduled, including the stop message, so
        // a caller can wait it out before closing the connection.
        uint64_t Stop();

        bool IsRunning() const noexcept { return m_running.load(); }

        // Each of these takes effect at the first pulse that has not been scheduled yet, which
        // is up to the lookahead away. The pulse already on its way keeps its timestamp, and the
        // swing phase carries over, so nothing jumps when a value changes underneath it.
        void BeatsPerMinute(_In_ double value) noexcept;
        double BeatsPerMinute() const noexcept;

        void ClockRatio(_In_ int numerator, _In_ int denominator) noexcept;
        void GetClockRatio(_Out_ int& numerator, _Out_ int& denominator) const noexcept;

        void SwingPercent(_In_ double value) noexcept;
        double SwingPercent() const noexcept;

        // Tempo with the ratio applied, which is the rate the receiving device will run at.
        double EffectiveBeatsPerMinute() const noexcept;

        uint64_t PulsesScheduled() const noexcept { return m_pulsesScheduled.load(); }
        uint64_t TicksPerPulse() const noexcept;

        // Lead time for a synchronized start, in MIDI clock ticks.
        static uint64_t SuggestedStartLeadTicks() noexcept;

        static uint64_t OffsetMillisecondsToTicks(_In_ double milliseconds) noexcept;

    private:
        // How the pulse timeline is bent to swing. All ones when swing is off, in which case
        // every pulse lands exactly where it would have without it.
        struct SwingShape
        {
            double PulsesPerHalf{ 0.0 };    // zero means straight
            double FirstScale{ 1.0 };
            double SecondScale{ 1.0 };
        };

        void ThreadWorker() noexcept;
        void SendToAllGroups(_In_ uint64_t timestamp, _In_ uint32_t const* words) noexcept;

        double TicksPerPulseForTempo(_In_ double beatsPerMinute) const noexcept;
        SwingShape BuildSwingShape() const noexcept;

        static double SwingPosition(_In_ uint64_t pulseIndex, _In_ SwingShape const& shape) noexcept;

        winrt::Windows::Devices::Midi2::MidiEndpointConnection m_connection{ nullptr };
        BeatClockGeneratorOptions m_options;

        std::vector<uint32_t> m_clockWords;
        std::vector<uint32_t> m_startWords;
        std::vector<uint32_t> m_stopWords;

        mutable std::mutex m_mutex;
        std::condition_variable m_wakeup;
        bool m_stopRequested{ false };

        // Kept fractional: at 10 MHz a whole-tick interval loses a third of a tick per pulse,
        // which is milliseconds of drift over a long session.
        double m_ticksPerPulse{ 0.0 };
        double m_requestedBeatsPerMinute{ 0.0 };

        // Bumped by any change to tempo, ratio or swing. The worker watches this one number
        // rather than comparing each value, so adding another timing control later does not
        // mean another comparison in the wait predicate.
        uint64_t m_timingGeneration{ 0 };

        uint64_t m_startOriginTimestamp{ 0 };

        std::thread m_worker;
        std::atomic<bool> m_running{ false };
        std::atomic<uint64_t> m_pulsesScheduled{ 0 };
        std::atomic<uint64_t> m_lastScheduledTimestamp{ 0 };
    };
}
