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

    // A run of MIDI 1.0 bytes which share a single BLE timestamp. Milliseconds are relative to
    // the first timestamp in the packet, so the sender's clock never has to be correlated with
    // ours: the receiver only needs the spacing between messages within one packet.
    struct DecodedSegment
    {
        uint32_t RelativeMilliseconds{ 0 };
        std::vector<uint8_t> Bytes{ };
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

            auto emit = [&segments](uint32_t const relative, uint8_t const value)
                {
                    if (segments.empty() || segments.back().RelativeMilliseconds != relative)
                    {
                        DecodedSegment segment{};
                        segment.RelativeMilliseconds = relative;
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
