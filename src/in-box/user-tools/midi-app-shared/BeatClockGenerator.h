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
    };

    // Timing accuracy comes from the service, not from this thread: pulses are sent ahead of time
    // with the timestamp they are meant to play at, and the service schedules them. All this
    // thread has to do is keep the queue topped up.
    class BeatClockGenerator
    {
    public:
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

        // Takes effect at the first pulse that has not been scheduled yet, which is up to the
        // lookahead away. The pulse already on its way keeps its timestamp, so the beat does
        // not jump when the tempo changes underneath it.
        void BeatsPerMinute(_In_ double value) noexcept;
        double BeatsPerMinute() const noexcept;

        uint64_t PulsesScheduled() const noexcept { return m_pulsesScheduled.load(); }
        uint64_t TicksPerPulse() const noexcept;

        // Lead time for a synchronized start, in MIDI clock ticks.
        static uint64_t SuggestedStartLeadTicks() noexcept;

    private:
        void ThreadWorker();
        void SendToAllGroups(_In_ uint64_t timestamp, _In_ uint32_t const* words) noexcept;

        double TicksPerPulseForTempo(_In_ double beatsPerMinute) const noexcept;

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

        uint64_t m_startOriginTimestamp{ 0 };

        std::thread m_worker;
        std::atomic<bool> m_running{ false };
        std::atomic<uint64_t> m_pulsesScheduled{ 0 };
        std::atomic<uint64_t> m_lastScheduledTimestamp{ 0 };
    };
}
