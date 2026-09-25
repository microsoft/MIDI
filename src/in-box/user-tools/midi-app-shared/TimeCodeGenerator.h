// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

#include "MidiTimeCode.h"

#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <mutex>
#include <thread>
#include <vector>

namespace midiapp
{
    struct TimeCodeGeneratorOptions
    {
        MidiTimeCodeFrameRate FrameRate{ MidiTimeCodeFrameRate::Frames30 };
        MidiTimeCodePosition StartPosition{};

        std::vector<uint8_t> GroupIndexes;

        // A full frame message parks a receiver at a position in one go, where quarter frames
        // take two frames to say the same thing. Sent as the clock starts and again as it stops,
        // which is what lets a receiver locate immediately instead of drifting in.
        bool SendFullFrameMessages{ true };

        // Shifts this output against the others, for a device that answers late. Negative is
        // earlier. Applied when the clock starts.
        double OffsetMilliseconds{ 0.0 };
    };

    // Sends MIDI Time Code. The same shape as BeatClockGenerator, and for the same reason:
    // timing accuracy comes from the service, not from this thread. Messages are handed over
    // ahead of time carrying the timestamp they are meant to play at, and all this thread does
    // is keep the queue topped up.
    //
    // Simpler than the beat clock in one way and harder in another. Simpler because the rate is
    // fixed: there is no tempo, no divider and no swing. Harder because the value being sent is
    // spread across eight messages and is deliberately two frames behind where the clock has
    // got to. See MidiTimeCode.h.
    class TimeCodeGenerator
    {
    public:
        TimeCodeGenerator(
            _In_ winrt::Windows::Devices::Midi2::MidiEndpointConnection const& connection,
            _In_ TimeCodeGeneratorOptions options);

        ~TimeCodeGenerator();

        TimeCodeGenerator(TimeCodeGenerator const&) = delete;
        TimeCodeGenerator& operator=(TimeCodeGenerator const&) = delete;

        // originTimestamp is when the first quarter frame plays. Zero means as soon as the
        // worker runs. Sharing a non-zero value with other generators is what starts a set of
        // them together; see BeatClockGenerator::SuggestedStartLeadTicks.
        void Start(_In_ uint64_t originTimestamp = 0);

        // Returns the timestamp of the last message scheduled, so a caller can wait it out
        // before closing the connection.
        uint64_t Stop();

        bool IsRunning() const noexcept { return m_running.load(); }

        MidiTimeCodeFrameRate FrameRate() const noexcept { return m_options.FrameRate; }

        // Where the timecode has reached. Safe to read from any thread.
        MidiTimeCodePosition CurrentPosition() const noexcept;

        uint64_t QuarterFramesScheduled() const noexcept { return m_quarterFramesScheduled.load(); }
        uint64_t TicksPerQuarterFrame() const noexcept;

    private:
        void ThreadWorker() noexcept;
        void SendToAllGroups(_In_ uint64_t timestamp, _In_ uint8_t dataByte) noexcept;
        void SendFullFrame(_In_ uint64_t timestamp, _In_ MidiTimeCodePosition const& position) noexcept;

        void StorePosition(_In_ MidiTimeCodePosition const& position) noexcept;

        winrt::Windows::Devices::Midi2::MidiEndpointConnection m_connection{ nullptr };
        TimeCodeGeneratorOptions m_options;

        mutable std::mutex m_mutex;
        std::condition_variable m_wakeup;
        bool m_stopRequested{ false };

        // Kept fractional, because at 29.97 a whole tick interval is wrong by enough to drift a
        // frame over a long session.
        double m_ticksPerQuarterFrame{ 0.0 };

        uint64_t m_startOriginTimestamp{ 0 };

        // Packed hours, minutes, seconds and frames, so the display can read a consistent
        // position without taking the lock the worker holds.
        std::atomic<uint32_t> m_packedPosition{ 0 };

        std::thread m_worker;
        std::atomic<bool> m_running{ false };
        std::atomic<uint64_t> m_quarterFramesScheduled{ 0 };
        std::atomic<uint64_t> m_lastScheduledTimestamp{ 0 };
    };
}
