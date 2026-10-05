// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#include "pch.h"
#include "LfoMessageGenerator.h"
#include "LfoSweep.h"

#include <algorithm>
#include <chrono>
#include <cmath>

namespace midiapp
{
    namespace
    {
        namespace lfomidi = ::winrt::Windows::Devices::Midi2;

        // Shorter than the beat clock's, because a change to a sweep should be heard straight
        // away, and long enough that a wakeup a timer tick late still finds samples queued.
        constexpr int64_t LookaheadMilliseconds = 100;
        constexpr int64_t RefillMarginMilliseconds = 40;

        LfoMessageGeneratorOptions Normalized(_In_ LfoMessageGeneratorOptions options) noexcept
        {
            options.BeatsPerCycle = std::clamp(options.BeatsPerCycle, MinimumLfoBeatsPerCycle, MaximumLfoBeatsPerCycle);
            options.BeatsPerMinute = std::clamp(options.BeatsPerMinute, MinimumLfoBeatsPerMinute, MaximumLfoBeatsPerMinute);
            options.Lowest = std::isfinite(options.Lowest) ? std::clamp(options.Lowest, 0.0, 1.0) : 0.0;
            options.Highest = std::isfinite(options.Highest) ? std::clamp(options.Highest, 0.0, 1.0) : 1.0;
            options.IntervalMilliseconds = std::clamp(
                options.IntervalMilliseconds, MinimumLfoIntervalMilliseconds, MaximumLfoIntervalMilliseconds);

            return options;
        }
    }

    _Use_decl_annotations_
    LfoMessageGenerator::LfoMessageGenerator(GeneratorSink sink, LfoMessageGeneratorOptions options) :
        m_sink(std::move(sink)),
        m_options(Normalized(std::move(options)))
    {
    }

    LfoMessageGenerator::~LfoMessageGenerator()
    {
        Stop();
    }

    _Use_decl_annotations_
    void LfoMessageGenerator::Options(LfoMessageGeneratorOptions const& options) noexcept
    {
        {
            std::lock_guard<std::mutex> const guard{ m_mutex };

            m_options = Normalized(options);
            m_optionsGeneration++;
        }

        m_wakeup.notify_all();
    }

    _Use_decl_annotations_
    void LfoMessageGenerator::Start(uint64_t originTimestamp)
    {
        if (m_running.load())
        {
            return;
        }

        if (m_worker.joinable())
        {
            m_worker.join();
        }

        {
            std::lock_guard<std::mutex> const guard{ m_mutex };

            m_stopRequested = false;
            m_startOriginTimestamp = originTimestamp;
        }

        m_samplesScheduled.store(0);
        m_lastScheduledTimestamp.store(0);
        m_running.store(true);

        m_worker = std::thread(&LfoMessageGenerator::ThreadWorker, this);
    }

    uint64_t LfoMessageGenerator::Stop()
    {
        {
            std::lock_guard<std::mutex> const guard{ m_mutex };

            m_stopRequested = true;
        }

        m_wakeup.notify_all();

        if (m_worker.joinable())
        {
            m_worker.join();
        }

        m_running.store(false);

        return m_lastScheduledTimestamp.load();
    }

    void LfoMessageGenerator::ThreadWorker() noexcept
    try
    {
        auto const frequency = lfomidi::MidiClock::TimestampFrequency();

        auto const lookaheadTicks = static_cast<uint64_t>(frequency * LookaheadMilliseconds / 1000);
        auto const refillMarginTicks = static_cast<uint64_t>(frequency * RefillMarginMilliseconds / 1000);

        LfoMessageGeneratorOptions options{};
        uint64_t generation{ 0 };
        uint64_t originTimestamp{ 0 };

        {
            std::lock_guard<std::mutex> const guard{ m_mutex };

            options = m_options;
            generation = m_optionsGeneration;
            originTimestamp = m_startOriginTimestamp != 0 ? m_startOriginTimestamp : lfomidi::MidiClock::Now();
        }

        LfoSweep sweep{};
        sweep.Begin(originTimestamp, frequency, options.BeatsPerCycle, options.BeatsPerMinute, options.IntervalMilliseconds);

        LfoNoise noise{};

        uint32_t lastWords[2]{};
        uint32_t lastCount{ 0 };

        for (;;)
        {
            {
                std::unique_lock<std::mutex> guard{ m_mutex };

                if (m_stopRequested)
                {
                    break;
                }

                if (m_optionsGeneration != generation)
                {
                    options = m_options;
                    generation = m_optionsGeneration;

                    sweep.Retime(options.BeatsPerCycle, options.BeatsPerMinute, options.IntervalMilliseconds);

                    // A new target or range sends its next value even when the words match.
                    lastCount = 0;
                }
            }

            auto const now = lfomidi::MidiClock::Now();

            sweep.CatchUp(now);

            auto const scheduleThrough = now + lookaheadTicks;

            while (sweep.NextDue() <= scheduleThrough)
            {
                auto const due = sweep.NextDue();
                auto const phase = sweep.NextPhase();

                auto const noiseSample = LfoWaveRepeats(options.Wave) ? 0.5 : noise.Next(options.Wave);
                auto const value = LfoSweepValue(options.Wave, phase, noiseSample, options.Lowest, options.Highest);

                sweep.Advance();
                m_samplesScheduled.fetch_add(1);

                uint32_t words[2]{};
                auto const count = BuildValueMessage(options.Target, value, words);

                if (count == 0 || (count == lastCount && std::equal(words, words + count, lastWords)))
                {
                    continue;
                }

                m_sink(due, words, count);

                std::copy_n(words, count, lastWords);
                lastCount = count;

                m_lastScheduledTimestamp.store(due);
            }

            auto const nextDue = sweep.NextDue();
            auto const afterSending = lfomidi::MidiClock::Now();

            auto const sleepTicks = nextDue > afterSending + refillMarginTicks
                ? nextDue - afterSending - refillMarginTicks
                : 0;

            auto const sleepMilliseconds = static_cast<int64_t>(
                lfomidi::MidiClock::ConvertTimestampTicksToMilliseconds(sleepTicks));

            std::unique_lock<std::mutex> guard{ m_mutex };

            m_wakeup.wait_for(guard,
                std::chrono::milliseconds(std::max<int64_t>(1, sleepMilliseconds)),
                [this, generation] { return m_stopRequested || m_optionsGeneration != generation; });
        }

        if (options.ReturnsToMiddleWhenStopped)
        {
            uint32_t words[2]{};
            auto const count = BuildValueMessage(options.Target, (options.Lowest + options.Highest) / 2.0, words);

            if (count > 0)
            {
                // After everything already queued, so the sweep cannot land on top of it.
                auto const last = m_lastScheduledTimestamp.load();
                auto const now = lfomidi::MidiClock::Now();
                auto const timestamp = last > now ? last + sweep.Interval() : now;

                m_sink(timestamp, words, count);
                m_lastScheduledTimestamp.store(timestamp);
            }
        }
    }
    catch (...)
    {
        // This is a thread body, so an escaping exception would terminate the whole app. The
        // sweep stops instead.
    }
}
