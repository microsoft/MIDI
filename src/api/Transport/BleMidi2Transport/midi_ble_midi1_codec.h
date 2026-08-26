// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://github.com/microsoft/MIDI/
// ============================================================================

#pragma once

#ifndef MIDI_BLE_MIDI1_CODEC_H
#define MIDI_BLE_MIDI1_CODEC_H

// Encoding and decoding for the BLE MIDI 1.0 Characteristic, as defined in
// MMA/AMEI RP-052 and restated in BLE-MIDI 2.0 (TSB #274) section 4.

namespace MidiBleMidi1
{
    // Header Byte: bit 7 = 1, bit 6 = 0 (reserved), bits 5-0 = timestampHigh
    // Timestamp Byte: bit 7 = 1, bits 6-0 = timestampLow
    inline constexpr uint8_t PacketMarkerBit = 0x80;
    inline constexpr uint8_t HeaderTimestampHighMask = 0x3F;
    inline constexpr uint8_t TimestampLowMask = 0x7F;
    inline constexpr uint16_t TimestampMask = 0x1FFF;

    // MTU minus 3 bytes of ATT overhead. 23 is the default BLE ATT MTU.
    inline constexpr size_t DefaultMaxPacketByteCount = 20;
    inline constexpr size_t MinimumMaxPacketByteCount = 5;
    inline constexpr size_t MaximumMaxPacketByteCount = 512;

    // The sender's 13-bit clock wraps at 8,192 ms.

    inline uint8_t ExpectedDataByteCount(_In_ uint8_t const statusByte) noexcept
    {
        if (statusByte >= 0xF8)
        {
            return 0;
        }

        switch (statusByte & 0xF0)
        {
        case 0x80:      // note off
        case 0x90:      // note on
        case 0xA0:      // poly pressure
        case 0xB0:      // control change
        case 0xE0:      // pitch bend
            return 2;

        case 0xC0:      // program change
        case 0xD0:      // channel pressure
            return 1;

        case 0xF0:
            switch (statusByte)
            {
            case 0xF1:  return 1;   // MIDI time code quarter frame
            case 0xF2:  return 2;   // song position pointer
            case 0xF3:  return 1;   // song select
            default:    return 0;
            }

        default:
            return 0;
        }
    }

    // A run of MIDI 1.0 bytes which share a single BLE timestamp. RelativeMilliseconds is the
    // spacing from the first timestamp in this packet. SenderTimestamp is the sender's own 13-bit
    // millisecond clock, which is what lets timestamps stay ordered across packet boundaries.
    struct DecodedSegment
    {
        uint32_t RelativeMilliseconds{ 0 };
        uint16_t SenderTimestamp{ 0 };
        std::vector<uint8_t> Bytes{ };
    };

    inline constexpr uint32_t TimestampWrapMilliseconds = 8192;

    // A jump larger than this is treated as the sender's clock having restarted rather than as
    // elapsed time, which stops one bad timestamp from throwing the mapping thousands of
    // milliseconds out.
    inline constexpr uint32_t MaximumPlausibleGapMilliseconds = 4096;

    // Maps the sender's 13-bit millisecond clock onto the local timestamp clock.
    //
    // RP-052 leaves this to the implementation, saying only that correlation "must be performed".
    // Anchoring each packet to its own arrival time does not work: consecutive packets are
    // anchored independently, so an early message in one packet can be given a timestamp before a
    // late message in the packet before it, and the stream arrives out of order.
    //
    // Instead the sender's clock is unwrapped into a continuous millisecond count and mapped to
    // the local clock through a single offset. The offset is the smallest arrival delay seen so
    // far, because that is the sample with the least transport latency in it, and it creeps
    // forward slowly to follow clock drift.
    class TimestampCorrelator
    {
    public:
        void Reset() noexcept
        {
            m_haveClock = false;
            m_haveOffset = false;
            m_senderContinuousMilliseconds = 0;
            m_lastSenderTimestamp = 0;
            m_offsetTicks = 0;
            m_lastEmittedTimestamp = 0;
        }

        // Maps every segment of one packet onto the local clock. Whole packets rather than single
        // segments, because all segments in a packet share one arrival time: treating each as its
        // own delay sample would drag the offset down by exactly the spacing being preserved and
        // collapse the packet to a single instant.
        void MapPacket(
            _In_ std::vector<DecodedSegment> const& segments,
            _In_ uint64_t const localArrivalTimestamp,
            _In_ uint64_t const ticksPerMillisecond,
            _Inout_ std::vector<uint64_t>& localTimestamps)
        {
            localTimestamps.clear();

            if (segments.empty())
            {
                return;
            }

            localTimestamps.reserve(segments.size());

            for (auto const& segment : segments)
            {
                localTimestamps.push_back(AdvanceSenderClock(segment.SenderTimestamp));
            }

            // The newest segment is the one whose arrival was actually observed.
            int64_t const newestSenderTicks =
                static_cast<int64_t>(localTimestamps.back() * ticksPerMillisecond);

            int64_t const candidateOffset = static_cast<int64_t>(localArrivalTimestamp) - newestSenderTicks;

            if (!m_haveOffset)
            {
                m_offsetTicks = candidateOffset;
                m_haveOffset = true;
            }
            else if (candidateOffset < m_offsetTicks)
            {
                // Less delay than anything seen before, so a better estimate of the true offset.
                m_offsetTicks = candidateOffset;
            }
            else
            {
                int64_t const creepLimit = static_cast<int64_t>(ticksPerMillisecond);
                int64_t const drift = candidateOffset - m_offsetTicks;

                m_offsetTicks += drift > creepLimit ? creepLimit : drift;
            }

            for (auto& value : localTimestamps)
            {
                int64_t mapped = static_cast<int64_t>(value * ticksPerMillisecond) + m_offsetTicks;

                // Never ahead of the arrival, and never behind what was already delivered.
                if (mapped > static_cast<int64_t>(localArrivalTimestamp))
                {
                    mapped = static_cast<int64_t>(localArrivalTimestamp);
                }

                uint64_t result = mapped < 0 ? 0 : static_cast<uint64_t>(mapped);

                if (result < m_lastEmittedTimestamp)
                {
                    result = m_lastEmittedTimestamp;
                }

                m_lastEmittedTimestamp = result;
                value = result;
            }
        }

    private:
        // Advances the sender clock by one segment and returns its continuous millisecond value.
        uint64_t AdvanceSenderClock(_In_ uint16_t const senderTimestamp) noexcept
        {
            uint16_t const masked = senderTimestamp & TimestampMask;

            if (!m_haveClock)
            {
                m_senderContinuousMilliseconds = masked;
                m_lastSenderTimestamp = masked;
                m_haveClock = true;

                return m_senderContinuousMilliseconds;
            }

            // Timestamps are required to increase, so the forward distance is the wrap-safe delta.
            uint32_t const forwardDelta =
                (static_cast<uint32_t>(masked) + TimestampWrapMilliseconds - m_lastSenderTimestamp) % TimestampWrapMilliseconds;

            m_senderContinuousMilliseconds += forwardDelta > MaximumPlausibleGapMilliseconds ? 0 : forwardDelta;
            m_lastSenderTimestamp = masked;

            return m_senderContinuousMilliseconds;
        }

        bool m_haveClock{ false };
        bool m_haveOffset{ false };
        uint64_t m_senderContinuousMilliseconds{ 0 };
        uint16_t m_lastSenderTimestamp{ 0 };
        int64_t m_offsetTicks{ 0 };
        uint64_t m_lastEmittedTimestamp{ 0 };
    };


    class PacketDecoder
    {
    public:
        void Reset() noexcept
        {
            m_inSysex = false;
            m_runningStatus = 0;
        }

        bool IsInSysex() const noexcept { return m_inSysex; }

        // Returns false if the packet was malformed and discarded.
        bool DecodePacket(
            _In_reads_bytes_opt_(byteCount) uint8_t const* const bytes,
            _In_ size_t const byteCount,
            _Inout_ std::vector<DecodedSegment>& segments)
        {
            segments.clear();

            if (bytes == nullptr || byteCount == 0)
            {
                // an empty payload is the specified response to the initial characteristic read
                return true;
            }

            if ((bytes[0] & PacketMarkerBit) == 0)
            {
                return false;
            }

            uint32_t timestampHigh = bytes[0] & HeaderTimestampHighMask;

            // the end of a BLE packet cancels Running Status
            m_runningStatus = 0;

            uint32_t firstTimestamp{ 0 };
            bool haveFirstTimestamp{ false };
            uint32_t previousTimestampLow{ 0 };
            bool havePreviousTimestampLow{ false };
            uint32_t relativeMilliseconds{ 0 };
            uint32_t absoluteMilliseconds{ 0 };

            auto emit = [&segments, &absoluteMilliseconds](uint32_t const relative, uint8_t const value)
                {
                    if (segments.empty() || segments.back().RelativeMilliseconds != relative)
                    {
                        DecodedSegment segment{};
                        segment.RelativeMilliseconds = relative;
                        segment.SenderTimestamp = static_cast<uint16_t>(absoluteMilliseconds & TimestampMask);
                        segments.push_back(std::move(segment));
                    }

                    segments.back().Bytes.push_back(value);
                };

            size_t i = 1;

            while (i < byteCount)
            {
                size_t const positionAtLoopStart = i;
                uint8_t const b = bytes[i];

                if ((b & PacketMarkerBit) != 0)
                {
                    uint32_t const timestampLow = b & TimestampLowMask;

                    // the increment is never transmitted; the receiver infers it from the wrap
                    if (havePreviousTimestampLow && timestampLow < previousTimestampLow)
                    {
                        timestampHigh++;
                    }

                    previousTimestampLow = timestampLow;
                    havePreviousTimestampLow = true;

                    uint32_t const absolute = (timestampHigh << 7) | timestampLow;

                    if (!haveFirstTimestamp)
                    {
                        firstTimestamp = absolute;
                        haveFirstTimestamp = true;
                    }

                    relativeMilliseconds = absolute > firstTimestamp ? absolute - firstTimestamp : 0;
                    absoluteMilliseconds = absolute;

                    i++;

                    if (i >= byteCount)
                    {
                        // a trailing timestamp byte with nothing after it
                        break;
                    }

                    uint8_t const statusOrData = bytes[i];

                    if ((statusOrData & PacketMarkerBit) != 0)
                    {
                        i++;

                        emit(relativeMilliseconds, statusOrData);

                        if (statusOrData == 0xF0)
                        {
                            m_inSysex = true;
                            m_runningStatus = 0;
                        }
                        else if (statusOrData >= 0xF8)
                        {
                            // System Real-Time may interrupt a SysEx and cancels neither the
                            // SysEx nor Running Status
                        }
                        else
                        {
                            m_inSysex = false;
                            m_runningStatus = statusOrData < 0xF0 ? statusOrData : 0;

                            CopyDataBytes(bytes, byteCount, i, ExpectedDataByteCount(statusOrData), relativeMilliseconds, emit);
                        }
                    }
                    else if (m_runningStatus != 0)
                    {
                        emit(relativeMilliseconds, m_runningStatus);
                        CopyDataBytes(bytes, byteCount, i, ExpectedDataByteCount(m_runningStatus), relativeMilliseconds, emit);
                    }
                    else
                    {
                        // a data byte with no status to attach it to
                        i++;
                    }
                }
                else if (m_inSysex)
                {
                    // SysEx continuation data follows the header byte or a real-time byte directly
                    emit(relativeMilliseconds, b);
                    i++;
                }
                else if (m_runningStatus != 0)
                {
                    // a Running Status message may omit its timestamp byte, in which case it
                    // inherits the timestamp of the most recent preceding message
                    emit(relativeMilliseconds, m_runningStatus);
                    CopyDataBytes(bytes, byteCount, i, ExpectedDataByteCount(m_runningStatus), relativeMilliseconds, emit);
                }
                else
                {
                    i++;
                }

                // guarantees forward progress no matter how malformed the packet is
                if (i == positionAtLoopStart)
                {
                    i++;
                }
            }

            return true;
        }

    private:
        template<typename TEmit>
        static void CopyDataBytes(
            _In_reads_bytes_(byteCount) uint8_t const* const bytes,
            _In_ size_t const byteCount,
            _Inout_ size_t& i,
            _In_ uint8_t const dataByteCount,
            _In_ uint32_t const relativeMilliseconds,
            _In_ TEmit&& emit)
        {
            for (uint8_t d = 0; d < dataByteCount && i < byteCount && (bytes[i] & PacketMarkerBit) == 0; d++)
            {
                emit(relativeMilliseconds, bytes[i]);
                i++;
            }
        }

        bool m_inSysex{ false };
        uint8_t m_runningStatus{ 0 };
    };


    // Builds BLE packets from complete MIDI 1.0 messages. Running Status is never generated:
    // transmitting it is optional, and every receiver is required to accept full messages.
    class PacketBuilder
    {
    public:
        void Reset()
        {
            m_packets.clear();
            m_current.clear();
            m_inSysex = false;
        }

        void SetMaxPacketByteCount(_In_ size_t const maxPacketByteCount) noexcept
        {
            m_maxPacketByteCount = std::clamp(maxPacketByteCount, MinimumMaxPacketByteCount, MaximumMaxPacketByteCount);
        }

        size_t MaxPacketByteCount() const noexcept { return m_maxPacketByteCount; }

        void AppendMessage(
            _In_reads_bytes_opt_(count) uint8_t const* const bytes,
            _In_ size_t const count,
            _In_ uint16_t const timestamp)
        {
            if (bytes == nullptr || count == 0)
            {
                return;
            }

            uint8_t const timestampHigh = static_cast<uint8_t>((timestamp >> 7) & HeaderTimestampHighMask);
            uint8_t const timestampByte = static_cast<uint8_t>(PacketMarkerBit | (timestamp & TimestampLowMask));

            if (m_inSysex || bytes[0] == 0xF0)
            {
                AppendSysexBytes(bytes, count, timestampHigh, timestampByte);
                return;
            }

            // a complete message other than SysEx is never split across two BLE packets
            EnsureRoom(1 + count, timestampHigh);

            m_current.push_back(timestampByte);
            m_current.insert(m_current.end(), bytes, bytes + count);
        }

        void EndPacket()
        {
            if (m_current.size() > 1)
            {
                m_packets.push_back(m_current);
            }

            m_current.clear();
        }

        std::vector<std::vector<uint8_t>> TakePackets()
        {
            EndPacket();

            auto packets = std::move(m_packets);
            m_packets.clear();

            return packets;
        }

    private:
        void AppendSysexBytes(
            _In_reads_bytes_(count) uint8_t const* const bytes,
            _In_ size_t const count,
            _In_ uint8_t const timestampHigh,
            _In_ uint8_t const timestampByte)
        {
            for (size_t i = 0; i < count; i++)
            {
                uint8_t const b = bytes[i];

                if ((b & PacketMarkerBit) == 0)
                {
                    EnsureRoom(1, timestampHigh);
                    m_current.push_back(b);
                }
                else
                {
                    // every status byte, including EOX and any real-time byte interrupting the
                    // transfer, gets its own timestamp byte
                    EnsureRoom(2, timestampHigh);
                    m_current.push_back(timestampByte);
                    m_current.push_back(b);

                    if (b == 0xF0)
                    {
                        m_inSysex = true;
                    }
                    else if (b < 0xF8)
                    {
                        // EOX, or any other status byte, terminates the transfer
                        m_inSysex = false;
                    }
                }
            }
        }

        void EnsureRoom(_In_ size_t const byteCount, _In_ uint8_t const timestampHigh)
        {
            // a packet carries one timestampHigh, so a message past a 128 ms boundary starts a
            // new packet rather than relying on the receiver's single-wrap allowance
            if (!m_current.empty() && (m_packetTimestampHigh != timestampHigh || m_current.size() + byteCount > m_maxPacketByteCount))
            {
                EndPacket();
            }

            if (m_current.empty())
            {
                m_current.push_back(static_cast<uint8_t>(PacketMarkerBit | timestampHigh));
                m_packetTimestampHigh = timestampHigh;
            }
        }

        size_t m_maxPacketByteCount{ DefaultMaxPacketByteCount };
        uint8_t m_packetTimestampHigh{ 0 };
        bool m_inSysex{ false };

        std::vector<uint8_t> m_current{ };
        std::vector<std::vector<uint8_t>> m_packets{ };
    };
}

#endif
