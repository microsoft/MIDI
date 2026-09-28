// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// RTP-MIDI (RFC 6295) payload codec and the AppleMIDI session protocol.
//
// Header only, with no Winsock, WinRT or COM dependency, so it can be unit tested and later
// moved into a service transport as-is. Every byte comes from the network: every read is
// bounds checked and a malformed datagram is rejected rather than partially trusted.
// ============================================================================

#pragma once

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <optional>
#include <string>
#include <vector>

namespace RtpMidi
{
    // ------------------------------------------------------------------------------------------
    // Big-endian reading and writing
    // ------------------------------------------------------------------------------------------

    class Reader
    {
    public:
        Reader(uint8_t const* data, size_t size) : m_data(data), m_size(size) {}

        size_t Position() const { return m_position; }
        size_t Remaining() const { return m_size - m_position; }
        uint8_t const* Current() const { return m_data + m_position; }

        bool Skip(size_t count)
        {
            if (count > Remaining()) return false;
            m_position += count;
            return true;
        }

        bool U8(uint8_t& value)
        {
            if (Remaining() < 1) return false;
            value = m_data[m_position++];
            return true;
        }

        bool U16(uint16_t& value)
        {
            if (Remaining() < 2) return false;
            value = static_cast<uint16_t>((m_data[m_position] << 8) | m_data[m_position + 1]);
            m_position += 2;
            return true;
        }

        bool U32(uint32_t& value)
        {
            if (Remaining() < 4) return false;
            value = (static_cast<uint32_t>(m_data[m_position]) << 24) |
                (static_cast<uint32_t>(m_data[m_position + 1]) << 16) |
                (static_cast<uint32_t>(m_data[m_position + 2]) << 8) |
                static_cast<uint32_t>(m_data[m_position + 3]);
            m_position += 4;
            return true;
        }

        bool U64(uint64_t& value)
        {
            uint32_t high{ 0 };
            uint32_t low{ 0 };
            if (!U32(high) || !U32(low)) return false;
            value = (static_cast<uint64_t>(high) << 32) | low;
            return true;
        }

    private:
        uint8_t const* m_data{ nullptr };
        size_t m_size{ 0 };
        size_t m_position{ 0 };
    };

    inline void PutU8(std::vector<uint8_t>& out, uint8_t value) { out.push_back(value); }

    inline void PutU16(std::vector<uint8_t>& out, uint16_t value)
    {
        out.push_back(static_cast<uint8_t>(value >> 8));
        out.push_back(static_cast<uint8_t>(value));
    }

    inline void PutU32(std::vector<uint8_t>& out, uint32_t value)
    {
        out.push_back(static_cast<uint8_t>(value >> 24));
        out.push_back(static_cast<uint8_t>(value >> 16));
        out.push_back(static_cast<uint8_t>(value >> 8));
        out.push_back(static_cast<uint8_t>(value));
    }

    inline void PutU64(std::vector<uint8_t>& out, uint64_t value)
    {
        PutU32(out, static_cast<uint32_t>(value >> 32));
        PutU32(out, static_cast<uint32_t>(value));
    }


    // ------------------------------------------------------------------------------------------
    // Names from the network
    // ------------------------------------------------------------------------------------------

    // Keeps well-formed UTF-8 and drops control characters. A remote name ends up in endpoint
    // names, port names and log lines, so it is never passed through raw.
    inline std::string SanitizeUtf8Name(std::string const& input, size_t maxBytes)
    {
        std::string output;
        output.reserve((std::min)(input.size(), maxBytes));

        size_t i = 0;
        while (i < input.size())
        {
            auto const lead = static_cast<uint8_t>(input[i]);
            size_t length = 0;
            uint32_t codePoint = 0;

            if (lead < 0x80) { length = 1; codePoint = lead; }
            else if ((lead & 0xE0) == 0xC0) { length = 2; codePoint = lead & 0x1F; }
            else if ((lead & 0xF0) == 0xE0) { length = 3; codePoint = lead & 0x0F; }
            else if ((lead & 0xF8) == 0xF0) { length = 4; codePoint = lead & 0x07; }
            else { i++; continue; }

            if (i + length > input.size()) break;

            bool valid = true;
            for (size_t k = 1; k < length; k++)
            {
                auto const next = static_cast<uint8_t>(input[i + k]);
                if ((next & 0xC0) != 0x80) { valid = false; break; }
                codePoint = (codePoint << 6) | (next & 0x3F);
            }

            // overlong forms, surrogates and out of range values are not characters
            if (valid)
            {
                if ((length == 2 && codePoint < 0x80) ||
                    (length == 3 && codePoint < 0x800) ||
                    (length == 4 && (codePoint < 0x10000 || codePoint > 0x10FFFF)) ||
                    (codePoint >= 0xD800 && codePoint <= 0xDFFF))
                {
                    valid = false;
                }
            }

            if (!valid) { i++; continue; }

            bool const isControl = codePoint < 0x20 || codePoint == 0x7F || (codePoint >= 0x80 && codePoint <= 0x9F);

            if (!isControl)
            {
                if (output.size() + length > maxBytes) break;
                output.append(input, i, length);
            }

            i += length;
        }

        return output;
    }


    // ------------------------------------------------------------------------------------------
    // AppleMIDI session protocol
    //
    // Every packet is 0xFFFF followed by a two-letter command. IN, OK, NO and BY share one
    // layout. CK is the three-step clock exchange, RS is receiver feedback for the recovery
    // journal, RL asks the sender to limit its bit rate.
    // ------------------------------------------------------------------------------------------

    constexpr uint16_t AppleMidiSignature = 0xFFFF;
    constexpr uint32_t AppleMidiProtocolVersion = 2;

    // Far beyond any real session name, and the cap on what is read from a remote.
    constexpr size_t AppleMidiMaxNameBytes = 256;

    enum class AppleMidiCommand : uint16_t
    {
        Invitation = 0x494E,            // IN
        InvitationAccepted = 0x4F4B,    // OK
        InvitationRejected = 0x4E4F,    // NO
        EndSession = 0x4259,            // BY
        Synchronization = 0x434B,       // CK
        ReceiverFeedback = 0x5253,      // RS
        BitrateReceiveLimit = 0x524C,   // RL
    };

    inline char const* AppleMidiCommandName(AppleMidiCommand command)
    {
        switch (command)
        {
        case AppleMidiCommand::Invitation: return "IN";
        case AppleMidiCommand::InvitationAccepted: return "OK";
        case AppleMidiCommand::InvitationRejected: return "NO";
        case AppleMidiCommand::EndSession: return "BY";
        case AppleMidiCommand::Synchronization: return "CK";
        case AppleMidiCommand::ReceiverFeedback: return "RS";
        case AppleMidiCommand::BitrateReceiveLimit: return "RL";
        default: return "??";
        }
    }

    struct AppleMidiInvitation
    {
        AppleMidiCommand Command{ AppleMidiCommand::Invitation };
        uint32_t ProtocolVersion{ AppleMidiProtocolVersion };
        uint32_t InitiatorToken{ 0 };
        uint32_t Ssrc{ 0 };
        std::string Name;
    };

    struct AppleMidiSynchronization
    {
        uint32_t Ssrc{ 0 };
        uint8_t Count{ 0 };
        std::array<uint64_t, 3> Timestamps{};
    };

    struct AppleMidiReceiverFeedback
    {
        uint32_t Ssrc{ 0 };

        // Implementations disagree on the layout of this field. Apple describes a 32-bit extended
        // sequence number, so the RTP sequence number is in the low 16 bits; some peers put it in
        // the high 16 bits instead. Kept raw so the caller can tell which it got.
        uint32_t SequenceField{ 0 };
    };

    struct AppleMidiBitrateLimit
    {
        uint32_t Ssrc{ 0 };
        uint32_t BitsPerSecond{ 0 };
    };

    inline bool TryGetAppleMidiCommand(uint8_t const* data, size_t size, AppleMidiCommand& command)
    {
        if (data == nullptr || size < 4) return false;
        if (data[0] != 0xFF || data[1] != 0xFF) return false;

        command = static_cast<AppleMidiCommand>((data[2] << 8) | data[3]);

        switch (command)
        {
        case AppleMidiCommand::Invitation:
        case AppleMidiCommand::InvitationAccepted:
        case AppleMidiCommand::InvitationRejected:
        case AppleMidiCommand::EndSession:
        case AppleMidiCommand::Synchronization:
        case AppleMidiCommand::ReceiverFeedback:
        case AppleMidiCommand::BitrateReceiveLimit:
            return true;
        default:
            return false;
        }
    }

    inline std::optional<AppleMidiInvitation> ParseInvitation(uint8_t const* data, size_t size)
    {
        AppleMidiCommand command{};
        if (!TryGetAppleMidiCommand(data, size, command)) return std::nullopt;

        if (command != AppleMidiCommand::Invitation &&
            command != AppleMidiCommand::InvitationAccepted &&
            command != AppleMidiCommand::InvitationRejected &&
            command != AppleMidiCommand::EndSession)
        {
            return std::nullopt;
        }

        Reader reader{ data, size };
        reader.Skip(4);

        AppleMidiInvitation message{};
        message.Command = command;

        if (!reader.U32(message.ProtocolVersion) ||
            !reader.U32(message.InitiatorToken) ||
            !reader.U32(message.Ssrc))
        {
            return std::nullopt;
        }

        // optional, NUL terminated, but a peer that forgets the NUL still gets its name read
        auto const name = reinterpret_cast<char const*>(reader.Current());
        size_t length = 0;
        while (length < reader.Remaining() && length < AppleMidiMaxNameBytes && name[length] != '\0')
        {
            length++;
        }

        message.Name = SanitizeUtf8Name(std::string{ name, length }, AppleMidiMaxNameBytes);

        return message;
    }

    inline std::optional<AppleMidiSynchronization> ParseSynchronization(uint8_t const* data, size_t size)
    {
        AppleMidiCommand command{};
        if (!TryGetAppleMidiCommand(data, size, command) || command != AppleMidiCommand::Synchronization) return std::nullopt;

        Reader reader{ data, size };
        reader.Skip(4);

        AppleMidiSynchronization message{};

        if (!reader.U32(message.Ssrc) || !reader.U8(message.Count) || !reader.Skip(3) ||
            !reader.U64(message.Timestamps[0]) || !reader.U64(message.Timestamps[1]) || !reader.U64(message.Timestamps[2]))
        {
            return std::nullopt;
        }

        if (message.Count > 2) return std::nullopt;

        return message;
    }

    inline std::optional<AppleMidiReceiverFeedback> ParseReceiverFeedback(uint8_t const* data, size_t size)
    {
        AppleMidiCommand command{};
        if (!TryGetAppleMidiCommand(data, size, command) || command != AppleMidiCommand::ReceiverFeedback) return std::nullopt;

        Reader reader{ data, size };
        reader.Skip(4);

        AppleMidiReceiverFeedback message{};
        if (!reader.U32(message.Ssrc) || !reader.U32(message.SequenceField)) return std::nullopt;

        return message;
    }

    inline std::optional<AppleMidiBitrateLimit> ParseBitrateLimit(uint8_t const* data, size_t size)
    {
        AppleMidiCommand command{};
        if (!TryGetAppleMidiCommand(data, size, command) || command != AppleMidiCommand::BitrateReceiveLimit) return std::nullopt;

        Reader reader{ data, size };
        reader.Skip(4);

        AppleMidiBitrateLimit message{};
        if (!reader.U32(message.Ssrc) || !reader.U32(message.BitsPerSecond)) return std::nullopt;

        return message;
    }

    inline std::vector<uint8_t> BuildInvitation(AppleMidiCommand command, uint32_t initiatorToken, uint32_t ssrc, std::string const& name)
    {
        std::vector<uint8_t> out;
        out.reserve(16 + name.size() + 1);

        PutU16(out, AppleMidiSignature);
        PutU16(out, static_cast<uint16_t>(command));
        PutU32(out, AppleMidiProtocolVersion);
        PutU32(out, initiatorToken);
        PutU32(out, ssrc);

        // BY never carries a name; IN and OK always do, even if it is empty
        if (command == AppleMidiCommand::Invitation || command == AppleMidiCommand::InvitationAccepted)
        {
            auto const length = (std::min)(name.size(), AppleMidiMaxNameBytes - 1);
            out.insert(out.end(), name.begin(), name.begin() + static_cast<std::ptrdiff_t>(length));
            out.push_back(0);
        }

        return out;
    }

    inline std::vector<uint8_t> BuildSynchronization(uint32_t ssrc, uint8_t count, std::array<uint64_t, 3> const& timestamps)
    {
        std::vector<uint8_t> out;
        out.reserve(36);

        PutU16(out, AppleMidiSignature);
        PutU16(out, static_cast<uint16_t>(AppleMidiCommand::Synchronization));
        PutU32(out, ssrc);
        PutU8(out, count);
        PutU8(out, 0);
        PutU16(out, 0);
        PutU64(out, timestamps[0]);
        PutU64(out, timestamps[1]);
        PutU64(out, timestamps[2]);

        return out;
    }

    inline std::vector<uint8_t> BuildReceiverFeedback(uint32_t ssrc, uint32_t sequenceField)
    {
        std::vector<uint8_t> out;
        out.reserve(12);

        PutU16(out, AppleMidiSignature);
        PutU16(out, static_cast<uint16_t>(AppleMidiCommand::ReceiverFeedback));
        PutU32(out, ssrc);
        PutU32(out, sequenceField);

        return out;
    }


    // ------------------------------------------------------------------------------------------
    // RTP header (RFC 3550 section 5.1)
    // ------------------------------------------------------------------------------------------

    // Dynamic payload type 97, which every AppleMIDI implementation uses. No SDP is exchanged.
    constexpr uint8_t RtpMidiPayloadType = 0x61;
    constexpr size_t RtpFixedHeaderSize = 12;

    struct RtpHeader
    {
        bool Marker{ false };
        uint8_t PayloadType{ 0 };
        uint16_t SequenceNumber{ 0 };
        uint32_t Timestamp{ 0 };
        uint32_t Ssrc{ 0 };
        size_t PayloadOffset{ 0 };
        size_t PayloadSize{ 0 };
    };

    inline bool ParseRtpHeader(uint8_t const* data, size_t size, RtpHeader& header)
    {
        if (data == nullptr || size < RtpFixedHeaderSize) return false;

        auto const first = data[0];
        if ((first >> 6) != 2) return false;

        bool const hasPadding = (first & 0x20) != 0;
        bool const hasExtension = (first & 0x10) != 0;
        size_t const csrcCount = first & 0x0F;

        header.Marker = (data[1] & 0x80) != 0;
        header.PayloadType = data[1] & 0x7F;

        Reader reader{ data, size };
        reader.Skip(2);
        reader.U16(header.SequenceNumber);
        reader.U32(header.Timestamp);
        reader.U32(header.Ssrc);

        if (!reader.Skip(csrcCount * 4)) return false;

        if (hasExtension)
        {
            uint16_t profile{ 0 };
            uint16_t lengthInWords{ 0 };
            if (!reader.U16(profile) || !reader.U16(lengthInWords)) return false;
            if (!reader.Skip(static_cast<size_t>(lengthInWords) * 4)) return false;
        }

        size_t end = size;

        if (hasPadding)
        {
            auto const padding = data[size - 1];
            if (padding == 0 || padding > end - reader.Position()) return false;
            end -= padding;
        }

        header.PayloadOffset = reader.Position();
        header.PayloadSize = end - reader.Position();

        return true;
    }

    // The marker bit is deliberately left clear. RFC 6295 says to set it when the MIDI list is
    // not empty, but macOS and rtpMIDI for Windows both ignore the MIDI in packets that do.
    inline void WriteRtpHeader(std::vector<uint8_t>& out, uint16_t sequence, uint32_t timestamp, uint32_t ssrc)
    {
        PutU8(out, 0x80);
        PutU8(out, RtpMidiPayloadType);
        PutU16(out, sequence);
        PutU32(out, timestamp);
        PutU32(out, ssrc);
    }


    // ------------------------------------------------------------------------------------------
    // MIDI 1.0 command lengths
    // ------------------------------------------------------------------------------------------

    constexpr int SysExLength = -1;
    constexpr int UndefinedSystemCommon = -2;

    // Data bytes after a status byte, or a negative marker for commands scanned to a terminator.
    inline int DataByteCount(uint8_t status)
    {
        switch (status & 0xF0)
        {
        case 0x80: case 0x90: case 0xA0: case 0xB0: case 0xE0: return 2;
        case 0xC0: case 0xD0: return 1;
        default: break;
        }

        switch (status)
        {
        case 0xF0: case 0xF7: return SysExLength;
        case 0xF1: case 0xF3: return 1;
        case 0xF2: return 2;
        case 0xF4: case 0xF5: return UndefinedSystemCommon;
        default: return 0;  // F6 and real time
        }
    }

    inline bool IsRealTime(uint8_t byte) { return byte >= 0xF8; }


    // ------------------------------------------------------------------------------------------
    // MIDI command section (RFC 6295 section 3)
    // ------------------------------------------------------------------------------------------

    constexpr uint8_t CommandSectionFlagB = 0x80;   // two-byte header, 12-bit LEN
    constexpr uint8_t CommandSectionFlagJ = 0x40;   // a journal section follows the MIDI list
    constexpr uint8_t CommandSectionFlagZ = 0x20;   // the first command has a delta time
    constexpr uint8_t CommandSectionFlagP = 0x10;   // status of the first command was not in the source

    constexpr size_t MaxShortListLength = 0x0F;
    constexpr size_t MaxLongListLength = 0x0FFF;

    // One decoded command. Bytes are a MIDI 1.0 byte stream fragment: concatenated in order, the
    // fragments of a stream form a valid byte stream, including SysEx split across packets.
    struct MidiEvent
    {
        uint32_t Timestamp{ 0 };   // sender's RTP timestamp plus the command's delta time
        std::vector<uint8_t> Bytes;
    };

    enum class DecodeStatus
    {
        Ok,
        NotRtpMidi,
        Malformed,
    };

    struct DecodedPacket
    {
        RtpHeader Header{};
        bool HasJournal{ false };
        bool PhantomStatus{ false };
        size_t JournalOffset{ 0 };      // into the datagram
        size_t JournalSize{ 0 };
        std::vector<MidiEvent> Events;
        uint32_t MalformedCommands{ 0 };
        uint32_t DroppedSysExSegments{ 0 };
    };

    // 1 to 4 octets, 7 bits each, high bit set on all but the last (RFC 6295 figure 4).
    inline bool ReadDeltaTime(uint8_t const* data, size_t end, size_t& position, uint32_t& delta)
    {
        delta = 0;

        for (int octet = 0; octet < 4; octet++)
        {
            if (position >= end) return false;

            auto const byte = data[position++];
            delta = (delta << 7) | (byte & 0x7F);

            if ((byte & 0x80) == 0) return true;
        }

        return false;
    }

    // One per remote stream. It remembers only whether a segmented SysEx is open, because that is
    // the only state which legitimately spans packets.
    class CommandSectionDecoder
    {
    public:
        bool SysExOpen() const { return m_sysExOpen; }
        void Reset() { m_sysExOpen = false; }

        // A lost packet can take the middle of a SysEx with it. Closing it here keeps the stream
        // well formed; the receiving app sees a short SysEx instead of two joined ones.
        bool CloseOpenSysEx(std::vector<uint8_t>& out)
        {
            if (!m_sysExOpen) return false;
            out.push_back(0xF7);
            m_sysExOpen = false;
            return true;
        }

        DecodeStatus Decode(uint8_t const* datagram, size_t size, DecodedPacket& packet)
        {
            packet = DecodedPacket{};

            if (!ParseRtpHeader(datagram, size, packet.Header)) return DecodeStatus::NotRtpMidi;
            if (packet.Header.PayloadType != RtpMidiPayloadType) return DecodeStatus::NotRtpMidi;

            auto const payload = datagram + packet.Header.PayloadOffset;
            auto const payloadSize = packet.Header.PayloadSize;

            if (payloadSize < 1) return DecodeStatus::Malformed;

            auto const flags = payload[0];
            size_t listLength = flags & 0x0F;
            size_t headerSize = 1;

            if (flags & CommandSectionFlagB)
            {
                if (payloadSize < 2) return DecodeStatus::Malformed;
                listLength = (listLength << 8) | payload[1];
                headerSize = 2;
            }

            if (headerSize + listLength > payloadSize) return DecodeStatus::Malformed;

            packet.HasJournal = (flags & CommandSectionFlagJ) != 0;
            packet.PhantomStatus = (flags & CommandSectionFlagP) != 0;

            if (packet.HasJournal)
            {
                packet.JournalOffset = packet.Header.PayloadOffset + headerSize + listLength;
                packet.JournalSize = payloadSize - headerSize - listLength;
            }

            DecodeList(payload + headerSize, listLength, (flags & CommandSectionFlagZ) != 0, packet);

            return DecodeStatus::Ok;
        }

    private:
        void Emit(DecodedPacket& packet, uint32_t timestamp, uint8_t const* bytes, size_t count)
        {
            MidiEvent event{};
            event.Timestamp = timestamp;
            event.Bytes.assign(bytes, bytes + count);
            packet.Events.push_back(std::move(event));
        }

        void EmitSysEx(DecodedPacket& packet, uint32_t timestamp, uint8_t head, uint8_t const* body, size_t bodyLength, uint8_t tail)
        {
            MidiEvent event{};
            event.Timestamp = timestamp;

            // a segment's head and tail are framing, not MIDI: first F0..F0, middle F7..F0,
            // last F7..F7, cancel F7 F4, and F5 marks a source SysEx whose F7 was dropped
            bool const isStart = head == 0xF0;
            bool const endsTransfer = tail == 0xF7 || tail == 0xF5;
            bool const continues = tail == 0xF0;
            bool const cancels = tail == 0xF4;

            if (isStart)
            {
                if (m_sysExOpen)
                {
                    // the previous transfer never finished, so close it before starting another
                    event.Bytes.push_back(0xF7);
                }

                if (cancels)
                {
                    packet.DroppedSysExSegments++;
                    m_sysExOpen = false;
                    if (!event.Bytes.empty()) packet.Events.push_back(std::move(event));
                    return;
                }

                event.Bytes.push_back(0xF0);
                event.Bytes.insert(event.Bytes.end(), body, body + bodyLength);

                if (endsTransfer) event.Bytes.push_back(0xF7);

                m_sysExOpen = continues;
            }
            else
            {
                if (!m_sysExOpen)
                {
                    // the start of this transfer was never seen, so the rest cannot be used
                    packet.DroppedSysExSegments++;
                    return;
                }

                if (cancels)
                {
                    event.Bytes.push_back(0xF7);
                    m_sysExOpen = false;
                }
                else
                {
                    event.Bytes.insert(event.Bytes.end(), body, body + bodyLength);

                    if (endsTransfer)
                    {
                        event.Bytes.push_back(0xF7);
                        m_sysExOpen = false;
                    }
                }
            }

            if (!event.Bytes.empty()) packet.Events.push_back(std::move(event));
        }

        void DecodeList(uint8_t const* list, size_t length, bool firstHasDelta, DecodedPacket& packet)
        {
            size_t position = 0;
            uint32_t timestamp = packet.Header.Timestamp;

            // the first channel command in every list carries its status, so this never spans packets
            uint8_t runningStatus = 0;

            if (firstHasDelta)
            {
                uint32_t delta = 0;
                if (!ReadDeltaTime(list, length, position, delta)) { packet.MalformedCommands++; return; }
                timestamp += delta;
            }

            while (position < length)
            {
                auto const lead = list[position];

                if (IsRealTime(lead))
                {
                    Emit(packet, timestamp, &list[position], 1);
                    position++;
                }
                else if (lead == 0xF0 || lead == 0xF7)
                {
                    // real time bytes may sit inside a segment and are legal MIDI 1.0 there
                    size_t scan = position + 1;
                    while (scan < length && (list[scan] < 0x80 || IsRealTime(list[scan]))) scan++;

                    if (scan >= length) { packet.MalformedCommands++; return; }

                    auto const tail = list[scan];

                    if (tail != 0xF0 && tail != 0xF7 && tail != 0xF4 && tail != 0xF5)
                    {
                        packet.MalformedCommands++;
                        return;
                    }

                    EmitSysEx(packet, timestamp, lead, &list[position + 1], scan - position - 1, tail);

                    position = scan + 1;
                    runningStatus = 0;
                }
                else if (lead >= 0x80)
                {
                    auto const dataBytes = DataByteCount(lead);

                    if (dataBytes == UndefinedSystemCommon)
                    {
                        // only legal when negotiated, which AppleMIDI never does; skip through its F7
                        size_t scan = position + 1;
                        while (scan < length && list[scan] < 0x80) scan++;
                        position = (scan < length && list[scan] == 0xF7) ? scan + 1 : scan;
                        runningStatus = 0;
                        packet.MalformedCommands++;
                    }
                    else
                    {
                        auto const count = static_cast<size_t>(dataBytes);

                        if (position + 1 + count > length) { packet.MalformedCommands++; return; }

                        for (size_t k = 1; k <= count; k++)
                        {
                            if (list[position + k] >= 0x80) { packet.MalformedCommands++; return; }
                        }

                        Emit(packet, timestamp, &list[position], 1 + count);

                        position += 1 + count;
                        runningStatus = (lead < 0xF0) ? lead : 0;
                    }
                }
                else
                {
                    if (runningStatus == 0) { packet.MalformedCommands++; return; }

                    auto const count = static_cast<size_t>(DataByteCount(runningStatus));

                    if (position + count > length) { packet.MalformedCommands++; return; }

                    for (size_t k = 0; k < count; k++)
                    {
                        if (list[position + k] >= 0x80) { packet.MalformedCommands++; return; }
                    }

                    // the status is restored so every emitted fragment stands on its own
                    std::array<uint8_t, 3> message{ runningStatus, 0, 0 };
                    std::memcpy(&message[1], &list[position], count);
                    Emit(packet, timestamp, message.data(), 1 + count);

                    position += count;
                }

                if (position >= length) break;

                // a delta time precedes every command after the first; the final one may stand alone
                uint32_t delta = 0;
                if (!ReadDeltaTime(list, length, position, delta)) { packet.MalformedCommands++; return; }
                timestamp += delta;
            }
        }

        bool m_sysExOpen{ false };
    };


    // ------------------------------------------------------------------------------------------
    // Outgoing MIDI list
    //
    // Takes a MIDI 1.0 byte stream in arbitrary pieces, as it arrives from a UMP to byte stream
    // translator, and produces MIDI lists. A SysEx still open when lists are taken is sent as a
    // segment, so a large transfer streams instead of being buffered whole.
    // ------------------------------------------------------------------------------------------

    class CommandSectionEncoder
    {
    public:
        // Room for the MIDI list alone. Kept well under an Ethernet MTU with IPv6 headers.
        explicit CommandSectionEncoder(size_t maxListBytes = 1000) : m_maxListBytes(maxListBytes) {}

        void Reset()
        {
            m_commands.clear();
            m_pending.clear();
            m_pendingNeeded = 0;
            m_status = 0;
            m_inSysEx = false;
            m_sysExSegmentSent = false;
            m_sysExBuffer.clear();
        }

        bool SysExOpen() const { return m_inSysEx; }

        void Append(uint8_t const* bytes, size_t count)
        {
            for (size_t i = 0; i < count; i++) AppendByte(bytes[i]);
        }

        std::vector<std::vector<uint8_t>> TakeLists()
        {
            // an open SysEx needs at least one data byte to make a legal first or middle segment
            if (m_inSysEx && !m_sysExBuffer.empty()) CloseSegment(false);

            std::vector<std::vector<uint8_t>> lists;
            std::vector<uint8_t> list;

            for (auto const& command : m_commands)
            {
                size_t const needed = list.empty() ? command.size() : command.size() + 1;

                if (!list.empty() && list.size() + needed > m_maxListBytes)
                {
                    lists.push_back(std::move(list));
                    list.clear();
                }

                // a zero delta time separates commands; the first has none because Z is clear
                if (!list.empty()) list.push_back(0x00);
                list.insert(list.end(), command.begin(), command.end());
            }

            if (!list.empty()) lists.push_back(std::move(list));

            m_commands.clear();

            return lists;
        }

    private:
        void Queue(std::vector<uint8_t>&& command) { m_commands.push_back(std::move(command)); }

        // Every segment but the last must carry at least one data byte.
        void CloseSegment(bool isLast)
        {
            std::vector<uint8_t> segment;
            segment.reserve(m_sysExBuffer.size() + 2);

            segment.push_back(m_sysExSegmentSent ? 0xF7 : 0xF0);
            segment.insert(segment.end(), m_sysExBuffer.begin(), m_sysExBuffer.end());
            segment.push_back(isLast ? 0xF7 : 0xF0);

            Queue(std::move(segment));

            m_sysExBuffer.clear();
            m_sysExSegmentSent = true;

            if (isLast)
            {
                m_inSysEx = false;
                m_sysExSegmentSent = false;
            }
        }

        void AppendByte(uint8_t byte)
        {
            if (IsRealTime(byte))
            {
                // real time may sit between segments; closing one here keeps its position
                if (m_inSysEx && !m_sysExBuffer.empty()) CloseSegment(false);
                Queue(std::vector<uint8_t>{ byte });
                return;
            }

            if (m_inSysEx)
            {
                if (byte < 0x80)
                {
                    m_sysExBuffer.push_back(byte);
                    if (m_sysExBuffer.size() + 2 >= m_maxListBytes) CloseSegment(false);
                    return;
                }

                // F7 ends it; any other status also ends it, as it would on a DIN cable
                CloseSegment(true);

                if (byte == 0xF7) return;
            }

            if (byte == 0xF0)
            {
                m_pending.clear();
                m_pendingNeeded = 0;
                m_status = 0;
                m_inSysEx = true;
                m_sysExSegmentSent = false;
                m_sysExBuffer.clear();
                return;
            }

            if (byte >= 0x80)
            {
                auto const dataBytes = DataByteCount(byte);

                m_pending.clear();
                m_pendingNeeded = 0;

                if (byte == 0xF7 || dataBytes < 0)
                {
                    // a stray F7 or an undefined system common has no place in a MIDI list
                    m_status = 0;
                    return;
                }

                m_pending.push_back(byte);
                m_pendingNeeded = static_cast<size_t>(dataBytes);
                m_status = (byte < 0xF0) ? byte : 0;

                if (m_pendingNeeded == 0)
                {
                    Queue(std::move(m_pending));
                    m_pending.clear();
                }

                return;
            }

            if (m_pending.empty())
            {
                if (m_status == 0) return;

                // the source used running status; every outgoing list starts with a status
                m_pending.push_back(m_status);
                m_pendingNeeded = static_cast<size_t>(DataByteCount(m_status));
            }

            m_pending.push_back(byte);

            if (--m_pendingNeeded == 0)
            {
                Queue(std::move(m_pending));
                m_pending.clear();
            }
        }

        size_t m_maxListBytes{ 1000 };
        std::vector<std::vector<uint8_t>> m_commands;

        std::vector<uint8_t> m_pending;
        size_t m_pendingNeeded{ 0 };
        uint8_t m_status{ 0 };

        bool m_inSysEx{ false };
        bool m_sysExSegmentSent{ false };
        std::vector<uint8_t> m_sysExBuffer;
    };

    // RTP header, command section header, MIDI list, and an optional journal section.
    inline std::vector<uint8_t> BuildRtpMidiPacket(
        uint16_t sequence,
        uint32_t timestamp,
        uint32_t ssrc,
        std::vector<uint8_t> const& list,
        std::vector<uint8_t> const* journal = nullptr)
    {
        std::vector<uint8_t> out;
        out.reserve(RtpFixedHeaderSize + 2 + list.size() + (journal ? journal->size() : 0));

        WriteRtpHeader(out, sequence, timestamp, ssrc);

        uint8_t const journalFlag = (journal != nullptr) ? CommandSectionFlagJ : 0;

        if (list.size() <= MaxShortListLength)
        {
            PutU8(out, static_cast<uint8_t>(journalFlag | list.size()));
        }
        else
        {
            PutU8(out, static_cast<uint8_t>(CommandSectionFlagB | journalFlag | ((list.size() >> 8) & 0x0F)));
            PutU8(out, static_cast<uint8_t>(list.size()));
        }

        out.insert(out.end(), list.begin(), list.end());

        if (journal != nullptr) out.insert(out.end(), journal->begin(), journal->end());

        return out;
    }


    // ------------------------------------------------------------------------------------------
    // Session clock
    //
    // AppleMIDI timestamps count 100 microsecond ticks from an arbitrary origin, 64 bits in the
    // CK exchange and the low 32 bits in each RTP header.
    // ------------------------------------------------------------------------------------------

    constexpr uint64_t SessionClockTicksPerSecond = 10000;

    // Remote clock minus local clock, from one CK exchange, as seen by the side that sent CK0.
    // CK0 left at t0 (local), the peer stamped t1 (remote), CK1 came back at t2 (local).
    inline int64_t ClockOffsetFromInitiatorSide(uint64_t t0, uint64_t t1, uint64_t t2)
    {
        auto const midpoint = static_cast<int64_t>(t0) + (static_cast<int64_t>(t2) - static_cast<int64_t>(t0)) / 2;
        return static_cast<int64_t>(t1) - midpoint;
    }

    // The same exchange from the side that answered: t0 and t2 are the remote's, t1 is ours.
    inline int64_t ClockOffsetFromResponderSide(uint64_t t0, uint64_t t1, uint64_t t2)
    {
        auto const midpoint = static_cast<int64_t>(t0) + (static_cast<int64_t>(t2) - static_cast<int64_t>(t0)) / 2;
        return midpoint - static_cast<int64_t>(t1);
    }

    // Rebuilds a 64-bit tick count from the low 32 bits in an RTP header, choosing the value
    // nearest an estimate of the sender's clock so a wrap every ~5 days is handled.
    inline uint64_t UnwrapTimestamp(uint32_t low32, uint64_t senderClockEstimate)
    {
        uint64_t candidate = (senderClockEstimate & 0xFFFFFFFF00000000ull) | low32;

        if (candidate > senderClockEstimate && candidate - senderClockEstimate > 0x80000000ull)
        {
            if (candidate >= 0x100000000ull) candidate -= 0x100000000ull;
        }
        else if (senderClockEstimate > candidate && senderClockEstimate - candidate > 0x80000000ull)
        {
            candidate += 0x100000000ull;
        }

        return candidate;
    }
}
