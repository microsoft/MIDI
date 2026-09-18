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

        ProfileInquiry = 0x20,
        ProfileInquiryReply = 0x21,
        SetProfileOn = 0x22,
        SetProfileOff = 0x23,
        ProfileEnabledReport = 0x24,
        ProfileDisabledReport = 0x25,
        ProfileAddedReport = 0x26,
        ProfileRemovedReport = 0x27,
        ProfileDetailsInquiry = 0x28,
        ProfileDetailsInquiryReply = 0x29,
        ProfileSpecificData = 0x2F,

        PropertyExchangeCapabilitiesInquiry = 0x30,
        PropertyExchangeCapabilitiesReply = 0x31,
        PropertyGetDataInquiry = 0x34,
        PropertyGetDataReply = 0x35,
        PropertySetDataInquiry = 0x36,
        PropertySetDataReply = 0x37,
        PropertySubscriptionInquiry = 0x38,
        PropertySubscriptionReply = 0x39,
        PropertyNotify = 0x3F,

        ProcessInquiryCapabilities = 0x40,
        ProcessInquiryCapabilitiesReply = 0x41,
        MidiMessageReport = 0x42,
        MidiMessageReportReply = 0x43,
        MidiMessageReportEnd = 0x44,

        Discovery = 0x70,
        DiscoveryReply = 0x71,
        EndpointInquiry = 0x72,
        EndpointReply = 0x73,
        Ack = 0x7D,
        InvalidateMuid = 0x7E,
        Nak = 0x7F,
    };

    // The capability categories a device declares in Discovery, as a bit per category.
    inline constexpr uint8_t CategoryProtocolNegotiation{ 0x02 };
    inline constexpr uint8_t CategoryProfileConfiguration{ 0x04 };
    inline constexpr uint8_t CategoryPropertyExchange{ 0x08 };
    inline constexpr uint8_t CategoryProcessInquiry{ 0x10 };

    inline constexpr size_t ProfileIdByteCount{ 5 };

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

    inline bool MessageTypeIsProfileConfiguration(_In_ MessageType const type) noexcept
    {
        const auto value = static_cast<uint8_t>(type);

        return value >= 0x20 && value <= 0x2F;
    }

    inline bool MessageTypeIsAcknowledgment(_In_ MessageType const type) noexcept
    {
        return type == MessageType::Ack || type == MessageType::Nak;
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

    // Profile Specific Data declares its length in four seven-bit bytes rather than two, because
    // what a profile may send is not bounded by the same rules as a property exchange chunk.
    inline uint32_t ReadTwentyEightBitValue(_In_reads_(4) uint8_t const* const bytes) noexcept
    {
        return
            (static_cast<uint32_t>(bytes[0] & 0x7F)) |
            (static_cast<uint32_t>(bytes[1] & 0x7F) << 7) |
            (static_cast<uint32_t>(bytes[2] & 0x7F) << 14) |
            (static_cast<uint32_t>(bytes[3] & 0x7F) << 21);
    }

    inline void WriteTwentyEightBitValue(_Out_writes_(4) uint8_t* const bytes, _In_ uint32_t const value) noexcept
    {
        bytes[0] = static_cast<uint8_t>(value & 0x7F);
        bytes[1] = static_cast<uint8_t>((value >> 7) & 0x7F);
        bytes[2] = static_cast<uint8_t>((value >> 14) & 0x7F);
        bytes[3] = static_cast<uint8_t>((value >> 21) & 0x7F);
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

    struct ProfileFields
    {
        // Every profile message names a profile except Profile Inquiry and the reply to it, which
        // ask for and return the whole list instead.
        bool HasProfileId{ false };
        uint8_t ProfileId[ProfileIdByteCount]{};

        // Set Profile On asks for this many channels; the enabled and disabled reports say how many
        // were actually taken. Absent on a message from a version 1 device, and specified as zero
        // when the message is addressed to a group or a function block rather than to a channel.
        bool HasChannelCount{ false };
        uint16_t ChannelCount{ 0 };

        // Profile Details Inquiry and its reply. Below 0x40 the meaning is common to all profiles;
        // from 0x40 up it is defined by the profile itself.
        bool HasInquiryTarget{ false };
        uint8_t InquiryTarget{ 0 };

        // The reply to a details inquiry, and the payload of Profile Specific Data. As with
        // property exchange, the offset is into the buffer the caller passed in.
        uint32_t TargetDataByteCount{ 0 };
        uint16_t TargetDataOffset{ 0 };

        // The reply to Profile Inquiry. Both are counts of five byte identifiers packed end to end
        // at the offset beside them.
        uint16_t EnabledProfileCount{ 0 };
        uint16_t EnabledProfileOffset{ 0 };

        uint16_t DisabledProfileCount{ 0 };
        uint16_t DisabledProfileOffset{ 0 };
    };

    struct AcknowledgmentFields
    {
        // Which message this answers, so a reply can be matched to the transaction that caused it.
        uint8_t OriginalMessageType{ 0 };

        uint8_t StatusCode{ 0 };
        uint8_t StatusData{ 0 };

        // What these five bytes mean depends on the branch: a profile identifier for a profile
        // message, and the request id plus chunk number for property exchange.
        uint8_t Details[5]{};

        uint16_t MessageTextByteCount{ 0 };
        uint16_t MessageTextOffset{ 0 };
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

        bool HasProfileFields{ false };
        ProfileFields Profile{};

        bool HasAcknowledgmentFields{ false };
        AcknowledgmentFields Acknowledgment{};

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

        if (MessageTypeIsProfileConfiguration(message.Type))
        {
            size_t offset = CommonHeaderByteCount;

            ProfileFields fields{};

            // Profile Inquiry asks for the whole list and so names no single profile; its reply
            // returns two counted lists. Everything else in the category names one profile.
            if (message.Type == MessageType::ProfileInquiryReply)
            {
                if (size < offset + 2)
                {
                    return ParseStatus::TooShort;
                }

                fields.EnabledProfileCount = ReadFourteenBitValue(data + offset);
                offset += 2;

                const size_t enabledBytes = static_cast<size_t>(fields.EnabledProfileCount) * ProfileIdByteCount;

                // A count that came off the wire. Check it against the real buffer before trusting it.
                if (offset + enabledBytes > size)
                {
                    return ParseStatus::LengthFieldExceedsBuffer;
                }

                fields.EnabledProfileOffset = static_cast<uint16_t>(offset);
                offset += enabledBytes;

                if (size < offset + 2)
                {
                    return ParseStatus::TooShort;
                }

                fields.DisabledProfileCount = ReadFourteenBitValue(data + offset);
                offset += 2;

                const size_t disabledBytes = static_cast<size_t>(fields.DisabledProfileCount) * ProfileIdByteCount;

                if (offset + disabledBytes > size)
                {
                    return ParseStatus::LengthFieldExceedsBuffer;
                }

                fields.DisabledProfileOffset = static_cast<uint16_t>(offset);

                message.Profile = fields;
                message.HasProfileFields = true;

                return ParseStatus::Ok;
            }

            if (message.Type != MessageType::ProfileInquiry)
            {
                if (size < offset + ProfileIdByteCount)
                {
                    return ParseStatus::TooShort;
                }

                for (size_t i = 0; i < ProfileIdByteCount; i++)
                {
                    fields.ProfileId[i] = data[offset + i] & 0x7F;
                }

                fields.HasProfileId = true;
                offset += ProfileIdByteCount;
            }

            switch (message.Type)
            {
            case MessageType::SetProfileOn:
            case MessageType::SetProfileOff:
            case MessageType::ProfileEnabledReport:
            case MessageType::ProfileDisabledReport:
                // A version 1 device stops after the identifier, so a missing count is not an error.
                if (size >= offset + 2)
                {
                    fields.ChannelCount = ReadFourteenBitValue(data + offset);
                    fields.HasChannelCount = true;
                }
                break;

            case MessageType::ProfileDetailsInquiry:
                if (size < offset + 1)
                {
                    return ParseStatus::TooShort;
                }

                fields.InquiryTarget = data[offset] & 0x7F;
                fields.HasInquiryTarget = true;
                break;

            case MessageType::ProfileDetailsInquiryReply:
            {
                if (size < offset + 3)
                {
                    return ParseStatus::TooShort;
                }

                fields.InquiryTarget = data[offset] & 0x7F;
                fields.HasInquiryTarget = true;
                offset++;

                fields.TargetDataByteCount = ReadFourteenBitValue(data + offset);
                offset += 2;

                if (offset + fields.TargetDataByteCount > size)
                {
                    return ParseStatus::LengthFieldExceedsBuffer;
                }

                fields.TargetDataOffset = static_cast<uint16_t>(offset);
                break;
            }

            case MessageType::ProfileSpecificData:
            {
                if (size < offset + 4)
                {
                    return ParseStatus::TooShort;
                }

                // Four seven-bit bytes here, not two, so the count can exceed what fits in a buffer
                // this size by a wide margin. Compare in a type that cannot overflow doing it.
                fields.TargetDataByteCount = ReadTwentyEightBitValue(data + offset);
                offset += 4;

                if (static_cast<uint64_t>(offset) + fields.TargetDataByteCount > size)
                {
                    return ParseStatus::LengthFieldExceedsBuffer;
                }

                fields.TargetDataOffset = static_cast<uint16_t>(offset);
                break;
            }

            default:
                break;
            }

            message.Profile = fields;
            message.HasProfileFields = true;

            return ParseStatus::Ok;
        }

        if (MessageTypeIsAcknowledgment(message.Type))
        {
            size_t offset = CommonHeaderByteCount;

            // Every field below arrived with message version 2. A version 1 responder sends a bare
            // acknowledgment, which is still a well formed message and still worth reporting.
            if (size < offset + 10)
            {
                return ParseStatus::Ok;
            }

            AcknowledgmentFields fields{};

            fields.OriginalMessageType = data[offset++] & 0x7F;
            fields.StatusCode = data[offset++] & 0x7F;
            fields.StatusData = data[offset++] & 0x7F;

            for (size_t i = 0; i < 5; i++)
            {
                fields.Details[i] = data[offset + i] & 0x7F;
            }

            offset += 5;

            fields.MessageTextByteCount = ReadFourteenBitValue(data + offset);
            offset += 2;

            if (offset + fields.MessageTextByteCount > size)
            {
                return ParseStatus::LengthFieldExceedsBuffer;
            }

            fields.MessageTextOffset = static_cast<uint16_t>(offset);

            message.Acknowledgment = fields;
            message.HasAcknowledgmentFields = true;

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

    inline constexpr size_t DiscoveryReplyByteCount{ 31 };

    // Discovery carries everything its reply does except the function block number, which only a
    // responder can know.
    inline constexpr size_t DiscoveryByteCount{ 30 };

    // Writes the thirteen bytes every capability inquiry message starts with. Returns the count
    // written, or zero when the buffer is too small.
    inline size_t WriteCommonHeader(
        _Out_writes_to_opt_(capacity, return) uint8_t* const buffer,
        _In_ size_t const capacity,
        _In_ uint8_t const deviceId,
        _In_ MessageType const type,
        _In_ uint32_t const sourceMuid,
        _In_ uint32_t const destinationMuid
    ) noexcept
    {
        if (buffer == nullptr || capacity < CommonHeaderByteCount)
        {
            return 0;
        }

        size_t offset{ 0 };

        buffer[offset++] = UniversalSystemExclusiveId;
        buffer[offset++] = deviceId & 0x7F;
        buffer[offset++] = SubId1CapabilityInquiry;
        buffer[offset++] = static_cast<uint8_t>(type);
        buffer[offset++] = 0x02;

        WriteMuid(buffer + offset, sourceMuid);
        offset += 4;

        WriteMuid(buffer + offset, destinationMuid);
        offset += 4;

        return offset;
    }

    // An initiator announces itself with this. The destination is always the broadcast MUID,
    // because the initiator does not yet know who is out there.
    inline size_t BuildDiscovery(
        _In_ DiscoveryReplyFields const& fields,
        _Out_writes_to_opt_(capacity, return) uint8_t* const buffer,
        _In_ size_t const capacity
    ) noexcept
    {
        if (buffer == nullptr || capacity < DiscoveryByteCount)
        {
            return 0;
        }

        size_t offset = WriteCommonHeader(
            buffer, capacity, DeviceIdFunctionBlock, MessageType::Discovery,
            fields.SourceMuid, MuidBroadcast);

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

        return offset;
    }

    inline constexpr size_t InvalidateMuidByteCount{ CommonHeaderByteCount + 4 };

    // Withdraws a MUID. It is sent to the broadcast MUID and gets no reply.
    inline size_t BuildInvalidateMuid(
        _In_ uint32_t const sourceMuid,
        _In_ uint32_t const targetMuid,
        _Out_writes_to_opt_(capacity, return) uint8_t* const buffer,
        _In_ size_t const capacity
    ) noexcept
    {
        if (buffer == nullptr || capacity < InvalidateMuidByteCount)
        {
            return 0;
        }

        size_t offset = WriteCommonHeader(
            buffer, capacity, DeviceIdFunctionBlock, MessageType::InvalidateMuid,
            sourceMuid, MuidBroadcast);

        WriteMuid(buffer + offset, targetMuid);
        offset += 4;

        return offset;
    }

    inline constexpr size_t AcknowledgmentFixedByteCount{ CommonHeaderByteCount + 1 + 1 + 1 + 5 + 2 };

    // Builds an ACK or a NAK. Anything else is refused rather than encoded under the wrong sub id.
    inline size_t BuildAcknowledgment(
        _In_ MessageType const type,
        _In_ uint8_t const deviceId,
        _In_ uint32_t const sourceMuid,
        _In_ uint32_t const destinationMuid,
        _In_ AcknowledgmentFields const& fields,
        _In_reads_opt_(messageTextByteCount) uint8_t const* const messageText,
        _In_ uint16_t const messageTextByteCount,
        _Out_writes_to_opt_(capacity, return) uint8_t* const buffer,
        _In_ size_t const capacity
    ) noexcept
    {
        if (buffer == nullptr || !MessageTypeIsAcknowledgment(type))
        {
            return 0;
        }

        if (capacity < AcknowledgmentFixedByteCount + messageTextByteCount)
        {
            return 0;
        }

        if (!AllBytesAreSevenBit(messageText, messageTextByteCount))
        {
            return 0;
        }

        size_t offset = WriteCommonHeader(
            buffer, capacity, deviceId, type, sourceMuid, destinationMuid);

        buffer[offset++] = fields.OriginalMessageType & 0x7F;
        buffer[offset++] = fields.StatusCode & 0x7F;
        buffer[offset++] = fields.StatusData & 0x7F;

        for (size_t i = 0; i < 5; i++)
        {
            buffer[offset++] = fields.Details[i] & 0x7F;
        }

        WriteFourteenBitValue(buffer + offset, messageTextByteCount);
        offset += 2;

        for (uint16_t i = 0; i < messageTextByteCount; i++)
        {
            buffer[offset++] = messageText[i];
        }

        return offset;
    }


    struct ProfileMessageFields
    {
        MessageType Type{ MessageType::ProfileInquiry };

        uint8_t DeviceId{ DeviceIdFunctionBlock };

        uint32_t SourceMuid{ 0 };
        uint32_t DestinationMuid{ 0 };

        uint8_t ProfileId[ProfileIdByteCount]{};

        uint16_t ChannelCount{ 0 };
        uint8_t InquiryTarget{ 0 };

        uint8_t const* Data{ nullptr };
        uint32_t DataByteCount{ 0 };
    };

    // Builds any of the profile configuration messages. Which trailing fields are written is
    // decided by the message type, so a caller fills in only what its message actually carries.
    inline size_t BuildProfileMessage(
        _In_ ProfileMessageFields const& fields,
        _Out_writes_to_opt_(capacity, return) uint8_t* const buffer,
        _In_ size_t const capacity
    ) noexcept
    {
        if (buffer == nullptr || !MessageTypeIsProfileConfiguration(fields.Type))
        {
            return 0;
        }

        // The reply to Profile Inquiry carries two lists and is built by the caller that owns them.
        if (fields.Type == MessageType::ProfileInquiryReply)
        {
            return 0;
        }

        size_t required = CommonHeaderByteCount;

        if (fields.Type != MessageType::ProfileInquiry)
        {
            required += ProfileIdByteCount;
        }

        switch (fields.Type)
        {
        case MessageType::SetProfileOn:
        case MessageType::SetProfileOff:
        case MessageType::ProfileEnabledReport:
        case MessageType::ProfileDisabledReport:
            required += 2;
            break;

        case MessageType::ProfileDetailsInquiry:
            required += 1;
            break;

        case MessageType::ProfileDetailsInquiryReply:
            required += 1 + 2 + fields.DataByteCount;
            break;

        case MessageType::ProfileSpecificData:
            required += 4 + fields.DataByteCount;
            break;

        default:
            break;
        }

        if (capacity < required)
        {
            return 0;
        }

        if (!AllBytesAreSevenBit(fields.Data, fields.DataByteCount))
        {
            return 0;
        }

        size_t offset = WriteCommonHeader(
            buffer, capacity, fields.DeviceId, fields.Type,
            fields.SourceMuid, fields.DestinationMuid);

        if (fields.Type != MessageType::ProfileInquiry)
        {
            for (size_t i = 0; i < ProfileIdByteCount; i++)
            {
                buffer[offset++] = fields.ProfileId[i] & 0x7F;
            }
        }

        switch (fields.Type)
        {
        case MessageType::SetProfileOn:
        case MessageType::SetProfileOff:
        case MessageType::ProfileEnabledReport:
        case MessageType::ProfileDisabledReport:
            WriteFourteenBitValue(buffer + offset, fields.ChannelCount);
            offset += 2;
            break;

        case MessageType::ProfileDetailsInquiry:
            buffer[offset++] = fields.InquiryTarget & 0x7F;
            break;

        case MessageType::ProfileDetailsInquiryReply:
            buffer[offset++] = fields.InquiryTarget & 0x7F;

            WriteFourteenBitValue(buffer + offset, static_cast<uint16_t>(fields.DataByteCount));
            offset += 2;

            for (uint32_t i = 0; i < fields.DataByteCount; i++)
            {
                buffer[offset++] = fields.Data[i];
            }
            break;

        case MessageType::ProfileSpecificData:
            WriteTwentyEightBitValue(buffer + offset, fields.DataByteCount);
            offset += 4;

            for (uint32_t i = 0; i < fields.DataByteCount; i++)
            {
                buffer[offset++] = fields.Data[i];
            }
            break;

        default:
            break;
        }

        return offset;
    }

    // Builds the reply to Profile Inquiry from two arrays of five byte identifiers packed end to
    // end. A device with no profiles on the addressed channel still replies, with both counts zero.
    inline size_t BuildProfileInquiryReply(
        _In_ uint8_t const deviceId,
        _In_ uint32_t const sourceMuid,
        _In_ uint32_t const destinationMuid,
        _In_reads_opt_(enabledCount * ProfileIdByteCount) uint8_t const* const enabledProfiles,
        _In_ uint16_t const enabledCount,
        _In_reads_opt_(disabledCount * ProfileIdByteCount) uint8_t const* const disabledProfiles,
        _In_ uint16_t const disabledCount,
        _Out_writes_to_opt_(capacity, return) uint8_t* const buffer,
        _In_ size_t const capacity
    ) noexcept
    {
        if (buffer == nullptr)
        {
            return 0;
        }

        if ((enabledProfiles == nullptr && enabledCount > 0) ||
            (disabledProfiles == nullptr && disabledCount > 0))
        {
            return 0;
        }

        const size_t enabledBytes = static_cast<size_t>(enabledCount) * ProfileIdByteCount;
        const size_t disabledBytes = static_cast<size_t>(disabledCount) * ProfileIdByteCount;

        if (capacity < CommonHeaderByteCount + 2 + enabledBytes + 2 + disabledBytes)
        {
            return 0;
        }

        if (!AllBytesAreSevenBit(enabledProfiles, enabledBytes) ||
            !AllBytesAreSevenBit(disabledProfiles, disabledBytes))
        {
            return 0;
        }

        size_t offset = WriteCommonHeader(
            buffer, capacity, deviceId, MessageType::ProfileInquiryReply,
            sourceMuid, destinationMuid);

        WriteFourteenBitValue(buffer + offset, enabledCount);
        offset += 2;

        for (size_t i = 0; i < enabledBytes; i++)
        {
            buffer[offset++] = enabledProfiles[i];
        }

        WriteFourteenBitValue(buffer + offset, disabledCount);
        offset += 2;

        for (size_t i = 0; i < disabledBytes; i++)
        {
            buffer[offset++] = disabledProfiles[i];
        }

        return offset;
    }

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

        // The reply header, already built. It goes on the first chunk and no other: the
        // specification requires every later chunk to declare a header length of zero.
        uint8_t const* Header{ nullptr };
        uint16_t HeaderByteCount{ 0 };

        uint16_t ChunkCount{ 0 };

        // The first chunk gives up room to the header, so the two sizes differ and a chunk's
        // position in the resource cannot be worked out by multiplication alone.
        uint16_t FirstChunkDataByteCount{ 0 };
        uint16_t LaterChunkDataByteCount{ 0 };

        // Sized against what the far end declared it can receive, not against what we can send.
        bool Plan(_In_ size_t const initiatorMaximumSysExMessageSize) noexcept
        {
            FirstChunkDataByteCount = MaximumPropertyDataBytesPerChunk(
                initiatorMaximumSysExMessageSize, HeaderByteCount);

            LaterChunkDataByteCount = MaximumPropertyDataBytesPerChunk(
                initiatorMaximumSysExMessageSize, 0);

            if (FirstChunkDataByteCount == 0 || LaterChunkDataByteCount == 0)
            {
                ChunkCount = 0;
                return false;
            }

            if (ResourceByteCount <= FirstChunkDataByteCount)
            {
                // One chunk, which is also the answer for a resource with no data at all: chunk
                // numbering starts at one, so there is no such thing as a reply of zero chunks.
                ChunkCount = 1;
            }
            else
            {
                const auto remaining = ResourceByteCount - FirstChunkDataByteCount;
                const auto later = ChunkCountForDataSize(remaining, LaterChunkDataByteCount);

                if (later == 0 || later > 0x3FFE)
                {
                    ChunkCount = 0;
                    return false;
                }

                ChunkCount = static_cast<uint16_t>(later + 1);
            }

            return true;
        }

        // Where a chunk's data starts in the resource.
        size_t OffsetOfChunk(_In_ uint16_t const chunkNumber) const noexcept
        {
            if (chunkNumber <= 1)
            {
                return 0;
            }

            return static_cast<size_t>(FirstChunkDataByteCount) +
                static_cast<size_t>(chunkNumber - 2) * LaterChunkDataByteCount;
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
            if (chunkNumber == 0 || chunkNumber > ChunkCount || FirstChunkDataByteCount == 0)
            {
                return 0;
            }

            const size_t offset = OffsetOfChunk(chunkNumber);

            if (offset > ResourceByteCount)
            {
                return 0;
            }

            const size_t budget = (chunkNumber == 1) ? FirstChunkDataByteCount : LaterChunkDataByteCount;
            const size_t remaining = ResourceByteCount - offset;

            const auto thisChunk = static_cast<uint16_t>(remaining < budget ? remaining : budget);

            PropertyExchangeMessageFields fields{};

            fields.Type = MessageType::PropertyGetDataReply;
            fields.SourceMuid = sourceMuid;
            fields.DestinationMuid = destinationMuid;
            fields.RequestId = requestId;
            fields.Header = (chunkNumber == 1) ? Header : nullptr;
            fields.HeaderByteCount = (chunkNumber == 1) ? HeaderByteCount : (uint16_t)0;
            fields.ChunkCount = ChunkCount;
            fields.ChunkNumber = chunkNumber;
            fields.Data = (thisChunk > 0) ? (Resource + offset) : nullptr;
            fields.DataByteCount = thisChunk;

            return BuildPropertyExchangeMessage(fields, buffer, capacity);
        }
    };
}
