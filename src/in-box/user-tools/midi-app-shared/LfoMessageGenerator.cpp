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
#include <array>
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

        // How long a sweep that follows a clock sleeps when no clock is coming in. A pulse wakes
        // it sooner.
        constexpr int64_t WaitForClockMilliseconds = 250;

        // At the shortest interval, the most samples one lookahead can hold.
        constexpr size_t MaximumSamplesPerPass = 32;

        constexpr uint32_t MessageTypeSystem = 0x1;
        constexpr uint8_t StatusSongPosition = 0xF2;
        constexpr uint8_t StatusTimingClock = 0xF8;
        constexpr uint8_t StatusStart = 0xFA;

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
    void LfoMessageGenerator::ReceiveClock(uint64_t timestamp, uint32_t const* words, uint32_t wordCount) noexcept
    {
        try
        {
            if (words == nullptr || wordCount == 0 || (words[0] >> 28) != MessageTypeSystem)
            {
                return;
            }

            auto const status = static_cast<uint8_t>((words[0] >> 16) & 0xFF);

            if (status != StatusTimingClock && status != StatusStart && status != StatusSongPosition)
            {
                return;
            }

            auto const time = timestamp != 0 ? timestamp : lfomidi::MidiClock::Now();

            {
                std::lock_guard<std::mutex> const guard{ m_mutex };

                if (status == StatusTimingClock)
                {
                    m_clock.Pulse(time);
                }
                else if (status == StatusStart)
                {
                    m_clock.Start(time);
                }
                else
                {
                    // Sixteenth notes since the top, low seven bits first.
                    m_clock.SongPosition(time, ((words[0] >> 8) & 0x7Fu) | ((words[0] & 0x7Fu) << 7));
                }

                m_clockGeneration++;
            }

            m_wakeup.notify_all();
        }
        catch (...)
        {
            // On a routing thread. A clock this can't take is a clock it doesn't follow.
        }
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

        // Fixed for the life of the sweep. A change to it means a new sweep.
        auto const followsClock = options.FollowsClock;

        auto const takeNewOptions = [&]()
            {
                options = m_options;
                options.FollowsClock = followsClock;
                generation = m_optionsGeneration;
            };

        auto const stepTicks = [&]()
            {
                return (std::max)(uint64_t{ 1 }, frequency * static_cast<uint64_t>(options.IntervalMilliseconds) / 1000);
            };

        LfoNoise noise{};

        uint32_t lastWords[2]{};
        uint32_t lastCount{ 0 };
        bool sentAny{ false };

        // One sample, unless it would be the same words as the one before.
        auto const send = [&](uint64_t due, double phase)
            {
                auto const noiseSample = LfoWaveRepeats(options.Wave) ? 0.5 : noise.Next(options.Wave);
                auto const value = LfoSweepValue(options.Wave, phase, noiseSample, options.Lowest, options.Highest);

                m_samplesScheduled.fetch_add(1);

                uint32_t words[2]{};
                auto const count = BuildValueMessage(options.Target, value, words);

                if (count == 0 || (count == lastCount && std::equal(words, words + count, lastWords)))
                {
                    return;
                }

                m_sink(due, words, count);

                std::copy_n(words, count, lastWords);
                lastCount = count;
                sentAny = true;

                m_lastScheduledTimestamp.store(due);
            };

        if (followsClock)
        {
            // Samples are taken on a grid the way a free sweep's are, but where each falls in the
            // cycle comes from the clock. Only as far ahead as the clock is known, so a clock that
            // stops leaves the sweep where it got to.
            auto next = originTimestamp;
            uint64_t clockGeneration{ 0 };

            std::array<std::pair<uint64_t, double>, MaximumSamplesPerPass> due{};

            for (;;)
            {
                size_t dueCount{ 0 };
                bool waitingForClock{ false };

                {
                    std::lock_guard<std::mutex> const guard{ m_mutex };

                    if (m_stopRequested)
                    {
                        break;
                    }

                    if (m_optionsGeneration != generation)
                    {
                        takeNewOptions();
                        lastCount = 0;
                    }

                    auto const step = stepTicks();
                    auto const now = lfomidi::MidiClock::Now();

                    // After a long wait for the clock, the grid carries on from now rather than
                    // working through everything it missed.
                    if (next < now && now - next > step * 8)
                    {
                        next = now;
                    }

                    auto const through = (std::min)(now + lookaheadTicks, m_clock.KnownUntil());

                    while (next <= through && dueCount < due.size())
                    {
                        if (auto const pulses = m_clock.PulsesAt(next))
                        {
                            due[dueCount++] = { next, pulses.value() };
                        }

                        next += step;
                    }

                    waitingForClock = next > m_clock.KnownUntil();
                    clockGeneration = m_clockGeneration;
                }

                auto const pulsesPerCycle = MidiClocksPerBeat * options.BeatsPerCycle;

                for (size_t i = 0; i < dueCount; i++)
                {
                    auto const turns = due[i].second / pulsesPerCycle;

                    send(due[i].first, turns - std::floor(turns));
                }

                auto const afterSending = lfomidi::MidiClock::Now();

                auto const sleepTicks = next > afterSending + refillMarginTicks
                    ? next - afterSending - refillMarginTicks
                    : 0;

                auto const sleepMilliseconds = waitingForClock
                    ? WaitForClockMilliseconds
                    : (std::max)(int64_t{ 1 }, static_cast<int64_t>(lfomidi::MidiClock::ConvertTimestampTicksToMilliseconds(sleepTicks)));

                std::unique_lock<std::mutex> guard{ m_mutex };

                m_wakeup.wait_for(guard, std::chrono::milliseconds(sleepMilliseconds),
                    [&] { return m_stopRequested || m_optionsGeneration != generation ||
                        (waitingForClock && m_clockGeneration != clockGeneration); });
            }
        }
        else
        {
            LfoSweep sweep{};
            sweep.Begin(originTimestamp, frequency, options.BeatsPerCycle, options.BeatsPerMinute, options.IntervalMilliseconds);

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
                        takeNewOptions();

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

                    sweep.Advance();

                    send(due, phase);
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
        }

        // Only a sweep that sent something has anything to put back.
        if (options.ReturnsToMiddleWhenStopped && sentAny)
        {
            uint32_t words[2]{};
            auto const count = BuildValueMessage(options.Target, (options.Lowest + options.Highest) / 2.0, words);

            if (count > 0)
            {
                // After everything already queued, so the sweep cannot land on top of it.
                auto const last = m_lastScheduledTimestamp.load();
                auto const now = lfomidi::MidiClock::Now();
                auto const timestamp = last > now ? last + stepTicks() : now;

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
