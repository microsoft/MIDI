// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://github.com/microsoft/MIDI/
// ============================================================================

#pragma once

// TODO: This class needs a timer that is going to check when we last received
// a message, and if more than X milliseconds, will send out a ping
// The same timer should also send out empty UMP command packets if nothing has
// been sent from this endpoint for X milliseconds

struct MidiPingInformation
{
    uint32_t PingId{ 0 };
    uint64_t Timestamp{ 0 };
};


class MidiNetworkConnection
{
public:
    HRESULT Initialize(
        _In_ MidiNetworkConnectionRole const role,
        _In_ winrt::Windows::Networking::Sockets::DatagramSocket const& socket,
        _In_ winrt::Windows::Networking::HostName const& remoteHostName,
        _In_ winrt::hstring const& remotePort,
        _In_ std::wstring const& thisEndpointName,
        _In_ std::wstring const& thisProductInstanceId,
        _In_ uint16_t const retransmitBufferMaxCommandPacketCount
    );

    HRESULT ProcessIncomingMessage(
        _In_ winrt::Windows::Storage::Streams::DataReader const& reader);

    HRESULT SendMidiMessagesToNetwork(
        _In_ std::vector<uint32_t> const& words);


    // todo: session info, connection to bidi streams, etc.

private:
    MidiNetworkConnectionRole m_role{};

    bool m_sessionActive{ false };

    winrt::Windows::Networking::HostName m_remoteHostName{ nullptr };
    std::wstring m_remotePort{ };

    std::wstring m_thisEndpointName{ };
    std::wstring m_thisProductInstanceId{ };

    std::shared_ptr<MidiNetworkDataWriter> m_writer{ nullptr };

    std::wstring ReadUtf8String(
        _In_ winrt::Windows::Storage::Streams::DataReader reader,
        _In_ size_t const byteCount);

    HRESULT HandleIncomingInvitation(
        _In_ MidiNetworkCommandPacketHeader const& header,
        _In_ MidiNetworkCommandInvitationCapabilities const& capabilities,
        _In_ std::wstring const& clientUmpEndpointName,
        _In_ std::wstring const& clientProductInstanceId);

    HRESULT HandleIncomingBye();

    uint32_t m_lastPingIdSent{ 0 };

    HRESULT HandleIncomingPing(_In_ uint32_t const pingId);
    HRESULT HandleIncomingPingReply(_In_ uint32_t const pingId);

    uint16_t m_lastSentUmpCommandSequenceNumber{ 0 };
    uint16_t m_lastReceivedUmpCommandSequenceNUmber{ 0 };
    
#pragma push_macro("max")
#undef max
    inline uint16_t IncrementAndGetNextUmpSequenceNumber()
    {
        return m_lastSentUmpCommandSequenceNumber == std::numeric_limits<uint16_t>::max() ? 0 : ++m_lastSentUmpCommandSequenceNumber;
    }
#pragma pop_macro("max")
    
        // TODO: Last n UMP packets kept in a circular buffer here for FEC
    uint16_t m_retransmitBufferMaxCommandPacketCount{ 0 };

    HRESULT StoreUmpPacketInRetransmitBuffer(_In_ uint16_t const sequenceNumber, _In_ std::vector<uint32_t> const& words);
    HRESULT RetransmitBufferedPackets(_In_ uint16_t const startingSequenceNumber, _In_ uint16_t const retransmitPacketCount);
};