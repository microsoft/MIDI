// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

#include <stdint.h>
#include <stddef.h>

// MIDI-CI message parsing and building.
//
// Everything here is allocation-free and non-throwing so that it can be used on a service message
// thread, and takes no ownership of any transport so that a responder built on it can be dropped
// into a message transform. Parsing never indexes with a length that came off the wire without
// first checking it against the real buffer.

namespace WindowsMidiServicesCapabilityInquiry
{
    inline constexpr uint8_t UniversalSystemExclusiveId{ 0x7E };
    inline constexpr uint8_t SubId1CapabilityInquiry{ 0x0D };

    inline constexpr uint8_t DeviceIdFunctionBlock{ 0x7F };

    inline constexpr uint32_t MuidBroadcast{ 0x0FFFFFFF };
    inline constexpr uint32_t MuidReservedStart{ 0x0FFFFF00 };
    inline constexpr uint32_t MuidReservedEnd{ 0x0FFFFFFE };
    inline constexpr uint32_t MuidMaxValue{ 0x0FFFFFFF };

    // 7E <device id> 0D <sub id 2> <version> <source muid x4> <destination muid x4>
    inline constexpr size_t CommonHeaderByteCount{ 13 };

    // Offsets within the payload, which starts at the 7E and excludes the F0 and F7.
    inline constexpr size_t OffsetDeviceId{ 1 };
    inline constexpr size_t OffsetSubId2{ 3 };
    inline constexpr size_t OffsetVersionFormat{ 4 };
    inline constexpr size_t OffsetSourceMuid{ 5 };
    inline constexpr size_t OffsetDestinationMuid{ 9 };


    enum class MessageType : uint8_t
    {
        Unknown = 0x00,

        PropertyExchangeCapabilitiesInquiry = 0x30,
        PropertyExchangeCapabilitiesReply = 0x31,
        PropertyGetDataInquiry = 0x34,
        PropertyGetDataReply = 0x35,
        PropertySetDataInquiry = 0x36,
        PropertySetDataReply = 0x37,
        PropertySubscriptionInquiry = 0x38,
        PropertySubscriptionReply = 0x39,
        PropertyNotify = 0x3F,

        Discovery = 0x70,
        DiscoveryReply = 0x71,
        EndpointInquiry = 0x72,
        EndpointReply = 0x73,
        Ack = 0x7D,
        InvalidateMuid = 0x7E,
        Nak = 0x7F,
    };

    enum class ParseStatus
    {
        Ok = 0,
        TooShort,
        NotUniversalSystemExclusive,
        NotCapabilityInquiry,
        HighBitSetInDataByte,
        LengthFieldExceedsBuffer,
    };


    inline bool MessageTypeIsPropertyExchange(_In_ MessageType const type) noexcept
    {
        const auto value = static_cast<uint8_t>(type);

        return value >= 0x30 && value <= 0x3F;
    }

    inline bool MuidIsUsable(_In_ uint32_t const muid) noexcept
    {
        return muid < MuidReservedStart;
    }

    // MUIDs are four seven-bit bytes, least significant first, everywhere they appear.
    inline uint32_t ReadMuid(_In_reads_(4) uint8_t const* const bytes) noexcept
    {
        return
            (static_cast<uint32_t>(bytes[0] & 0x7F)) |
            (static_cast<uint32_t>(bytes[1] & 0x7F) << 7) |
            (static_cast<uint32_t>(bytes[2] & 0x7F) << 14) |
            (static_cast<uint32_t>(bytes[3] & 0x7F) << 21);
    }

    inline void WriteMuid(_Out_writes_(4) uint8_t* const bytes, _In_ uint32_t const muid) noexcept
    {
        bytes[0] = static_cast<uint8_t>(muid & 0x7F);
        bytes[1] = static_cast<uint8_t>((muid >> 7) & 0x7F);
        bytes[2] = static_cast<uint8_t>((muid >> 14) & 0x7F);
        bytes[3] = static_cast<uint8_t>((muid >> 21) & 0x7F);
    }

    // Property Exchange lengths are two seven-bit bytes, least significant first.
    inline uint16_t ReadFourteenBitValue(_In_reads_(2) uint8_t const* const bytes) noexcept
    {
        return static_cast<uint16_t>(
            (static_cast<uint16_t>(bytes[0] & 0x7F)) |
            (static_cast<uint16_t>(bytes[1] & 0x7F) << 7));
    }

    inline void WriteFourteenBitValue(_Out_writes_(2) uint8_t* const bytes, _In_ uint16_t const value) noexcept
    {
        bytes[0] = static_cast<uint8_t>(value & 0x7F);
        bytes[1] = static_cast<uint8_t>((value >> 7) & 0x7F);
    }


    struct PropertyExchangeFields
    {
        uint8_t RequestId{ 0 };

        uint16_t HeaderByteCount{ 0 };
        uint16_t HeaderOffset{ 0 };

        uint16_t ChunkCount{ 0 };
        uint16_t ChunkNumber{ 0 };

        uint16_t DataByteCount{ 0 };
        uint16_t DataOffset{ 0 };
    };

    struct ParsedMessage
    {
        MessageType Type{ MessageType::Unknown };

        uint8_t DeviceId{ 0 };
        uint8_t VersionFormat{ 0 };

        uint32_t SourceMuid{ 0 };
        uint32_t DestinationMuid{ 0 };

        bool HasPropertyExchangeFields{ false };
        PropertyExchangeFields PropertyExchange{};

        // Carried by Invalidate MUID only.
        uint32_t TargetMuid{ 0 };

        // Carried by Discovery from message version 2 onward. A reply has to echo it.
        uint8_t OutputPathId{ 0 };
    };


    // The payload starts at the 7E and excludes the leading F0 and trailing F7.
    inline ParseStatus Parse(
        _In_reads_(size) uint8_t const* const data,
        _In_ size_t const size,
        _Out_ ParsedMessage& message
    ) noexcept
    {
        message = ParsedMessage{};

        if (data == nullptr || size < CommonHeaderByteCount)
        {
            return ParseStatus::TooShort;
        }

        if (data[0] != UniversalSystemExclusiveId)
        {
            return ParseStatus::NotUniversalSystemExclusive;
        }

        if (data[2] != SubId1CapabilityInquiry)
        {
            return ParseStatus::NotCapabilityInquiry;
        }

        message.Type = static_cast<MessageType>(data[OffsetSubId2] & 0x7F);
        message.DeviceId = data[OffsetDeviceId] & 0x7F;
        message.VersionFormat = data[OffsetVersionFormat] & 0x7F;

        message.SourceMuid = ReadMuid(data + OffsetSourceMuid);
        message.DestinationMuid = ReadMuid(data + OffsetDestinationMuid);

        if (message.Type == MessageType::InvalidateMuid)
        {
            if (size < CommonHeaderByteCount + 4)
            {
                return ParseStatus::TooShort;
            }

            message.TargetMuid = ReadMuid(data + CommonHeaderByteCount);

            return ParseStatus::Ok;
        }

        if (message.Type == MessageType::Discovery)
        {
            // Older initiators predate the output path id and simply stop short.
            if (size >= 30)
            {
                message.OutputPathId = data[29] & 0x7F;
            }

            return ParseStatus::Ok;
        }

        if (MessageTypeIsPropertyExchange(message.Type))
        {
            // request id, then a two byte header length
            size_t offset = CommonHeaderByteCount;

            if (size < offset + 3)
            {
                return ParseStatus::TooShort;
            }

            PropertyExchangeFields fields{};

            fields.RequestId = data[offset] & 0x7F;
            offset++;

            fields.HeaderByteCount = ReadFourteenBitValue(data + offset);
            offset += 2;

            // Attacker-controlled length. Check it against the real buffer before trusting it.
            if (fields.HeaderByteCount > size || offset + fields.HeaderByteCount > size)
            {
                return ParseStatus::LengthFieldExceedsBuffer;
            }

            fields.HeaderOffset = static_cast<uint16_t>(offset);
            offset += fields.HeaderByteCount;

            // chunk count, chunk number, then a two byte data length
            if (size < offset + 6)
            {
                return ParseStatus::TooShort;
            }

            fields.ChunkCount = ReadFourteenBitValue(data + offset);
            offset += 2;

            fields.ChunkNumber = ReadFourteenBitValue(data + offset);
            offset += 2;

            fields.DataByteCount = ReadFourteenBitValue(data + offset);
            offset += 2;

            if (fields.DataByteCount > size || offset + fields.DataByteCount > size)
            {
                return ParseStatus::LengthFieldExceedsBuffer;
            }

            fields.DataOffset = static_cast<uint16_t>(offset);

            message.PropertyExchange = fields;
            message.HasPropertyExchangeFields = true;

            return ParseStatus::Ok;
        }

        return ParseStatus::Ok;
    }


    struct DiscoveryReplyFields
    {
        uint32_t SourceMuid{ 0 };
        uint32_t DestinationMuid{ 0 };

        // MIDI-CI always carries three manufacturer bytes, unlike a MIDI 1.0 identity reply, which
        // uses one unless the first is zero.
        uint8_t ManufacturerSysExId[3]{};

        uint16_t DeviceFamily{ 0 };
        uint16_t DeviceFamilyModelNumber{ 0 };

        uint8_t SoftwareRevisionLevel[4]{};

        uint8_t CapabilityCategories{ 0 };
        uint32_t ReceivableMaximumSysExSize{ 0 };

        uint8_t OutputPathId{ 0 };
        uint8_t FunctionBlockNumber{ 0 };
    };

    inline constexpr size_t DiscoveryReplyByteCount{ 31 };

    // Replying to Discovery is required even of a device that supports no MIDI-CI categories at
    // all. Returns the byte count written, or zero when the buffer is too small.
    inline size_t BuildDiscoveryReply(
        _In_ DiscoveryReplyFields const& fields,
        _Out_writes_to_opt_(capacity, return) uint8_t* const buffer,
        _In_ size_t const capacity
    ) noexcept
    {
        if (buffer == nullptr || capacity < DiscoveryReplyByteCount)
        {
            return 0;
        }

        size_t offset{ 0 };

        buffer[offset++] = UniversalSystemExclusiveId;
        buffer[offset++] = DeviceIdFunctionBlock;
        buffer[offset++] = SubId1CapabilityInquiry;
        buffer[offset++] = static_cast<uint8_t>(MessageType::DiscoveryReply);
        buffer[offset++] = 0x02;

        WriteMuid(buffer + offset, fields.SourceMuid);
        offset += 4;

        WriteMuid(buffer + offset, fields.DestinationMuid);
        offset += 4;

        buffer[offset++] = fields.ManufacturerSysExId[0] & 0x7F;
        buffer[offset++] = fields.ManufacturerSysExId[1] & 0x7F;
        buffer[offset++] = fields.ManufacturerSysExId[2] & 0x7F;

        WriteFourteenBitValue(buffer + offset, fields.DeviceFamily);
        offset += 2;

        WriteFourteenBitValue(buffer + offset, fields.DeviceFamilyModelNumber);
        offset += 2;

        buffer[offset++] = fields.SoftwareRevisionLevel[0] & 0x7F;
        buffer[offset++] = fields.SoftwareRevisionLevel[1] & 0x7F;
        buffer[offset++] = fields.SoftwareRevisionLevel[2] & 0x7F;
        buffer[offset++] = fields.SoftwareRevisionLevel[3] & 0x7F;

        buffer[offset++] = fields.CapabilityCategories & 0x7F;

        WriteMuid(buffer + offset, fields.ReceivableMaximumSysExSize);
        offset += 4;

        buffer[offset++] = fields.OutputPathId & 0x7F;
        buffer[offset++] = fields.FunctionBlockNumber & 0x7F;

        return offset;
    }


    // Common header, request id, and the three two byte length fields.
    inline constexpr size_t PropertyExchangeFixedByteCount{ CommonHeaderByteCount + 1 + 2 + 2 + 2 + 2 };

    // Common header, then simultaneous request count and the two version bytes that arrived with
    // MIDI-CI message version 2.
    inline constexpr size_t PropertyExchangeCapabilitiesByteCount{ CommonHeaderByteCount + 3 };

    // Common Rules for Property Exchange 1.0 and 1.1 both report 0.0.
    inline constexpr uint8_t PropertyExchangeMajorVersion{ 0x00 };
    inline constexpr uint8_t PropertyExchangeMinorVersion{ 0x00 };

    inline size_t BuildPropertyExchangeCapabilitiesReply(
        _In_ uint32_t const sourceMuid,
        _In_ uint32_t const destinationMuid,
        _In_ uint8_t const simultaneousRequests,
        _Out_writes_to_opt_(capacity, return) uint8_t* const buffer,
        _In_ size_t const capacity
    ) noexcept
    {
        if (buffer == nullptr || capacity < PropertyExchangeCapabilitiesByteCount)
        {
            return 0;
        }

        size_t offset{ 0 };

        buffer[offset++] = UniversalSystemExclusiveId;
        buffer[offset++] = DeviceIdFunctionBlock;
        buffer[offset++] = SubId1CapabilityInquiry;
        buffer[offset++] = static_cast<uint8_t>(MessageType::PropertyExchangeCapabilitiesReply);
        buffer[offset++] = 0x02;

        WriteMuid(buffer + offset, sourceMuid);
        offset += 4;

        WriteMuid(buffer + offset, destinationMuid);
        offset += 4;

        buffer[offset++] = simultaneousRequests & 0x7F;
        buffer[offset++] = PropertyExchangeMajorVersion;
        buffer[offset++] = PropertyExchangeMinorVersion;

        return offset;
    }

    // The F0 and F7 that bracket what the builder produces come out of the same budget, so a caller
    // that passes the size a device declared in its Discovery message gets a chunk that fits.
    inline uint16_t MaximumPropertyDataBytesPerChunk(
        _In_ size_t const maximumSysExMessageSize,
        _In_ uint16_t const headerByteCount
    ) noexcept
    {
        const size_t overhead = PropertyExchangeFixedByteCount + headerByteCount + 2;

        if (maximumSysExMessageSize <= overhead)
        {
            return 0;
        }

        const size_t available = maximumSysExMessageSize - overhead;

        return available > 0x3FFF ? static_cast<uint16_t>(0x3FFF) : static_cast<uint16_t>(available);
    }

    inline uint16_t ChunkCountForDataSize(
        _In_ size_t const totalDataByteCount,
        _In_ uint16_t const dataBytesPerChunk
    ) noexcept
    {
        if (dataBytesPerChunk == 0)
        {
            return 0;
        }

        // A resource with no data is still one chunk, because chunk numbering starts at one.
        if (totalDataByteCount == 0)
        {
            return 1;
        }

        const size_t count = (totalDataByteCount + dataBytesPerChunk - 1) / dataBytesPerChunk;

        return count > 0x3FFF ? static_cast<uint16_t>(0) : static_cast<uint16_t>(count);
    }


    struct PropertyExchangeMessageFields
    {
        MessageType Type{ MessageType::PropertyGetDataReply };

        uint32_t SourceMuid{ 0 };
        uint32_t DestinationMuid{ 0 };

        // A reply must carry the request id of the inquiry it answers.
        uint8_t RequestId{ 0 };

        uint8_t const* Header{ nullptr };
        uint16_t HeaderByteCount{ 0 };

        uint16_t ChunkCount{ 0 };
        uint16_t ChunkNumber{ 0 };

        uint8_t const* Data{ nullptr };
        uint16_t DataByteCount{ 0 };
    };

    inline bool AllBytesAreSevenBit(
        _In_reads_opt_(count) uint8_t const* const bytes,
        _In_ size_t const count
    ) noexcept
    {
        if (bytes == nullptr)
        {
            return count == 0;
        }

        for (size_t i = 0; i < count; i++)
        {
            if (bytes[i] > 0x7F)
            {
                return false;
            }
        }

        return true;
    }

    // Returns the byte count written, or zero when the buffer is too small or the caller handed us
    // something that cannot legally travel inside a system exclusive message.
    inline size_t BuildPropertyExchangeMessage(
        _In_ PropertyExchangeMessageFields const& fields,
        _Out_writes_to_opt_(capacity, return) uint8_t* const buffer,
        _In_ size_t const capacity
    ) noexcept
    {
        if (buffer == nullptr || !MessageTypeIsPropertyExchange(fields.Type))
        {
            return 0;
        }

        const size_t required =
            PropertyExchangeFixedByteCount +
            static_cast<size_t>(fields.HeaderByteCount) +
            static_cast<size_t>(fields.DataByteCount);

        if (capacity < required)
        {
            return 0;
        }

        // Masking a stray high bit would silently corrupt the payload, so refuse it instead.
        if (!AllBytesAreSevenBit(fields.Header, fields.HeaderByteCount) ||
            !AllBytesAreSevenBit(fields.Data, fields.DataByteCount))
        {
            return 0;
        }

        size_t offset{ 0 };

        buffer[offset++] = UniversalSystemExclusiveId;
        buffer[offset++] = DeviceIdFunctionBlock;
        buffer[offset++] = SubId1CapabilityInquiry;
        buffer[offset++] = static_cast<uint8_t>(fields.Type);
        buffer[offset++] = 0x02;

        WriteMuid(buffer + offset, fields.SourceMuid);
        offset += 4;

        WriteMuid(buffer + offset, fields.DestinationMuid);
        offset += 4;

        buffer[offset++] = fields.RequestId & 0x7F;

        WriteFourteenBitValue(buffer + offset, fields.HeaderByteCount);
        offset += 2;

        for (uint16_t i = 0; i < fields.HeaderByteCount; i++)
        {
            buffer[offset++] = fields.Header[i];
        }

        WriteFourteenBitValue(buffer + offset, fields.ChunkCount);
        offset += 2;

        WriteFourteenBitValue(buffer + offset, fields.ChunkNumber);
        offset += 2;

        WriteFourteenBitValue(buffer + offset, fields.DataByteCount);
        offset += 2;

        for (uint16_t i = 0; i < fields.DataByteCount; i++)
        {
            buffer[offset++] = fields.Data[i];
        }

        return offset;
    }


    // A resource is serialized once when its data changes, and every reply is a byte range slice of
    // it. Nothing is built and nothing is allocated while a request is being answered.
    struct PropertyReplyChunker
    {
        uint8_t const* Resource{ nullptr };
        size_t ResourceByteCount{ 0 };

        // The reply header, already built. It is repeated on every chunk.
        uint8_t const* Header{ nullptr };
        uint16_t HeaderByteCount{ 0 };

        uint16_t ChunkCount{ 0 };
        uint16_t DataBytesPerChunk{ 0 };

        // Sized against what the far end declared it can receive, not against what we can send.
        bool Plan(_In_ size_t const initiatorMaximumSysExMessageSize) noexcept
        {
            DataBytesPerChunk = MaximumPropertyDataBytesPerChunk(
                initiatorMaximumSysExMessageSize, HeaderByteCount);

            ChunkCount = ChunkCountForDataSize(ResourceByteCount, DataBytesPerChunk);

            return DataBytesPerChunk > 0 && ChunkCount > 0;
        }

        // Chunk numbers count from one. Returns bytes written, or zero.
        size_t BuildChunk(
            _In_ uint16_t const chunkNumber,
            _In_ uint32_t const sourceMuid,
            _In_ uint32_t const destinationMuid,
            _In_ uint8_t const requestId,
            _Out_writes_to_opt_(capacity, return) uint8_t* const buffer,
            _In_ size_t const capacity
        ) const noexcept
        {
            if (chunkNumber == 0 || chunkNumber > ChunkCount || DataBytesPerChunk == 0)
            {
                return 0;
            }

            const size_t offset = static_cast<size_t>(chunkNumber - 1) * DataBytesPerChunk;

            if (offset > ResourceByteCount)
            {
                return 0;
            }

            const size_t remaining = ResourceByteCount - offset;
            const auto thisChunk = static_cast<uint16_t>(
                remaining < DataBytesPerChunk ? remaining : DataBytesPerChunk);

            PropertyExchangeMessageFields fields{};

            fields.Type = MessageType::PropertyGetDataReply;
            fields.SourceMuid = sourceMuid;
            fields.DestinationMuid = destinationMuid;
            fields.RequestId = requestId;
            fields.Header = Header;
            fields.HeaderByteCount = HeaderByteCount;
            fields.ChunkCount = ChunkCount;
            fields.ChunkNumber = chunkNumber;
            fields.Data = (thisChunk > 0) ? (Resource + offset) : nullptr;
            fields.DataByteCount = thisChunk;

            return BuildPropertyExchangeMessage(fields, buffer, capacity);
        }
    };
}
