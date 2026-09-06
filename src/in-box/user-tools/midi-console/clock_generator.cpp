// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

#include "pch.h"

#include "clock_generator.h"

namespace midi2console
{
    namespace
    {
        constexpr uint8_t StatusTimingClock = 0xF8;
        constexpr uint8_t StatusStart = 0xFA;
        constexpr uint8_t StatusStop = 0xFC;

        // Short enough that stopping is responsive, long enough that a slow wakeup cannot starve
        // the queue at the highest tempo and pulse rate this command allows.
        constexpr int64_t LookaheadMilliseconds = 500;
        constexpr int64_t RefillMarginMilliseconds = 150;

        std::vector<uint32_t> BuildSystemMessageWords(
            _In_ std::vector<uint8_t> const& groupIndexes,
            _In_ uint8_t status)
        {
            std::vector<uint32_t> words;

            words.reserve(groupIndexes.size());

            for (auto const groupIndex : groupIndexes)
            {
                auto const message = midi2msg::MidiMessageBuilder::BuildSystemMessage(
                    0, midi2::MidiGroup{ groupIndex }, status, 0, 0);

                words.push_back(message.Word0());
            }

            return words;
        }
    }

    _Use_decl_annotations_
    ClockGenerator::ClockGenerator(
        midi2::MidiEndpointConnection const& connection,
        ClockGeneratorOptions options) :
        m_connection(connection),
        m_options(std::move(options))
    {
        m_clockWords = BuildSystemMessageWords(m_options.GroupIndexes, StatusTimingClock);
        m_startWords = BuildSystemMessageWords(m_options.GroupIndexes, StatusStart);
        m_stopWords = BuildSystemMessageWords(m_options.GroupIndexes, StatusStop);

        auto const ticksPerMinute = static_cast<double>(midi2::MidiClock::TimestampFrequency()) * 60.0;

        m_ticksPerPulse =
            ticksPerMinute / m_options.BeatsPerMinute / static_cast<double>(m_options.PulsesPerQuarterNote);
    }

    ClockGenerator::~ClockGenerator()
    {
        Stop();
    }

    void ClockGenerator::Start()
    {
        m_worker = std::thread(&ClockGenerator::ThreadWorker, this);
    }

    uint64_t ClockGenerator::Stop()
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

        return m_lastScheduledTimestamp.load();
    }

    _Use_decl_annotations_
    void ClockGenerator::SendToAllGroups(uint64_t timestamp, uint32_t const* words)
    {
        for (size_t index = 0; index < m_clockWords.size(); index++)
        {
            m_connection.SendSingleMessageWords(timestamp, words[index]);
        }
    }

    void ClockGenerator::ThreadWorker()
    {
        auto const frequency = midi2::MidiClock::TimestampFrequency();

        auto const lookaheadTicks = static_cast<uint64_t>(frequency * LookaheadMilliseconds / 1000);
        auto const refillMarginTicks = static_cast<uint64_t>(frequency * RefillMarginMilliseconds / 1000);

        if (m_options.SendStartMessage && !m_startWords.empty())
        {
            SendToAllGroups(midi2::MidiClock::TimestampConstantSendImmediately(), m_startWords.data());
        }

        // Every pulse is computed from one origin, so truncating each interval to whole ticks
        // cannot accumulate into audible drift over a long session.
        auto const originTimestamp = midi2::MidiClock::Now();

        uint64_t pulseIndex{ 0 };

        auto timestampForPulse = [this, originTimestamp](uint64_t index)
            {
                return originTimestamp + static_cast<uint64_t>(llround(index * m_ticksPerPulse));
            };

        for (;;)
        {
            {
                std::unique_lock<std::mutex> guard{ m_mutex };

                if (m_stopRequested)
                {
                    break;
                }
            }

            auto const scheduleThrough = midi2::MidiClock::Now() + lookaheadTicks;

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
            auto const now = midi2::MidiClock::Now();

            auto const sleepTicks = nextPulseTimestamp > now + refillMarginTicks
                ? nextPulseTimestamp - now - refillMarginTicks
                : 0;

            auto const sleepMilliseconds = static_cast<int64_t>(
                midi2::MidiClock::ConvertTimestampTicksToMilliseconds(sleepTicks));

            std::unique_lock<std::mutex> guard{ m_mutex };

            m_wakeup.wait_for(guard,
                std::chrono::milliseconds(std::max<int64_t>(1, sleepMilliseconds)),
                [this] { return m_stopRequested; });
        }

        if (m_options.SendStopMessage && !m_stopWords.empty())
        {
            // One pulse after the last clock, so the stop lands after everything already queued.
            SendToAllGroups(
                m_lastScheduledTimestamp.load() + static_cast<uint64_t>(llround(m_ticksPerPulse)),
                m_stopWords.data());
        }
    }
}
