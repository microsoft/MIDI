// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://github.com/microsoft/MIDI/
// ============================================================================

#pragma once

// Independent implementation of the Network MIDI 2.0 UDP wire format, written from
// M2-124-UM v1.0 and deliberately sharing NO code with the transport under test. If these tests
// used the transport's own MidiNetworkDataWriter, a defect in that writer would be invisible
// because both sides of the comparison would contain it. Constants are re-declared here for the
// same reason: this is double-entry bookkeeping against the specification.

#include <cstdint>
#include <string>
#include <vector>

namespace NetworkMidiTest
{
    // Spec section 5.3: all UDP packets start with this, once per packet
    constexpr uint32_t UdpPacketSignature{ 0x4D494449 };     // "MIDI"

    // Spec section 5.2
    constexpr size_t MaxRecommendedUdpPayloadBytes{ 1400 };

    // Spec section 7.1
    constexpr size_t MaxUmpWordsPerCommand{ 64 };

    // Spec sections 6.4 - 6.8
    constexpr size_t MaxEndpointNameBytes{ 98 };
    constexpr size_t MaxProductInstanceIdBytes{ 42 };

    enum class CommandCode : uint8_t
    {
        Invitation = 0x01,
        InvitationWithAuthentication = 0x02,
        InvitationWithUserAuthentication = 0x03,

        InvitationReplyAccepted = 0x10,
        InvitationReplyPending = 0x11,
        InvitationReplyAuthenticationRequired = 0x12,
        InvitationReplyUserAuthenticationRequired = 0x13,

        Ping = 0x20,
        PingReply = 0x21,

        RetransmitRequest = 0x80,
        RetransmitError = 0x81,
        SessionReset = 0x82,
        SessionResetReply = 0x83,
        Nak = 0x8F,

        Bye = 0xF0,
        ByeReply = 0xF1,

        UmpData = 0xFF,
    };

    // Spec section 6.16
    enum class ByeReason : uint8_t
    {
        Undefined = 0x00,
        UserTerminated = 0x01,
        PowerDown = 0x02,
        TooManyMissingUmps = 0x03,
        Timeout = 0x04,
        SessionNotEstablished = 0x05,
        NoPendingInvitation = 0x06,
        ProtocolError = 0x07,

        TooManyOpenSessions = 0x40,
        AuthWithoutPriorAttempt = 0x41,
        UserDidNotAccept = 0x42,
        AuthFailed = 0x43,
        UsernameNotFound = 0x44,
        NoMatchingAuthenticationMethod = 0x45,

        InvitationCanceled = 0x80,
    };

    // Spec section 6.15
    enum class NakReason : uint8_t
    {
        Other = 0x00,
        CommandNotSupported = 0x01,
        CommandNotExpected = 0x02,
        CommandMalformed = 0x03,
        BadPingReply = 0x20,
    };

    // Spec section 7.2.4
    enum class RetransmitErrorReason : uint8_t
    {
        Unknown = 0x00,
        DataNotAvailable = 0x01,
    };

    // Spec section 6.4, Table 11. Bitmap.
    enum class InvitationCapabilities : uint8_t
    {
        None = 0x00,
        SupportsAuthentication = 0x01,
        SupportsUserAuthentication = 0x02,
    };


    // Builds a datagram byte by byte. Everything is big-endian on the wire. Nothing here
    // validates, so a test can deliberately emit a malformed packet.
    class PacketBuilder
    {
    public:
        PacketBuilder& StartPacket();

        // Emits a command header. payloadLengthWords is written verbatim, so a test can lie
        // about it on purpose.
        PacketBuilder& AddCommandHeader(
            _In_ CommandCode const code,
            _In_ uint8_t const payloadLengthWords,
            _In_ uint8_t const commandSpecificData1,
            _In_ uint8_t const commandSpecificData2);

        PacketBuilder& AddCommandHeader(
            _In_ CommandCode const code,
            _In_ uint8_t const payloadLengthWords,
            _In_ uint16_t const commandSpecificData);

        PacketBuilder& AddByte(_In_ uint8_t const value);
        PacketBuilder& AddUInt16(_In_ uint16_t const value);
        PacketBuilder& AddUInt32(_In_ uint32_t const value);
        PacketBuilder& AddRawBytes(_In_ std::vector<uint8_t> const& bytes);

        // Writes the string and zero-pads it out to a 32-bit word boundary
        PacketBuilder& AddPaddedString(_In_ std::string const& value);

        // Well-formed command helpers, for the many tests that only need valid traffic
        PacketBuilder& AddInvitation(
            _In_ std::string const& endpointName,
            _In_ std::string const& productInstanceId,
            _In_ InvitationCapabilities const capabilities = InvitationCapabilities::None);

        PacketBuilder& AddPing(_In_ uint32_t const pingId);
        PacketBuilder& AddPingReply(_In_ uint32_t const pingId);
        PacketBuilder& AddBye(_In_ ByeReason const reason, _In_ std::string const& text = "");
        PacketBuilder& AddByeReply();
        PacketBuilder& AddSessionReset();
        PacketBuilder& AddSessionResetReply();
        PacketBuilder& AddUmpData(_In_ uint16_t const sequenceNumber, _In_ std::vector<uint32_t> const& words);
        PacketBuilder& AddRetransmitRequest(_In_ uint16_t const sequenceNumber, _In_ uint16_t const commandCount);
        PacketBuilder& AddNak(_In_ uint32_t const originalCommandHeader, _In_ NakReason const reason, _In_ std::string const& text = "");

        std::vector<uint8_t> const& Bytes() const { return m_bytes; }
        size_t Size() const { return m_bytes.size(); }

        static uint8_t PaddedWordCount(_In_ size_t const byteCount);

    private:
        std::vector<uint8_t> m_bytes{ };
    };


    // One command parsed out of a received datagram.
    struct ReceivedCommand
    {
        CommandCode Code{ };
        uint8_t PayloadLengthWords{ 0 };
        uint8_t CommandSpecificData1{ 0 };
        uint8_t CommandSpecificData2{ 0 };
        uint16_t CommandSpecificDataUInt16{ 0 };

        uint32_t HeaderWord{ 0 };
        std::vector<uint8_t> Payload{ };

        ByeReason GetByeReason() const { return static_cast<ByeReason>(CommandSpecificData1); }
        NakReason GetNakReason() const { return static_cast<NakReason>(CommandSpecificData1); }
        uint16_t GetSequenceNumber() const { return CommandSpecificDataUInt16; }

        uint32_t GetPayloadUInt32(_In_ size_t const wordIndex) const;
        std::string GetPayloadString(_In_ size_t const byteOffset, _In_ size_t const byteCount) const;
    };


    struct ParsedPacket
    {
        bool SignatureValid{ false };
        bool Truncated{ false };
        std::vector<ReceivedCommand> Commands{ };

        bool Contains(_In_ CommandCode const code) const;
        ReceivedCommand const* Find(_In_ CommandCode const code) const;
        size_t Count(_In_ CommandCode const code) const;
    };

    ParsedPacket ParsePacket(_In_reads_bytes_(byteCount) uint8_t const* bytes, _In_ size_t const byteCount);

    std::wstring CommandCodeToString(_In_ CommandCode const code);
    std::wstring DescribePacket(_In_ ParsedPacket const& packet);
}
