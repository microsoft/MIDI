// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#include "pch.h"
#include "BeatClockGenerator.h"

#include <algorithm>
#include <chrono>
#include <cmath>

namespace midiapp
{
    namespace
    {
        namespace clockmidi = ::winrt::Windows::Devices::Midi2;
        namespace clockmsg = ::winrt::Windows::Devices::Midi2::Utilities::Messages;

        constexpr uint8_t StatusTimingClock = 0xF8;
        constexpr uint8_t StatusStart = 0xFA;
        constexpr uint8_t StatusStop = 0xFC;

        // Short enough that stopping is responsive and a tempo change is heard quickly, long
        // enough that a slow wakeup cannot starve the queue at the highest tempo and pulse rate.
        constexpr int64_t LookaheadMilliseconds = 500;
        constexpr int64_t RefillMarginMilliseconds = 150;

        // Enough for a set of generators to each open their worker and queue a first pulse
        // before the shared origin arrives.
        constexpr int64_t StartLeadMilliseconds = 250;

        constexpr double MinimumBeatsPerMinute = 1.0;
        constexpr double MaximumBeatsPerMinute = 1000.0;

        std::vector<uint32_t> BuildSystemMessageWords(
            _In_ std::vector<uint8_t> const& groupIndexes,
            _In_ uint8_t status)
        {
            std::vector<uint32_t> words;

            words.reserve(groupIndexes.size());

            for (auto const groupIndex : groupIndexes)
            {
                auto const message = clockmsg::MidiMessageBuilder::BuildSystemMessage(
                    0, clockmidi::MidiGroup{ groupIndex }, status, 0, 0);

                words.push_back(message.Word0());
            }

            return words;
        }
    }

    _Use_decl_annotations_
    BeatClockGenerator::BeatClockGenerator(
        clockmidi::MidiEndpointConnection const& connection,
        BeatClockGeneratorOptions options) :
        m_connection(connection),
        m_options(std::move(options))
    {
        m_options.ClockRatioNumerator = std::clamp(m_options.ClockRatioNumerator, 1, MaximumClockRatioPart);
        m_options.ClockRatioDenominator = std::clamp(m_options.ClockRatioDenominator, 1, MaximumClockRatioPart);
        m_options.SwingPercent = std::clamp(m_options.SwingPercent, MinimumSwingPercent, MaximumSwingPercent);
        m_options.SwingSubdivision = std::clamp(m_options.SwingSubdivision, 1, 16);
        m_options.OffsetMilliseconds =
            std::clamp(m_options.OffsetMilliseconds, -MaximumOffsetMilliseconds, MaximumOffsetMilliseconds);

        m_clockWords = BuildSystemMessageWords(m_options.GroupIndexes, StatusTimingClock);
        m_startWords = BuildSystemMessageWords(m_options.GroupIndexes, StatusStart);
        m_stopWords = BuildSystemMessageWords(m_options.GroupIndexes, StatusStop);

        m_requestedBeatsPerMinute = m_options.BeatsPerMinute;
        m_ticksPerPulse = TicksPerPulseForTempo(m_options.BeatsPerMinute);
    }

    BeatClockGenerator::~BeatClockGenerator()
    {
        Stop();
    }

    _Use_decl_annotations_
    double BeatClockGenerator::TicksPerPulseForTempo(double beatsPerMinute) const noexcept
    {
        auto const clamped = std::clamp(beatsPerMinute, MinimumBeatsPerMinute, MaximumBeatsPerMinute);

        auto const pulsesPerQuarterNote = std::max(1, m_options.PulsesPerQuarterNote);

        auto const ticksPerMinute = static_cast<double>(clockmidi::MidiClock::TimestampFrequency()) * 60.0;

        auto const base = ticksPerMinute / clamped / static_cast<double>(pulsesPerQuarterNote);

        // A higher ratio means more pulses in the same time, so the gap between them shrinks.
        return base * static_cast<double>(m_options.ClockRatioDenominator)
            / static_cast<double>(m_options.ClockRatioNumerator);
    }

    BeatClockGenerator::SwingShape BeatClockGenerator::BuildSwingShape() const noexcept
    {
        SwingShape shape{};

        auto const pulsesPerHalf =
            static_cast<double>(std::max(1, m_options.PulsesPerQuarterNote)) /
            static_cast<double>(std::max(1, m_options.SwingSubdivision));

        // Below a pulse there is nothing left to move, and at fifty percent the two halves are
        // already equal, so both cases leave the timeline exactly as it was.
        if (pulsesPerHalf < 1.0 || m_options.SwingPercent <= MinimumSwingPercent)
        {
            return shape;
        }

        auto const ratio = std::clamp(m_options.SwingPercent, MinimumSwingPercent, MaximumSwingPercent) / 100.0;

        shape.PulsesPerHalf = pulsesPerHalf;
        shape.FirstScale = ratio * 2.0;
        shape.SecondScale = (1.0 - ratio) * 2.0;

        return shape;
    }

    _Use_decl_annotations_
    double BeatClockGenerator::SwingPosition(uint64_t pulseIndex, SwingShape const& shape) noexcept
    {
        if (shape.PulsesPerHalf <= 0.0)
        {
            return static_cast<double>(pulseIndex);
        }

        auto const pair = shape.PulsesPerHalf * 2.0;
        auto const index = static_cast<double>(pulseIndex);

        auto const pairIndex = std::floor(index / pair);
        auto const withinPair = index - (pairIndex * pair);

        auto const warped = withinPair <= shape.PulsesPerHalf
            ? withinPair * shape.FirstScale
            : (shape.PulsesPerHalf * shape.FirstScale) + ((withinPair - shape.PulsesPerHalf) * shape.SecondScale);

        return (pairIndex * pair) + warped;
    }

    uint64_t BeatClockGenerator::SuggestedStartLeadTicks() noexcept
    {
        return static_cast<uint64_t>(
            clockmidi::MidiClock::TimestampFrequency() * StartLeadMilliseconds / 1000);
    }

    _Use_decl_annotations_
    uint64_t BeatClockGenerator::OffsetMillisecondsToTicks(double milliseconds) noexcept
    {
        auto const clamped = std::clamp(std::abs(milliseconds), 0.0, MaximumOffsetMilliseconds);

        return static_cast<uint64_t>(
            llround(static_cast<double>(clockmidi::MidiClock::TimestampFrequency()) * clamped / 1000.0));
    }

    uint64_t BeatClockGenerator::TicksPerPulse() const noexcept
    {
        std::lock_guard<std::mutex> const guard{ m_mutex };

        return static_cast<uint64_t>(m_ticksPerPulse);
    }

    double BeatClockGenerator::BeatsPerMinute() const noexcept
    {
        std::lock_guard<std::mutex> const guard{ m_mutex };

        return m_requestedBeatsPerMinute;
    }

    double BeatClockGenerator::EffectiveBeatsPerMinute() const noexcept
    {
        std::lock_guard<std::mutex> const guard{ m_mutex };

        return m_requestedBeatsPerMinute * static_cast<double>(m_options.ClockRatioNumerator)
            / static_cast<double>(m_options.ClockRatioDenominator);
    }

    _Use_decl_annotations_
    void BeatClockGenerator::BeatsPerMinute(double value) noexcept
    {
        {
            std::lock_guard<std::mutex> const guard{ m_mutex };

            m_requestedBeatsPerMinute = std::clamp(value, MinimumBeatsPerMinute, MaximumBeatsPerMinute);
            m_timingGeneration++;

            if (!m_running.load())
            {
                // nothing is scheduled, so the new tempo applies from the first pulse
                m_ticksPerPulse = TicksPerPulseForTempo(m_requestedBeatsPerMinute);
            }
        }

        m_wakeup.notify_all();
    }

    _Use_decl_annotations_
    void BeatClockGenerator::ClockRatio(int numerator, int denominator) noexcept
    {
        {
            std::lock_guard<std::mutex> const guard{ m_mutex };

            m_options.ClockRatioNumerator = std::clamp(numerator, 1, MaximumClockRatioPart);
            m_options.ClockRatioDenominator = std::clamp(denominator, 1, MaximumClockRatioPart);
            m_timingGeneration++;

            if (!m_running.load())
            {
                m_ticksPerPulse = TicksPerPulseForTempo(m_requestedBeatsPerMinute);
            }
        }

        m_wakeup.notify_all();
    }

    _Use_decl_annotations_
    void BeatClockGenerator::GetClockRatio(int& numerator, int& denominator) const noexcept
    {
        std::lock_guard<std::mutex> const guard{ m_mutex };

        numerator = m_options.ClockRatioNumerator;
        denominator = m_options.ClockRatioDenominator;
    }

    _Use_decl_annotations_
    void BeatClockGenerator::SwingPercent(double value) noexcept
    {
        {
            std::lock_guard<std::mutex> const guard{ m_mutex };

            m_options.SwingPercent = std::clamp(value, MinimumSwingPercent, MaximumSwingPercent);
            m_timingGeneration++;
        }

        m_wakeup.notify_all();
    }

    double BeatClockGenerator::SwingPercent() const noexcept
    {
        std::lock_guard<std::mutex> const guard{ m_mutex };

        return m_options.SwingPercent;
    }

    _Use_decl_annotations_
    void BeatClockGenerator::Start(uint64_t originTimestamp)
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
            m_ticksPerPulse = TicksPerPulseForTempo(m_requestedBeatsPerMinute);
        }

        m_pulsesScheduled.store(0);
        m_lastScheduledTimestamp.store(0);
        m_running.store(true);

        m_worker = std::thread(&BeatClockGenerator::ThreadWorker, this);
    }

    uint64_t BeatClockGenerator::Stop()
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

    _Use_decl_annotations_
    void BeatClockGenerator::SendToAllGroups(uint64_t timestamp, uint32_t const* words) noexcept
    {
        try
        {
            for (size_t index = 0; index < m_clockWords.size(); index++)
            {
                m_connection.SendSingleMessageWords(timestamp, words[index]);
            }
        }
        catch (...)
        {
            // A send failure must not escape the worker thread. The endpoint going away is
            // reported through the connection, not through every pulse.
        }
    }

    void BeatClockGenerator::ThreadWorker() noexcept
    try
    {
        auto const frequency = clockmidi::MidiClock::TimestampFrequency();

        auto const lookaheadTicks = static_cast<uint64_t>(frequency * LookaheadMilliseconds / 1000);
        auto const refillMarginTicks = static_cast<uint64_t>(frequency * RefillMarginMilliseconds / 1000);

        // Every pulse is computed from one origin, so truncating each interval to whole ticks
        // cannot accumulate into audible drift over a long session.
        uint64_t originTimestamp{ 0 };
        double ticksPerPulse{ 0.0 };
        uint64_t timingGeneration{ 0 };
        SwingShape swingShape{};
        double offsetMilliseconds{ 0.0 };

        {
            std::lock_guard<std::mutex> const guard{ m_mutex };

            originTimestamp = m_startOriginTimestamp != 0 ? m_startOriginTimestamp : clockmidi::MidiClock::Now();
            ticksPerPulse = m_ticksPerPulse;
            timingGeneration = m_timingGeneration;
            swingShape = BuildSwingShape();
            offsetMilliseconds = m_options.OffsetMilliseconds;
        }

        // The per-output offset shifts the whole stream against the shared origin, which is what
        // lets one device that answers late line up with the rest.
        if (offsetMilliseconds != 0.0)
        {
            auto const offsetTicks = OffsetMillisecondsToTicks(offsetMilliseconds);

            if (offsetMilliseconds > 0.0)
            {
                originTimestamp += offsetTicks;
            }
            else
            {
                originTimestamp = originTimestamp > offsetTicks ? originTimestamp - offsetTicks : 0;
            }
        }

        if (m_options.SendStartMessage && !m_startWords.empty())
        {
            // One tick early, so a receiver cannot see the first pulse before the start.
            SendToAllGroups(originTimestamp - 1, m_startWords.data());
        }

        // Counts every pulse since the clock started, so the swing phase is continuous even
        // after a tempo change re-bases the origin.
        uint64_t pulseIndex{ 0 };
        uint64_t originPulseIndex{ 0 };
        double originPosition{ 0.0 };

        auto timestampForPulse =
            [&originTimestamp, &originPosition, &ticksPerPulse, &swingShape](uint64_t index)
            {
                auto const delta = (SwingPosition(index, swingShape) - originPosition) * ticksPerPulse;

                return delta <= 0.0 ? originTimestamp : originTimestamp + static_cast<uint64_t>(llround(delta));
            };

        for (;;)
        {
            {
                std::unique_lock<std::mutex> guard{ m_mutex };

                if (m_stopRequested)
                {
                    break;
                }

                if (m_timingGeneration != timingGeneration)
                {
                    // Re-base on the first pulse that has not been scheduled, so that pulse
                    // still plays when it was promised and the new spacing runs on from there.
                    // The pulse index is not reset, so the swing keeps its place in the bar.
                    originTimestamp = timestampForPulse(pulseIndex);
                    originPulseIndex = pulseIndex;

                    timingGeneration = m_timingGeneration;
                    m_ticksPerPulse = TicksPerPulseForTempo(m_requestedBeatsPerMinute);
                    ticksPerPulse = m_ticksPerPulse;
                    swingShape = BuildSwingShape();
                    originPosition = SwingPosition(originPulseIndex, swingShape);
                }
            }

            auto const scheduleThrough = clockmidi::MidiClock::Now() + lookaheadTicks;

            while (timestampForPulse(pulseIndex) <= scheduleThrough)
            {
                auto const pulseTimestamp = timestampForPulse(pulseIndex);

                SendToAllGroups(pulseTimestamp, m_clockWords.data());

                m_lastScheduledTimestamp.store(pulseTimestamp);
                m_pulsesScheduled.fetch_add(1);

                pulseIndex++;
            }

            auto const nextPulseTimestamp = timestampForPulse(pulseIndex);

            // Wake up in time to refill before the queue runs dry, and immediately on a stop.
            auto const now = clockmidi::MidiClock::Now();

            auto const sleepTicks = nextPulseTimestamp > now + refillMarginTicks
                ? nextPulseTimestamp - now - refillMarginTicks
                : 0;

            auto const sleepMilliseconds = static_cast<int64_t>(
                clockmidi::MidiClock::ConvertTimestampTicksToMilliseconds(sleepTicks));

            std::unique_lock<std::mutex> guard{ m_mutex };

            m_wakeup.wait_for(guard,
                std::chrono::milliseconds(std::max<int64_t>(1, sleepMilliseconds)),
                [this, timingGeneration] { return m_stopRequested || m_timingGeneration != timingGeneration; });
        }

        if (m_options.SendStopMessage && !m_stopWords.empty())
        {
            // One pulse after the last clock, so the stop lands after everything already queued.
            auto const stopTimestamp =
                m_lastScheduledTimestamp.load() + static_cast<uint64_t>(llround(ticksPerPulse));

            SendToAllGroups(stopTimestamp, m_stopWords.data());

            // Stop() hands this to the caller, which waits it out before closing the
            // connection. Leaving the clock's timestamp here loses the stop message.
            m_lastScheduledTimestamp.store(stopTimestamp);
        }
    }
    catch (...)
    {
        // This is a thread body, so an escaping exception would terminate the whole app. The
        // clock stops instead; Stop() still returns the last timestamp that was scheduled.
    }
}
