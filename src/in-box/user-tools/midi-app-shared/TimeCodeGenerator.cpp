// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#include "pch.h"
#include "TimeCodeGenerator.h"
#include "BeatClockGenerator.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>

namespace midiapp
{
    namespace
    {
        namespace tcmidi = ::winrt::Windows::Devices::Midi2;
        namespace tcmsg = ::winrt::Windows::Devices::Midi2::Utilities::Messages;

        constexpr uint8_t StatusTimeCodeQuarterFrame = 0xF1;

        // System exclusive status nibbles in a Universal MIDI Packet.
        constexpr uint8_t SysExStart = 0x1;
        constexpr uint8_t SysExEnd = 0x3;

        // Same numbers as the beat clock, and for the same reasons: long enough that a slow
        // wakeup cannot starve the queue, short enough that stopping is responsive.
        constexpr int64_t LookaheadMilliseconds = 500;
        constexpr int64_t RefillMarginMilliseconds = 150;

        uint32_t PackPosition(_In_ MidiTimeCodePosition const& position) noexcept
        {
            return (static_cast<uint32_t>(position.Hours) << 24) |
                (static_cast<uint32_t>(position.Minutes) << 16) |
                (static_cast<uint32_t>(position.Seconds) << 8) |
                static_cast<uint32_t>(position.Frames);
        }

        MidiTimeCodePosition UnpackPosition(_In_ uint32_t const packed) noexcept
        {
            MidiTimeCodePosition position{};

            position.Hours = static_cast<uint8_t>((packed >> 24) & 0xFF);
            position.Minutes = static_cast<uint8_t>((packed >> 16) & 0xFF);
            position.Seconds = static_cast<uint8_t>((packed >> 8) & 0xFF);
            position.Frames = static_cast<uint8_t>(packed & 0xFF);

            return position;
        }
    }

    _Use_decl_annotations_
    TimeCodeGenerator::TimeCodeGenerator(
        tcmidi::MidiEndpointConnection const& connection,
        TimeCodeGeneratorOptions options) :
        m_connection(connection),
        m_options(std::move(options))
    {
        m_options.StartPosition = ClampPosition(m_options.StartPosition, m_options.FrameRate);
        m_options.OffsetMilliseconds = std::clamp(
            m_options.OffsetMilliseconds,
            -BeatClockGenerator::MaximumOffsetMilliseconds,
            BeatClockGenerator::MaximumOffsetMilliseconds);

        m_ticksPerQuarterFrame =
            static_cast<double>(tcmidi::MidiClock::TimestampFrequency()) /
            QuarterFramesPerSecond(m_options.FrameRate);

        m_packedPosition.store(PackPosition(m_options.StartPosition));
    }

    TimeCodeGenerator::~TimeCodeGenerator()
    {
        Stop();
    }

    uint64_t TimeCodeGenerator::TicksPerQuarterFrame() const noexcept
    {
        std::lock_guard<std::mutex> const guard{ m_mutex };

        return static_cast<uint64_t>(m_ticksPerQuarterFrame);
    }

    MidiTimeCodePosition TimeCodeGenerator::CurrentPosition() const noexcept
    {
        return UnpackPosition(m_packedPosition.load());
    }

    _Use_decl_annotations_
    void TimeCodeGenerator::StorePosition(MidiTimeCodePosition const& position) noexcept
    {
        m_packedPosition.store(PackPosition(position));
    }

    _Use_decl_annotations_
    void TimeCodeGenerator::Start(uint64_t originTimestamp)
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

        m_quarterFramesScheduled.store(0);
        m_lastScheduledTimestamp.store(0);
        StorePosition(m_options.StartPosition);
        m_running.store(true);

        m_worker = std::thread(&TimeCodeGenerator::ThreadWorker, this);
    }

    uint64_t TimeCodeGenerator::Stop()
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
    void TimeCodeGenerator::SendToAllGroups(uint64_t timestamp, uint8_t dataByte) noexcept
    {
        try
        {
            for (auto const groupIndex : m_options.GroupIndexes)
            {
                auto const message = tcmsg::MidiMessageBuilder::BuildSystemMessage(
                    0, tcmidi::MidiGroup{ groupIndex }, StatusTimeCodeQuarterFrame, dataByte, 0);

                m_connection.SendSingleMessageWords(timestamp, message.Word0());
            }
        }
        catch (...)
        {
            // A send failure must not escape the worker thread. The endpoint going away is
            // reported through the connection, not through every message.
        }
    }

    _Use_decl_annotations_
    void TimeCodeGenerator::SendFullFrame(uint64_t timestamp, MidiTimeCodePosition const& position) noexcept
    {
        try
        {
            std::array<uint8_t, MidiTimeCodeFullFramePayloadSize> payload{};

            FillFullFramePayload(position, m_options.FrameRate, payload.data());

            for (auto const groupIndex : m_options.GroupIndexes)
            {
                tcmidi::MidiGroup const group{ groupIndex };

                // Eight payload bytes do not fit one packet, so the dump is a start carrying six
                // and an end carrying the last two.
                auto const first = tcmsg::MidiMessageBuilder::BuildSystemExclusive7Message(
                    0, group, SysExStart, 6,
                    payload[0], payload[1], payload[2], payload[3], payload[4], payload[5]);

                auto const second = tcmsg::MidiMessageBuilder::BuildSystemExclusive7Message(
                    0, group, SysExEnd, 2,
                    payload[6], payload[7], 0, 0, 0, 0);

                m_connection.SendSingleMessageWords(timestamp, first.Word0(), first.Word1());
                m_connection.SendSingleMessageWords(timestamp, second.Word0(), second.Word1());
            }
        }
        catch (...)
        {
        }
    }

    void TimeCodeGenerator::ThreadWorker() noexcept
    try
    {
        auto const frequency = tcmidi::MidiClock::TimestampFrequency();

        auto const lookaheadTicks = static_cast<uint64_t>(frequency * LookaheadMilliseconds / 1000);
        auto const refillMarginTicks = static_cast<uint64_t>(frequency * RefillMarginMilliseconds / 1000);

        uint64_t originTimestamp{ 0 };
        double ticksPerQuarterFrame{ 0.0 };

        {
            std::lock_guard<std::mutex> const guard{ m_mutex };

            originTimestamp = m_startOriginTimestamp != 0 ? m_startOriginTimestamp : tcmidi::MidiClock::Now();
            ticksPerQuarterFrame = m_ticksPerQuarterFrame;
        }

        if (m_options.OffsetMilliseconds != 0.0)
        {
            auto const offsetTicks = BeatClockGenerator::OffsetMillisecondsToTicks(m_options.OffsetMilliseconds);

            if (m_options.OffsetMilliseconds > 0.0)
            {
                originTimestamp += offsetTicks;
            }
            else
            {
                originTimestamp = originTimestamp > offsetTicks ? originTimestamp - offsetTicks : 0;
            }
        }

        auto const rate = m_options.FrameRate;

        if (m_options.SendFullFrameMessages && !m_options.GroupIndexes.empty())
        {
            // One tick early, so a receiver cannot see the first quarter frame before it has
            // been told where it is.
            SendFullFrame(originTimestamp - 1, m_options.StartPosition);
        }

        // Where the clock has actually reached, moving on one frame every four quarter frames.
        auto currentPosition = m_options.StartPosition;

        // What is being spelled out right now. Latched at the start of every group of eight, so
        // a receiver which assembles it two frames later gets a value it can correct.
        auto transmittedPosition = currentPosition;

        uint64_t quarterFrameIndex{ 0 };

        auto timestampForQuarterFrame =
            [&originTimestamp, &ticksPerQuarterFrame](uint64_t index)
            {
                return originTimestamp + static_cast<uint64_t>(llround(index * ticksPerQuarterFrame));
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

            auto const scheduleThrough = tcmidi::MidiClock::Now() + lookaheadTicks;

            while (timestampForQuarterFrame(quarterFrameIndex) <= scheduleThrough)
            {
                auto const piece = static_cast<uint8_t>(quarterFrameIndex % MidiTimeCodePiecesPerSequence);

                if (piece == 0)
                {
                    transmittedPosition = currentPosition;
                }

                auto const timestamp = timestampForQuarterFrame(quarterFrameIndex);

                SendToAllGroups(timestamp, QuarterFrameDataByte(transmittedPosition, rate, piece));

                m_lastScheduledTimestamp.store(timestamp);
                m_quarterFramesScheduled.fetch_add(1);

                quarterFrameIndex++;

                if ((quarterFrameIndex % MidiTimeCodeQuarterFramesPerFrame) == 0)
                {
                    AdvanceOneFrame(currentPosition, rate);
                    StorePosition(currentPosition);
                }
            }

            auto const nextTimestamp = timestampForQuarterFrame(quarterFrameIndex);
            auto const now = tcmidi::MidiClock::Now();

            auto const sleepTicks = nextTimestamp > now + refillMarginTicks
                ? nextTimestamp - now - refillMarginTicks
                : 0;

            auto const sleepMilliseconds = static_cast<int64_t>(
                tcmidi::MidiClock::ConvertTimestampTicksToMilliseconds(sleepTicks));

            std::unique_lock<std::mutex> guard{ m_mutex };

            m_wakeup.wait_for(guard,
                std::chrono::milliseconds(std::max<int64_t>(1, sleepMilliseconds)),
                [this] { return m_stopRequested; });
        }

        if (m_options.SendFullFrameMessages && !m_options.GroupIndexes.empty())
        {
            // One quarter frame after the last message, so it lands after everything already
            // queued and parks the receiver where the clock stopped.
            auto const stopTimestamp =
                m_lastScheduledTimestamp.load() + static_cast<uint64_t>(llround(ticksPerQuarterFrame));

            SendFullFrame(stopTimestamp, currentPosition);

            // Stop() hands this to the caller, which waits it out before closing the connection.
            // Leaving the quarter frame's timestamp here would lose the full frame.
            m_lastScheduledTimestamp.store(stopTimestamp);
        }
    }
    catch (...)
    {
        // This is a thread body, so an escaping exception would terminate the whole app.
    }
}
