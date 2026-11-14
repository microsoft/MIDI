// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://github.com/microsoft/MIDI/
// ============================================================================

#pragma once

#ifndef MIDI_BLE_JR_H
#define MIDI_BLE_JR_H

#include <cmath>

// Jitter Reduction (JR) Clock and JR Timestamp handling for the BLE MIDI 2.0 UMP Characteristic.
//
// PROVISIONAL. The UMP rules come from M2-104-UM sections 7.1.6 and 7.2.2. The BLE-specific rules
// come from BLE MIDI 2.0 Working Group discussion and are not all in a published draft yet:
//
//   - JR Clock and JR Timestamp messages are generated and consumed inside the transport. They
//     are never passed to or accepted from applications.
//   - JR Clocks go out on their own schedule, never tied to when data is sent: a startup burst
//     when JR is agreed and when the connection interval changes, then a steady period which is
//     not a whole number of connection intervals. Each clock then waits a different time for its
//     connection event, so the receiver can find the shortest wait. See ClockSchedule.
//   - Every message gets a JR Timestamp, except that messages in the same packet which fall in
//     the same JR tick share one. A JR Timestamp is never later than the sender's JR time when
//     the message is handed to Bluetooth.
//   - Nothing is sent with JR until the other side has agreed to receive it.
//   - Incoming JR Timestamps place messages in time. Incoming JR Clocks track the correlation
//     between the two clocks.
//
// Expect this to change when the specification is updated.
//
// No Bluetooth, service or COM dependency, so it is unit tested on its own.

namespace MidiBleJr
{
    // JR time is a 16-bit count of 1/31250 second ticks (32 microseconds), so it wraps every
    // 2.09712 seconds.
    inline constexpr uint64_t JrTicksPerSecond = 31250;
    inline constexpr uint32_t JrTimeWrap = 0x10000;
    inline constexpr uint32_t JrTimeHalfWrap = JrTimeWrap / 2;

    // UMP section 7.2.2.1: a sender shall send a JR Clock at least this often. The BLE rule below
    // is stricter.
    inline constexpr uint32_t RequiredClockIntervalMilliseconds = 250;

    // BLE MIDI 2.0 Working Group discussion: no more than this between JR Clocks.
    inline constexpr uint32_t MaximumClockSpacingMilliseconds = 100;

    // The startup burst. Sixteen clocks this far apart land at well spread points in any
    // connection interval of 15 ms or less, so the receiver can line up the clocks quickly.
    inline constexpr uint32_t StartupBurstClockCount = 16;
    inline constexpr uint32_t StartupBurstSpacingMicroseconds = 8000;

    // After the burst, a sender which knows the connection interval but not when its connection
    // events happen (Windows doesn't say) sends a clock every whole number of intervals plus this
    // fraction of one. Each clock then lands at a new point in the interval, and the points never
    // repeat. The whole number is the smallest that makes the period at least the minimum.
    inline constexpr double ClockPeriodIntervalFraction = 0.618;
    inline constexpr uint32_t MinimumClockPeriodMicroseconds = 75000;

    // When the connection interval isn't known. Not a whole number of any common interval.
    inline constexpr uint32_t UnknownIntervalClockPeriodMicroseconds = 97000;

    inline constexpr uint8_t UtilityMessageType = 0x0;
    inline constexpr uint8_t JrClockStatus = 0x1;
    inline constexpr uint8_t JrTimestampStatus = 0x2;

    inline constexpr size_t WordByteCount = sizeof(uint32_t);

    inline uint8_t GetMessageType(_In_ uint32_t const word) noexcept
    {
        return static_cast<uint8_t>((word >> 28) & 0x0F);
    }

    inline uint8_t GetUtilityStatus(_In_ uint32_t const word) noexcept
    {
        return static_cast<uint8_t>((word >> 20) & 0x0F);
    }

    inline bool IsJrClock(_In_ uint32_t const word) noexcept
    {
        return GetMessageType(word) == UtilityMessageType && GetUtilityStatus(word) == JrClockStatus;
    }

    inline bool IsJrTimestamp(_In_ uint32_t const word) noexcept
    {
        return GetMessageType(word) == UtilityMessageType && GetUtilityStatus(word) == JrTimestampStatus;
    }

    inline bool IsJrMessage(_In_ uint32_t const word) noexcept
    {
        return IsJrClock(word) || IsJrTimestamp(word);
    }

    inline uint16_t GetJrTime(_In_ uint32_t const word) noexcept
    {
        return static_cast<uint16_t>(word & 0xFFFF);
    }

    inline uint32_t BuildJrClock(_In_ uint16_t const jrTime) noexcept
    {
        return (static_cast<uint32_t>(JrClockStatus) << 20) | jrTime;
    }

    inline uint32_t BuildJrTimestamp(_In_ uint16_t const jrTime) noexcept
    {
        return (static_cast<uint32_t>(JrTimestampStatus) << 20) | jrTime;
    }

    // Split into whole seconds and remainder so a long uptime cannot overflow the multiply.
    inline uint64_t LocalTicksToJrTicks(_In_ uint64_t const localTicks, _In_ uint64_t const localTicksPerSecond) noexcept
    {
        if (localTicksPerSecond == 0)
        {
            return 0;
        }

        return (localTicks / localTicksPerSecond) * JrTicksPerSecond +
            ((localTicks % localTicksPerSecond) * JrTicksPerSecond) / localTicksPerSecond;
    }

    inline int64_t JrTicksToLocalTicks(_In_ int64_t const jrTicks, _In_ uint64_t const localTicksPerSecond) noexcept
    {
        auto const perSecond = static_cast<int64_t>(JrTicksPerSecond);
        auto const localPerSecond = static_cast<int64_t>(localTicksPerSecond);

        return (jrTicks / perSecond) * localPerSecond + ((jrTicks % perSecond) * localPerSecond) / perSecond;
    }

    inline uint16_t LocalTimestampToJrTime(_In_ uint64_t const localTicks, _In_ uint64_t const localTicksPerSecond) noexcept
    {
        return static_cast<uint16_t>(LocalTicksToJrTicks(localTicks, localTicksPerSecond) & 0xFFFF);
    }

    // Stream Configuration Notification: MT 0xF, Form 0, Status 0x006. Byte 4 bit 1 is "will
    // receive JR Timestamps" and bit 0 is "will transmit JR Timestamps", both from the point of
    // view of the UMP Endpoint which sent the notification.
    inline constexpr uint32_t StreamConfigurationNotificationMask = 0xFFFF0000;
    inline constexpr uint32_t StreamConfigurationNotificationValue = 0xF0060000;

    struct StreamConfiguration
    {
        uint8_t Protocol{ 0 };
        bool EndpointReceivesJr{ false };
        bool EndpointSendsJr{ false };
    };

    inline bool IsStreamConfigurationNotification(_In_ uint32_t const firstWord) noexcept
    {
        return (firstWord & StreamConfigurationNotificationMask) == StreamConfigurationNotificationValue;
    }

    inline StreamConfiguration ParseStreamConfigurationNotification(_In_ uint32_t const firstWord) noexcept
    {
        StreamConfiguration configuration{};

        configuration.Protocol = static_cast<uint8_t>((firstWord >> 8) & 0xFF);
        configuration.EndpointReceivesJr = (firstWord & 0x00000002) != 0;
        configuration.EndpointSendsJr = (firstWord & 0x00000001) != 0;

        return configuration;
    }

    // The word count of a UMP from its first word, per M2-104-UM. Zero is never a valid size, so
    // callers stop at a zero.
    inline uint8_t GetUmpWordCount(_In_ uint32_t const firstWord) noexcept
    {
        switch (GetMessageType(firstWord))
        {
        case 0x0: case 0x1: case 0x2: case 0x6: case 0x7:
            return 1;
        case 0x3: case 0x4: case 0x8: case 0x9: case 0xA:
            return 2;
        case 0xB: case 0xC:
            return 3;
        default:
            return 4;
        }
    }


    // A run of incoming messages which share one local timestamp.
    struct TimestampedRun
    {
        uint64_t Timestamp{ 0 };
        std::vector<uint32_t> Words{ };
    };

    // A clock not heard from in this long is not trusted to place messages. The sender owes one
    // every 250 ms, and this also stays well inside the 1.05 second half-wrap where a 16-bit time
    // stops being unambiguous.
    inline constexpr uint32_t ClockStaleMilliseconds = 1000;

    // How far back the offset estimate looks for the smallest arrival delay.
    inline constexpr uint32_t OffsetWindowMilliseconds = 5000;

    // A single step larger than this is the sender's clock having moved, not transport latency.
    inline constexpr uint32_t MaximumOffsetDriftMilliseconds = 1000;

    // A JR tick is 32 microseconds, so a running clock moves about 1,500 ticks in this much real
    // time. A clock which stays put across it is not running.
    inline constexpr uint32_t ClockStallEvidenceMilliseconds = 50;
    inline constexpr uint32_t ClockStallObservationCount = 3;

    // Removes JR Clock and JR Timestamp messages from an incoming UMP stream and uses them to give
    // the remaining messages local timestamps.
    //
    // JR Clocks are unwrapped into a continuous count of sender ticks and mapped onto the local
    // clock through one offset. Arrival delay is never negative, so the smallest observed
    // (arrival - sender) over a sliding window is the best estimate of that offset. This is the
    // same approach as the BLE MIDI 1.0 correlator, with JR Clocks as the samples.
    //
    // Messages are given the time of the most recent JR Timestamp. A message with no usable
    // JR Timestamp, from a device which never sends JR or whose clock is missing or not running,
    // gets its arrival time. Timestamps are never later than arrival and never earlier than what
    // was already delivered.
    class IncomingDecoder
    {
    public:
        void Reset() noexcept
        {
            m_haveClock = false;
            m_clockIsStale = false;
            m_lastClockJrTime = 0;
            m_clockContinuousJrTicks = 0;
            m_lastClockArrival = 0;
            m_offsetSamples.clear();
            m_lastEmittedTimestamp = 0;
            m_clockStallObservations = 0;
            m_haveSeenClockAdvance = false;
            m_haveCarriedTimestamp = false;
            m_carriedTimestampJrTicks = 0;
        }

        bool HaveSeenClock() const noexcept { return m_haveClock; }

        bool ClockIsStalled() const noexcept { return m_clockStallObservations >= ClockStallObservationCount; }

        // A clock was heard from, but not recently enough to be used.
        bool ClockIsStale() const noexcept { return m_haveClock && m_clockIsStale; }

        // JR Timestamps are being used to place messages.
        bool ClockIsUsable() const noexcept
        {
            return m_haveClock && !m_clockIsStale && !ClockIsStalled() && !m_offsetSamples.empty();
        }

        // Takes the complete UMPs of one BLE payload. Every UMP except JR Clock and JR Timestamp
        // comes back in runs, in the order received.
        void DecodePayload(
            _In_reads_(wordCount) uint32_t const* const words,
            _In_ size_t const wordCount,
            _In_ uint64_t const localArrivalTimestamp,
            _In_ uint64_t const localTicksPerSecond,
            _Inout_ std::vector<TimestampedRun>& runs)
        {
            runs.clear();

            if (words == nullptr || wordCount == 0 || localTicksPerSecond == 0)
            {
                return;
            }

            uint64_t const ticksPerMillisecond = (std::max<uint64_t>)(localTicksPerSecond / 1000, 1);

            // Clocks first. The working group recommends the clock at the end of the packet, but
            // it is the newest reference for the timestamps before it too.
            for (size_t i = 0; i < wordCount; )
            {
                auto const messageWordCount = GetUmpWordCount(words[i]);

                if (i + messageWordCount > wordCount)
                {
                    break;
                }

                if (IsJrClock(words[i]))
                {
                    ProcessClock(GetJrTime(words[i]), localArrivalTimestamp, localTicksPerSecond, ticksPerMillisecond);
                }

                i += messageWordCount;
            }

            m_clockIsStale = m_haveClock &&
                localArrivalTimestamp > m_lastClockArrival &&
                localArrivalTimestamp - m_lastClockArrival > static_cast<uint64_t>(ClockStaleMilliseconds) * ticksPerMillisecond;

            bool const clockIsUsable = ClockIsUsable();

            // A JR Timestamp at the very end of the previous payload applies to the first messages
            // of this one, because a sender which runs out of room defers the message to the next
            // packet. Once a timestamp has been used, a later packet does not inherit it. A device
            // which sends one timestamp and then none would otherwise pin every later message to
            // that one instant.
            bool haveTimestamp = m_haveCarriedTimestamp && clockIsUsable;
            bool timestampWasUsed = false;
            int64_t timestampJrTicks = m_carriedTimestampJrTicks;

            m_haveCarriedTimestamp = false;

            for (size_t i = 0; i < wordCount; )
            {
                auto const messageWordCount = GetUmpWordCount(words[i]);

                if (i + messageWordCount > wordCount)
                {
                    break;
                }

                auto const firstWord = words[i];

                if (IsJrClock(firstWord))
                {
                    // consumed above
                }
                else if (IsJrTimestamp(firstWord))
                {
                    if (clockIsUsable)
                    {
                        // Signed distance from the newest clock. A timestamp is never more than
                        // half a wrap from the clock it was sent with.
                        auto const delta = static_cast<int16_t>(static_cast<uint16_t>(GetJrTime(firstWord) - m_lastClockJrTime));

                        timestampJrTicks = m_clockContinuousJrTicks + delta;
                        haveTimestamp = true;
                        timestampWasUsed = false;
                    }
                }
                else
                {
                    uint64_t timestamp{ 0 };

                    if (haveTimestamp)
                    {
                        timestamp = MapSenderTicks(timestampJrTicks, localArrivalTimestamp, localTicksPerSecond);
                        timestampWasUsed = true;
                    }
                    else
                    {
                        timestamp = ClampToEmitted(localArrivalTimestamp);
                    }

                    if (runs.empty() || runs.back().Timestamp != timestamp)
                    {
                        runs.push_back({ timestamp, { } });
                    }

                    runs.back().Words.insert(runs.back().Words.end(), words + i, words + i + messageWordCount);
                }

                i += messageWordCount;
            }

            if (haveTimestamp && !timestampWasUsed)
            {
                m_haveCarriedTimestamp = true;
                m_carriedTimestampJrTicks = timestampJrTicks;
            }
        }

    private:
        struct OffsetSample
        {
            uint64_t ArrivalTicks{ 0 };
            int64_t Offset{ 0 };
        };

        void ProcessClock(
            _In_ uint16_t const jrTime,
            _In_ uint64_t const localArrivalTimestamp,
            _In_ uint64_t const localTicksPerSecond,
            _In_ uint64_t const ticksPerMillisecond)
        {
            bool resynchronize{ false };

            if (!m_haveClock)
            {
                m_clockContinuousJrTicks = jrTime;
                m_haveClock = true;
            }
            else
            {
                uint64_t const localGap = localArrivalTimestamp > m_lastClockArrival ?
                    localArrivalTimestamp - m_lastClockArrival : 0;

                uint32_t const forward = static_cast<uint32_t>(static_cast<uint16_t>(jrTime - m_lastClockJrTime));

                if (localGap > static_cast<uint64_t>(ClockStaleMilliseconds) * ticksPerMillisecond)
                {
                    // Too long since the last clock for a 16-bit time to say how much passed.
                    resynchronize = true;
                }
                else if (forward >= JrTimeHalfWrap)
                {
                    // JR time only runs forward, so this is a sender which restarted its clock.
                    resynchronize = true;
                }

                if (resynchronize)
                {
                    m_clockContinuousJrTicks = jrTime;
                    m_offsetSamples.clear();
                }
                else
                {
                    m_clockContinuousJrTicks += forward;

                    if (forward > 0)
                    {
                        if (ClockIsStalled())
                        {
                            // Running again, so nothing learned while it was stopped still applies.
                            m_offsetSamples.clear();
                        }

                        m_clockStallObservations = 0;
                        m_haveSeenClockAdvance = true;
                    }
                    else if (localGap > static_cast<uint64_t>(ClockStallEvidenceMilliseconds) * ticksPerMillisecond)
                    {
                        if (m_clockStallObservations < ClockStallObservationCount)
                        {
                            m_clockStallObservations++;
                        }
                    }
                }
            }

            m_lastClockJrTime = jrTime;
            m_lastClockArrival = localArrivalTimestamp;

            if (ClockIsStalled())
            {
                return;
            }

            int64_t const candidateOffset =
                static_cast<int64_t>(localArrivalTimestamp) - JrTicksToLocalTicks(m_clockContinuousJrTicks, localTicksPerSecond);

            // one sample is always kept so there is always an offset to map with
            uint64_t const windowTicks = static_cast<uint64_t>(OffsetWindowMilliseconds) * ticksPerMillisecond;

            while (m_offsetSamples.size() > 1 &&
                   localArrivalTimestamp > m_offsetSamples.front().ArrivalTicks &&
                   localArrivalTimestamp - m_offsetSamples.front().ArrivalTicks > windowTicks)
            {
                m_offsetSamples.pop_front();
            }

            if (!m_offsetSamples.empty() &&
                candidateOffset - m_offsetSamples.front().Offset >
                    static_cast<int64_t>(MaximumOffsetDriftMilliseconds) * static_cast<int64_t>(ticksPerMillisecond))
            {
                // Waiting for the window to age out would strand every message in the past until it did.
                m_offsetSamples.clear();
            }

            // a younger and smaller sample means the older, larger ones can never be the minimum again
            while (!m_offsetSamples.empty() && m_offsetSamples.back().Offset >= candidateOffset)
            {
                m_offsetSamples.pop_back();
            }

            m_offsetSamples.push_back({ localArrivalTimestamp, candidateOffset });
        }

        uint64_t MapSenderTicks(
            _In_ int64_t const senderJrTicks,
            _In_ uint64_t const localArrivalTimestamp,
            _In_ uint64_t const localTicksPerSecond) noexcept
        {
            int64_t mapped = JrTicksToLocalTicks(senderJrTicks, localTicksPerSecond) + m_offsetSamples.front().Offset;

            // A timestamp ahead of arrival is a message the sender meant for later. This service
            // timestamps incoming messages and leaves it to the application when to act on them,
            // so it is delivered now and stamped now rather than held back.
            if (mapped > static_cast<int64_t>(localArrivalTimestamp))
            {
                mapped = static_cast<int64_t>(localArrivalTimestamp);
            }

            return ClampToEmitted(mapped < 0 ? 0 : static_cast<uint64_t>(mapped));
        }

        uint64_t ClampToEmitted(_In_ uint64_t const timestamp) noexcept
        {
            uint64_t const result = timestamp < m_lastEmittedTimestamp ? m_lastEmittedTimestamp : timestamp;

            m_lastEmittedTimestamp = result;

            return result;
        }

        bool m_haveClock{ false };
        bool m_clockIsStale{ false };
        uint16_t m_lastClockJrTime{ 0 };
        int64_t m_clockContinuousJrTicks{ 0 };
        uint64_t m_lastClockArrival{ 0 };

        // ascending by arrival and by offset, so the front is the smallest delay in the window
        std::deque<OffsetSample> m_offsetSamples{ };

        uint64_t m_lastEmittedTimestamp{ 0 };

        uint32_t m_clockStallObservations{ 0 };
        bool m_haveSeenClockAdvance{ false };

        bool m_haveCarriedTimestamp{ false };
        int64_t m_carriedTimestampJrTicks{ 0 };
    };


    // Packs outgoing UMPs into BLE payloads. A UMP is never split across packets.
    //
    // With JR enabled, each message is preceded by a JR Timestamp unless the message before it in
    // the same packet had the same JR time. JR Clocks are not added here: they go out on their own
    // schedule, because a clock tied to when data is sent waits the same way every time.
    //
    // JR Clock and JR Timestamp messages handed in by applications are dropped whether or not JR
    // is enabled. They belong to this link and only the transport can generate them correctly.
    class OutgoingPacketBuilder
    {
    public:
        void Reset() noexcept
        {
            m_current.clear();
            m_packets.clear();
            m_haveTimestampInPacket = false;
            m_lastJrTimeInPacket = 0;
        }

        void SetMaxPayloadByteCount(_In_ size_t const byteCount) noexcept
        {
            m_maxPayloadByteCount = byteCount < WordByteCount ? WordByteCount : byteCount;
        }

        void SetJrEnabled(_In_ bool const enabled) noexcept
        {
            m_jrEnabled = enabled;
        }

        bool JrEnabled() const noexcept { return m_jrEnabled; }

        // Returns true when the message was taken, false when it was a JR message and dropped.
        bool AppendMessage(
            _In_reads_(wordCount) uint32_t const* const words,
            _In_ uint8_t const wordCount,
            _In_ uint16_t const jrTime)
        {
            if (words == nullptr || wordCount == 0)
            {
                return false;
            }

            if (IsJrMessage(words[0]))
            {
                return false;
            }

            size_t const messageByteCount = static_cast<size_t>(wordCount) * WordByteCount;

            if (!m_jrEnabled)
            {
                if (!m_current.empty() && m_current.size() + messageByteCount > m_maxPayloadByteCount)
                {
                    FinishPacket();
                }

                AppendWords(words, wordCount);

                return true;
            }

            bool needTimestamp = !m_haveTimestampInPacket || m_lastJrTimeInPacket != jrTime;

            size_t needed = messageByteCount + (needTimestamp ? WordByteCount : 0);

            if (!m_current.empty() && m_current.size() + needed > m_maxPayloadByteCount)
            {
                FinishPacket();

                needTimestamp = true;
            }

            if (needTimestamp)
            {
                AppendWord(BuildJrTimestamp(jrTime));

                m_haveTimestampInPacket = true;
                m_lastJrTimeInPacket = jrTime;
            }

            AppendWords(words, wordCount);

            return true;
        }

        // Ends the packet in progress and hands over everything built so far.
        std::vector<std::vector<uint8_t>> TakePackets()
        {
            if (!m_current.empty())
            {
                FinishPacket();
            }

            std::vector<std::vector<uint8_t>> packets;
            packets.swap(m_packets);

            return packets;
        }

        // A JR Clock on its own. The writer fills in the time just before the write.
        static std::vector<uint8_t> BuildClockOnlyPacket(_In_ uint16_t const jrTime)
        {
            std::vector<uint8_t> packet;
            AppendWordBigEndian(packet, BuildJrClock(jrTime));

            return packet;
        }

        static void AppendWordBigEndian(_Inout_ std::vector<uint8_t>& bytes, _In_ uint32_t const word)
        {
            bytes.push_back(static_cast<uint8_t>((word >> 24) & 0xFF));
            bytes.push_back(static_cast<uint8_t>((word >> 16) & 0xFF));
            bytes.push_back(static_cast<uint8_t>((word >> 8) & 0xFF));
            bytes.push_back(static_cast<uint8_t>(word & 0xFF));
        }

    private:
        void AppendWord(_In_ uint32_t const word)
        {
            AppendWordBigEndian(m_current, word);
        }

        void AppendWords(_In_reads_(wordCount) uint32_t const* const words, _In_ uint8_t const wordCount)
        {
            for (uint8_t i = 0; i < wordCount; i++)
            {
                AppendWord(words[i]);
            }
        }

        void FinishPacket()
        {
            m_packets.push_back(std::move(m_current));

            m_current.clear();
            m_haveTimestampInPacket = false;
            m_lastJrTimeInPacket = 0;
        }

        size_t m_maxPayloadByteCount{ 64 };
        bool m_jrEnabled{ false };

        std::vector<uint8_t> m_current{ };
        std::vector<std::vector<uint8_t>> m_packets{ };

        bool m_haveTimestampInPacket{ false };
        uint16_t m_lastJrTimeInPacket{ 0 };
    };

    // The period between JR Clocks after the startup burst, for a connection interval in
    // microseconds. Zero means the interval isn't known.
    inline uint32_t ClockPeriodMicroseconds(_In_ uint32_t const connectionIntervalMicroseconds) noexcept
    {
        if (connectionIntervalMicroseconds == 0)
        {
            return UnknownIntervalClockPeriodMicroseconds;
        }

        double const interval = static_cast<double>(connectionIntervalMicroseconds);

        double wholeIntervals = std::ceil(static_cast<double>(MinimumClockPeriodMicroseconds) / interval - ClockPeriodIntervalFraction);
        wholeIntervals = (std::max)(wholeIntervals, 0.0);

        double period = (wholeIntervals + ClockPeriodIntervalFraction) * interval;

        // Only a connection interval well beyond what BLE MIDI asks for gets here
        while (period > static_cast<double>(MaximumClockSpacingMilliseconds) * 1000.0 && wholeIntervals > 0.0)
        {
            wholeIntervals -= 1.0;
            period = (wholeIntervals + ClockPeriodIntervalFraction) * interval;
        }

        if (period > static_cast<double>(MaximumClockSpacingMilliseconds) * 1000.0)
        {
            return UnknownIntervalClockPeriodMicroseconds;
        }

        return static_cast<uint32_t>(std::llround(period));
    }

    // When each outgoing JR Clock is due, in local timestamp ticks. Starts with the burst, then
    // runs at ClockPeriodMicroseconds. Every due time is worked out from the first one, not from
    // the one before, so rounding never builds up. A sender which falls behind skips the clocks
    // it missed rather than sending them in a rush.
    class ClockSchedule
    {
    public:
        void Start(
            _In_ uint64_t const nowTicks,
            _In_ uint32_t const connectionIntervalMicroseconds,
            _In_ uint64_t const localTicksPerSecond) noexcept
        {
            m_running = localTicksPerSecond != 0;
            m_ticksPerMicrosecond = static_cast<double>(localTicksPerSecond) / 1'000'000.0;
            m_burstStartTicks = nowTicks;
            m_index = 0;
            m_periodMicroseconds = ClockPeriodMicroseconds(connectionIntervalMicroseconds);
            m_periodStartTicks = BurstTime(StartupBurstClockCount - 1) +
                static_cast<uint64_t>(std::llround(m_periodMicroseconds * m_ticksPerMicrosecond));
        }

        void Stop() noexcept
        {
            m_running = false;
        }

        bool IsRunning() const noexcept { return m_running; }
        bool InBurst() const noexcept { return m_running && m_index < StartupBurstClockCount; }
        uint32_t PeriodMicroseconds() const noexcept { return m_periodMicroseconds; }

        uint64_t NextDueTicks() const noexcept
        {
            return m_index < StartupBurstClockCount ?
                BurstTime(m_index) :
                PeriodTime(m_index - StartupBurstClockCount);
        }

        // After the clock that was due has been sent.
        void Advance(_In_ uint64_t const nowTicks) noexcept
        {
            m_index++;

            if (m_index < StartupBurstClockCount)
            {
                return;
            }

            // Behind by more than a period: carry on from the next time still ahead, keeping
            // the same grid
            uint64_t const periodTicks = static_cast<uint64_t>(std::llround(m_periodMicroseconds * m_ticksPerMicrosecond));

            if (periodTicks != 0 && nowTicks > NextDueTicks() + periodTicks)
            {
                uint64_t const behind = (nowTicks - m_periodStartTicks) / periodTicks;

                m_index = StartupBurstClockCount + static_cast<uint32_t>((std::min<uint64_t>)(behind, UINT32_MAX - StartupBurstClockCount));
            }
        }

    private:
        uint64_t BurstTime(_In_ uint32_t const index) const noexcept
        {
            return m_burstStartTicks + static_cast<uint64_t>(std::llround(
                static_cast<double>(index) * StartupBurstSpacingMicroseconds * m_ticksPerMicrosecond));
        }

        uint64_t PeriodTime(_In_ uint32_t const index) const noexcept
        {
            return m_periodStartTicks + static_cast<uint64_t>(std::llround(
                static_cast<double>(index) * m_periodMicroseconds * m_ticksPerMicrosecond));
        }

        bool m_running{ false };
        double m_ticksPerMicrosecond{ 0.0 };
        uint64_t m_burstStartTicks{ 0 };
        uint64_t m_periodStartTicks{ 0 };
        uint32_t m_index{ 0 };
        uint32_t m_periodMicroseconds{ UnknownIntervalClockPeriodMicroseconds };
    };
}

#endif
