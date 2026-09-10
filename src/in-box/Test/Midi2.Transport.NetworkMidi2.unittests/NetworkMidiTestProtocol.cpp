// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://github.com/microsoft/MIDI/
// ============================================================================

#include "pch.h"

#include <sstream>

namespace NetworkMidiTest
{
    uint8_t PacketBuilder::PaddedWordCount(size_t const byteCount)
    {
        return static_cast<uint8_t>((byteCount + sizeof(uint32_t) - 1) / sizeof(uint32_t));
    }

    PacketBuilder& PacketBuilder::StartPacket()
    {
        m_bytes.clear();

        return AddUInt32(UdpPacketSignature);
    }

    PacketBuilder& PacketBuilder::AddByte(uint8_t const value)
    {
        m_bytes.push_back(value);

        return *this;
    }

    PacketBuilder& PacketBuilder::AddUInt16(uint16_t const value)
    {
        m_bytes.push_back(static_cast<uint8_t>((value >> 8) & 0xFF));
        m_bytes.push_back(static_cast<uint8_t>(value & 0xFF));

        return *this;
    }

    PacketBuilder& PacketBuilder::AddUInt32(uint32_t const value)
    {
        m_bytes.push_back(static_cast<uint8_t>((value >> 24) & 0xFF));
        m_bytes.push_back(static_cast<uint8_t>((value >> 16) & 0xFF));
        m_bytes.push_back(static_cast<uint8_t>((value >> 8) & 0xFF));
        m_bytes.push_back(static_cast<uint8_t>(value & 0xFF));

        return *this;
    }

    PacketBuilder& PacketBuilder::AddRawBytes(std::vector<uint8_t> const& bytes)
    {
        m_bytes.insert(m_bytes.end(), bytes.begin(), bytes.end());

        return *this;
    }

    PacketBuilder& PacketBuilder::AddCommandHeader(
        CommandCode const code,
        uint8_t const payloadLengthWords,
        uint8_t const commandSpecificData1,
        uint8_t const commandSpecificData2)
    {
        AddByte(static_cast<uint8_t>(code));
        AddByte(payloadLengthWords);
        AddByte(commandSpecificData1);
        AddByte(commandSpecificData2);

        return *this;
    }

    PacketBuilder& PacketBuilder::AddCommandHeader(
        CommandCode const code,
        uint8_t const payloadLengthWords,
        uint16_t const commandSpecificData)
    {
        AddByte(static_cast<uint8_t>(code));
        AddByte(payloadLengthWords);
        AddUInt16(commandSpecificData);

        return *this;
    }

    PacketBuilder& PacketBuilder::AddPaddedString(std::string const& value)
    {
        for (auto const& ch : value)
        {
            m_bytes.push_back(static_cast<uint8_t>(ch));
        }

        auto padding = (sizeof(uint32_t) - (value.size() % sizeof(uint32_t))) % sizeof(uint32_t);

        for (size_t i = 0; i < padding; i++)
        {
            m_bytes.push_back(0);
        }

        return *this;
    }

    PacketBuilder& PacketBuilder::AddInvitation(
        std::string const& endpointName,
        std::string const& productInstanceId,
        InvitationCapabilities const capabilities)
    {
        auto nameWords = PaddedWordCount(endpointName.size());
        auto productWords = PaddedWordCount(productInstanceId.size());

        AddCommandHeader(
            CommandCode::Invitation,
            static_cast<uint8_t>(nameWords + productWords),
            nameWords,
            static_cast<uint8_t>(capabilities));

        AddPaddedString(endpointName);
        AddPaddedString(productInstanceId);

        return *this;
    }

    PacketBuilder& PacketBuilder::AddPing(uint32_t const pingId)
    {
        AddCommandHeader(CommandCode::Ping, 1, static_cast<uint16_t>(0));

        return AddUInt32(pingId);
    }

    PacketBuilder& PacketBuilder::AddPingReply(uint32_t const pingId)
    {
        AddCommandHeader(CommandCode::PingReply, 1, static_cast<uint16_t>(0));

        return AddUInt32(pingId);
    }

    PacketBuilder& PacketBuilder::AddBye(ByeReason const reason, std::string const& text)
    {
        AddCommandHeader(
            CommandCode::Bye,
            PaddedWordCount(text.size()),
            static_cast<uint8_t>(reason),
            0);

        return AddPaddedString(text);
    }

    PacketBuilder& PacketBuilder::AddByeReply()
    {
        return AddCommandHeader(CommandCode::ByeReply, 0, static_cast<uint16_t>(0));
    }

    PacketBuilder& PacketBuilder::AddSessionReset()
    {
        return AddCommandHeader(CommandCode::SessionReset, 0, static_cast<uint16_t>(0));
    }

    PacketBuilder& PacketBuilder::AddSessionResetReply()
    {
        return AddCommandHeader(CommandCode::SessionResetReply, 0, static_cast<uint16_t>(0));
    }

    PacketBuilder& PacketBuilder::AddUmpData(uint16_t const sequenceNumber, std::vector<uint32_t> const& words)
    {
        AddCommandHeader(CommandCode::UmpData, static_cast<uint8_t>(words.size()), sequenceNumber);

        for (auto const& word : words)
        {
            AddUInt32(word);
        }

        return *this;
    }

    PacketBuilder& PacketBuilder::AddRetransmitRequest(uint16_t const sequenceNumber, uint16_t const commandCount)
    {
        AddCommandHeader(CommandCode::RetransmitRequest, 1, sequenceNumber);

        AddUInt16(commandCount);
        AddUInt16(0);       // reserved

        return *this;
    }

    PacketBuilder& PacketBuilder::AddNak(uint32_t const originalCommandHeader, NakReason const reason, std::string const& text)
    {
        // payload is the original command header word plus optional text
        AddCommandHeader(
            CommandCode::Nak,
            static_cast<uint8_t>(1 + PaddedWordCount(text.size())),
            static_cast<uint8_t>(reason),
            0);

        AddUInt32(originalCommandHeader);

        return AddPaddedString(text);
    }


    uint32_t ReceivedCommand::GetPayloadUInt32(size_t const wordIndex) const
    {
        auto offset = wordIndex * sizeof(uint32_t);

        if (offset + sizeof(uint32_t) > Payload.size())
        {
            return 0;
        }

        return (static_cast<uint32_t>(Payload[offset]) << 24) |
            (static_cast<uint32_t>(Payload[offset + 1]) << 16) |
            (static_cast<uint32_t>(Payload[offset + 2]) << 8) |
            static_cast<uint32_t>(Payload[offset + 3]);
    }

    std::string ReceivedCommand::GetPayloadString(size_t const byteOffset, size_t const byteCount) const
    {
        if (byteOffset >= Payload.size())
        {
            return std::string{ };
        }

        auto available = min(byteCount, Payload.size() - byteOffset);

        std::string result(reinterpret_cast<char const*>(Payload.data()) + byteOffset, available);

        // strings are zero padded out to a word boundary
        auto firstNull = result.find('\0');

        if (firstNull != std::string::npos)
        {
            result.resize(firstNull);
        }

        return result;
    }


    bool ParsedPacket::Contains(CommandCode const code) const
    {
        return Find(code) != nullptr;
    }

    ReceivedCommand const* ParsedPacket::Find(CommandCode const code) const
    {
        for (auto const& command : Commands)
        {
            if (command.Code == code)
            {
                return &command;
            }
        }

        return nullptr;
    }

    size_t ParsedPacket::Count(CommandCode const code) const
    {
        size_t count{ 0 };

        for (auto const& command : Commands)
        {
            if (command.Code == code)
            {
                count++;
            }
        }

        return count;
    }


    ParsedPacket ParsePacket(uint8_t const* bytes, size_t const byteCount)
    {
        ParsedPacket packet{ };

        if (bytes == nullptr || byteCount < sizeof(uint32_t))
        {
            packet.Truncated = true;

            return packet;
        }

        size_t position{ 0 };

        uint32_t signature =
            (static_cast<uint32_t>(bytes[0]) << 24) |
            (static_cast<uint32_t>(bytes[1]) << 16) |
            (static_cast<uint32_t>(bytes[2]) << 8) |
            static_cast<uint32_t>(bytes[3]);

        packet.SignatureValid = (signature == UdpPacketSignature);

        if (!packet.SignatureValid)
        {
            return packet;
        }

        position += sizeof(uint32_t);

        while (position + sizeof(uint32_t) <= byteCount)
        {
            ReceivedCommand command{ };

            command.Code = static_cast<CommandCode>(bytes[position]);
            command.PayloadLengthWords = bytes[position + 1];
            command.CommandSpecificData1 = bytes[position + 2];
            command.CommandSpecificData2 = bytes[position + 3];
            command.CommandSpecificDataUInt16 =
                static_cast<uint16_t>((static_cast<uint16_t>(bytes[position + 2]) << 8) | bytes[position + 3]);

            command.HeaderWord =
                (static_cast<uint32_t>(bytes[position]) << 24) |
                (static_cast<uint32_t>(bytes[position + 1]) << 16) |
                (static_cast<uint32_t>(bytes[position + 2]) << 8) |
                static_cast<uint32_t>(bytes[position + 3]);

            position += sizeof(uint32_t);

            size_t payloadBytes = static_cast<size_t>(command.PayloadLengthWords) * sizeof(uint32_t);

            if (position + payloadBytes > byteCount)
            {
                packet.Truncated = true;

                // keep the command so a test can assert on what did arrive
                command.Payload.assign(bytes + position, bytes + byteCount);
                packet.Commands.push_back(command);

                break;
            }

            command.Payload.assign(bytes + position, bytes + position + payloadBytes);
            position += payloadBytes;

            packet.Commands.push_back(command);
        }

        return packet;
    }


    std::wstring CommandCodeToString(CommandCode const code)
    {
        switch (code)
        {
        case CommandCode::Invitation:                               return L"Invitation";
        case CommandCode::InvitationWithAuthentication:             return L"InvitationWithAuthentication";
        case CommandCode::InvitationWithUserAuthentication:         return L"InvitationWithUserAuthentication";
        case CommandCode::InvitationReplyAccepted:                  return L"InvitationReplyAccepted";
        case CommandCode::InvitationReplyPending:                   return L"InvitationReplyPending";
        case CommandCode::InvitationReplyAuthenticationRequired:    return L"InvitationReplyAuthenticationRequired";
        case CommandCode::InvitationReplyUserAuthenticationRequired:return L"InvitationReplyUserAuthenticationRequired";
        case CommandCode::Ping:                                     return L"Ping";
        case CommandCode::PingReply:                                return L"PingReply";
        case CommandCode::RetransmitRequest:                        return L"RetransmitRequest";
        case CommandCode::RetransmitError:                          return L"RetransmitError";
        case CommandCode::SessionReset:                             return L"SessionReset";
        case CommandCode::SessionResetReply:                        return L"SessionResetReply";
        case CommandCode::Nak:                                      return L"NAK";
        case CommandCode::Bye:                                      return L"Bye";
        case CommandCode::ByeReply:                                 return L"ByeReply";
        case CommandCode::UmpData:                                  return L"UmpData";
        default:                                                    return L"Unknown";
        }
    }

    std::wstring DescribePacket(ParsedPacket const& packet)
    {
        std::wstringstream ss;

        if (!packet.SignatureValid)
        {
            ss << L"[invalid signature]";

            return ss.str();
        }

        ss << L"[";

        for (size_t i = 0; i < packet.Commands.size(); i++)
        {
            if (i > 0)
            {
                ss << L", ";
            }

            auto const& command = packet.Commands[i];

            ss << CommandCodeToString(command.Code)
                << L"(0x" << std::hex << static_cast<int>(command.Code) << std::dec
                << L", payloadWords=" << static_cast<int>(command.PayloadLengthWords);

            if (command.Code == CommandCode::Bye)
            {
                ss << L", reason=0x" << std::hex << static_cast<int>(command.CommandSpecificData1) << std::dec;
            }
            else if (command.Code == CommandCode::Nak)
            {
                ss << L", reason=0x" << std::hex << static_cast<int>(command.CommandSpecificData1) << std::dec;
            }
            else if (command.Code == CommandCode::UmpData)
            {
                ss << L", seq=" << command.CommandSpecificDataUInt16;
            }

            ss << L")";
        }

        if (packet.Truncated)
        {
            ss << L" [truncated]";
        }

        ss << L"]";

        return ss.str();
    }


    namespace
    {
        // Word 0 of every Stream message: type 0xF, then the form, then a 10 bit status.
        uint32_t StreamWord0(_In_ uint8_t const form, _In_ uint16_t const status, _In_ uint16_t const remainder)
        {
            return
                (static_cast<uint32_t>(UmpMessageTypeStream) << 28) |
                (static_cast<uint32_t>(form & 0x3) << 26) |
                (static_cast<uint32_t>(status & 0x3FF) << 16) |
                static_cast<uint32_t>(remainder);
        }

        uint16_t StreamStatusOf(_In_ uint32_t const word0)
        {
            return static_cast<uint16_t>((word0 >> 16) & 0x3FF);
        }

        bool IsStreamWord0(_In_ uint32_t const word0)
        {
            return ((word0 >> 28) & 0xF) == UmpMessageTypeStream;
        }

        // Text notifications pack their characters after the fixed fields. How many land in
        // word 0 follows from the per-packet character count, and the rest fill words 1 to 3.
        std::vector<uint32_t> BuildTextNotification(
            _In_ uint16_t const status,
            _In_ uint16_t const word0Remainder,
            _In_ size_t const maxBytes,
            _In_ size_t const bytesPerPacket,
            _In_ std::string const& text)
        {
            std::vector<uint32_t> words{ };

            auto const truncated = text.substr(0, maxBytes);

            size_t packetCount = truncated.size() / bytesPerPacket;
            if (truncated.size() % bytesPerPacket != 0 || packetCount == 0) packetCount++;

            size_t offset = 0;

            for (size_t packet = 0; packet < packetCount; packet++)
            {
                uint8_t form;

                if (packetCount == 1)               form = StreamFormComplete;
                else if (packet == 0)               form = StreamFormStart;
                else if (packet == packetCount - 1) form = StreamFormEnd;
                else                                form = StreamFormContinue;

                uint8_t payload[14]{ 0 };

                for (size_t i = 0; i < bytesPerPacket && offset < truncated.size(); i++)
                {
                    payload[i] = static_cast<uint8_t>(truncated[offset++]);
                }

                uint32_t word0 = StreamWord0(form, status, word0Remainder);

                size_t index = 0;

                // 14 characters per packet leaves two in word 0, 13 leaves one
                if (bytesPerPacket % 4 == 2)
                {
                    word0 |= static_cast<uint32_t>(payload[index++]) << 8;
                }

                if (bytesPerPacket % 4 >= 1)
                {
                    word0 |= static_cast<uint32_t>(payload[index++]);
                }

                words.push_back(word0);

                for (size_t word = 0; word < 3; word++)
                {
                    words.push_back(
                        static_cast<uint32_t>(payload[index]) << 24 |
                        static_cast<uint32_t>(payload[index + 1]) << 16 |
                        static_cast<uint32_t>(payload[index + 2]) << 8 |
                        static_cast<uint32_t>(payload[index + 3]));

                    index += 4;
                }
            }

            return words;
        }
    }

    _Use_decl_annotations_
    bool IsStreamMessageWithStatus(std::vector<uint32_t> const& words, uint16_t const status)
    {
        return
            words.size() >= 4 &&
            IsStreamWord0(words[0]) &&
            StreamStatusOf(words[0]) == status;
    }

    _Use_decl_annotations_
    bool ContainsStreamMessageWithStatus(std::vector<uint32_t> const& words, uint16_t const status)
    {
        // A Stream message is always four words, so anything in the run which is not one cannot
        // be stepped over reliably. Discovery requests arrive on their own, so scanning on a four
        // word stride is enough and avoids decoding every message type.
        for (size_t i = 0; i + 3 < words.size(); i += 4)
        {
            if (IsStreamWord0(words[i]) && StreamStatusOf(words[i]) == status)
            {
                return true;
            }
        }

        return false;
    }

    _Use_decl_annotations_
    std::vector<uint32_t> BuildEndpointInfoNotification(uint8_t const functionBlockCount, bool const staticFunctionBlocks)
    {
        // UMP version 1.1
        uint16_t const word0Remainder = (static_cast<uint16_t>(1) << 8) | 1;

        uint32_t word1 = static_cast<uint32_t>(functionBlockCount & 0x7F) << 24;

        if (staticFunctionBlocks) word1 |= 0x80000000;

        word1 |= 0x0100;    // supports MIDI 1.0 protocol
        word1 |= 0x0200;    // supports MIDI 2.0 protocol

        return { StreamWord0(StreamFormComplete, StreamStatusEndpointInfoNotification, word0Remainder), word1, 0, 0 };
    }

    _Use_decl_annotations_
    std::vector<uint32_t> BuildFunctionBlockInfoNotification(FunctionBlockDescription const& block)
    {
        uint16_t word0Remainder{ 0 };

        word0Remainder |= static_cast<uint16_t>(block.Number & 0x7F) << 8;
        word0Remainder |= static_cast<uint16_t>(block.Direction);

        // the active flag is the high bit of the function block number byte
        if (block.IsActive) word0Remainder |= 0x8000;

        uint32_t const word1 =
            static_cast<uint32_t>(block.FirstGroup) << 24 |
            static_cast<uint32_t>(block.GroupCount) << 16;

        return { StreamWord0(StreamFormComplete, StreamStatusFunctionBlockInfoNotification, word0Remainder), word1, 0, 0 };
    }

    _Use_decl_annotations_
    std::vector<uint32_t> BuildEndpointNameNotification(std::string const& name)
    {
        return BuildTextNotification(StreamStatusEndpointNameNotification, 0, 98, 14, name);
    }

    _Use_decl_annotations_
    std::vector<uint32_t> BuildProductInstanceIdNotification(std::string const& productInstanceId)
    {
        return BuildTextNotification(StreamStatusProductInstanceIdNotification, 0, 42, 14, productInstanceId);
    }

    _Use_decl_annotations_
    std::vector<uint32_t> BuildFunctionBlockNameNotification(uint8_t const functionBlockNumber, std::string const& name)
    {
        // the block number sits where the second character would otherwise go
        return BuildTextNotification(
            StreamStatusFunctionBlockNameNotification,
            static_cast<uint16_t>(functionBlockNumber) << 8,
            91,
            13,
            name);
    }
}
