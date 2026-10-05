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

#include "ChannelVoiceWords.h"
#include "ClockFollower.h"
#include "GeneratorSink.h"
#include "LfoWave.h"

namespace midiapp
{
    struct LfoMessageGeneratorOptions
    {
        LfoWave Wave{ LfoWave::Sine };

        // How long one pass takes, in quarter notes at BeatsPerMinute.
        double BeatsPerCycle{ 4.0 };
        double BeatsPerMinute{ 120.0 };

        // The two ends of the sweep, 0 to 1 of the message's whole range. Lowest above highest
        // turns the wave upside down.
        double Lowest{ 0.0 };
        double Highest{ 1.0 };

        int32_t IntervalMilliseconds{ DefaultLfoIntervalMilliseconds };

        ValueMessageTarget Target{};

        // Sends the middle of the range when it stops. A pitch bend left wherever the sweep
        // happened to be is an instrument left out of tune.
        bool ReturnsToMiddleWhenStopped{ true };

        // Follows the clock handed to ReceiveClock instead of BeatsPerMinute: one pass is
        // BeatsPerCycle beats of that clock, and its Start puts the sweep back at the top. Fixed
        // when the sweep starts.
        bool FollowsClock{ false };
    };

    // Sends an LFO sweep as channel voice messages. The same shape as BeatClockGenerator, and for
    // the same reason: messages are handed over ahead of time carrying the timestamp they are
    // meant to play at, and the service schedules them, so a slow wakeup on this thread does not
    // put a stagger in the wave.
    //
    // A message that would be the same words as the one before it is not sent again. A slow sweep
    // on a seven bit controller repeats each value several times, and a DIN cable has better
    // things to carry.
    class LfoMessageGenerator
    {
    public:
        LfoMessageGenerator(_In_ GeneratorSink sink, _In_ LfoMessageGeneratorOptions options);

        ~LfoMessageGenerator();

        LfoMessageGenerator(LfoMessageGenerator const&) = delete;
        LfoMessageGenerator& operator=(LfoMessageGenerator const&) = delete;

        // originTimestamp is when the first sample plays. Zero means as soon as the worker runs.
        void Start(_In_ uint64_t originTimestamp = 0);

        // Returns the timestamp of the last message scheduled, including the one that returns the
        // value to the middle, so a caller can wait it out before closing a connection.
        uint64_t Stop();

        bool IsRunning() const noexcept { return m_running.load(); }

        // Takes effect at the first sample that has not been scheduled yet, keeping the phase the
        // sweep has reached, so changing the rate does not jump the wave back to its start.
        void Options(_In_ LfoMessageGeneratorOptions const& options) noexcept;

        // Timing clock, start and song position from the clock a sweep that FollowsClock keeps
        // in step with, at the time each plays. Anything else is ignored. Safe from any thread.
        void ReceiveClock(
            _In_ uint64_t timestamp,
            _In_reads_(wordCount) uint32_t const* words,
            _In_ uint32_t wordCount) noexcept;

        uint64_t SamplesScheduled() const noexcept { return m_samplesScheduled.load(); }

    private:
        void ThreadWorker() noexcept;

        GeneratorSink m_sink{};
        LfoMessageGeneratorOptions m_options{};

        mutable std::mutex m_mutex{};
        std::condition_variable m_wakeup{};
        bool m_stopRequested{ false };

        // Bumped by every change to the options. The worker watches this one number.
        uint64_t m_optionsGeneration{ 0 };

        // The clock a sweep that FollowsClock keeps in step with, and a count of what has
        // arrived from it, so the worker can wait for the next pulse.
        ClockFollower m_clock{};
        uint64_t m_clockGeneration{ 0 };

        uint64_t m_startOriginTimestamp{ 0 };

        std::thread m_worker{};
        std::atomic<bool> m_running{ false };
        std::atomic<uint64_t> m_samplesScheduled{ 0 };
        std::atomic<uint64_t> m_lastScheduledTimestamp{ 0 };
    };
}
