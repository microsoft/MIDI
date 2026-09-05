// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

namespace midi2console
{
    struct ClockGeneratorOptions
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
    class ClockGenerator
    {
    public:
        ClockGenerator(
            _In_ midi2::MidiEndpointConnection const& connection,
            _In_ ClockGeneratorOptions options);

        ~ClockGenerator();

        ClockGenerator(ClockGenerator const&) = delete;
        ClockGenerator& operator=(ClockGenerator const&) = delete;

        void Start();

        // Returns the timestamp the last scheduled pulse plays at, so a caller can wait it out.
        uint64_t Stop();

        uint64_t PulsesScheduled() const noexcept { return m_pulsesScheduled.load(); }
        uint64_t TicksPerPulse() const noexcept { return static_cast<uint64_t>(m_ticksPerPulse); }

    private:
        void ThreadWorker();
        void SendToAllGroups(_In_ uint64_t timestamp, _In_ uint32_t const* words);

        midi2::MidiEndpointConnection m_connection{ nullptr };
        ClockGeneratorOptions m_options;

        std::vector<uint32_t> m_clockWords;
        std::vector<uint32_t> m_startWords;
        std::vector<uint32_t> m_stopWords;

        // Kept fractional: at 10 MHz a whole-tick interval loses a third of a tick per pulse,
        // which is milliseconds of drift over a long session.
        double m_ticksPerPulse{ 0.0 };

        std::thread m_worker;
        std::mutex m_mutex;
        std::condition_variable m_wakeup;
        bool m_stopRequested{ false };

        std::atomic<uint64_t> m_pulsesScheduled{ 0 };
        std::atomic<uint64_t> m_lastScheduledTimestamp{ 0 };
    };
}
