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

struct MidiRetransmitBufferEntry
{
    uint16_t SequenceNumber{ 0 };
    std::vector<uint32_t> Words{ };
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

    HRESULT SendMidiMessagesToNetwork(
        _In_ PVOID const bytes,
        _In_ UINT const byteCount);


    HRESULT SetMidiCallback(
        _In_ IMidiCallback* callback
    );


    // todo: session info, connection to bidi streams, etc.

private:
    MidiNetworkConnectionRole m_role{};

    std::wstring m_sessionEndpointDeviceInterfaceId{};  // swd
    std::wstring m_sessionDeviceInstanceId{};           // what we used to create/delete the device
    bool m_sessionActive{ false };
    IMidiCallback* m_callback{ nullptr };

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

    HRESULT HandleIncomingUmpData(
        _In_ uint64_t const timestamp,
        _In_ std::vector<uint32_t> const& words
    );

    HRESULT HandleIncomingRetransmitRequest(
        _In_ uint16_t const startingSequenceNumber, 
        _In_ uint16_t const retransmitPacketCount);

#pragma push_macro("max")
#undef max
    uint16_t m_lastSentUmpCommandSequenceNumber{ std::numeric_limits<uint16_t>::max() };    // init to this so first real one is zero
    uint16_t m_lastReceivedUmpCommandSequenceNumber{ 0 };
    
    inline uint16_t IncrementAndGetNextUmpSequenceNumber()
    {
        if (m_lastSentUmpCommandSequenceNumber == std::numeric_limits<uint16_t>::max())
        {
            m_lastSentUmpCommandSequenceNumber = 0;
        }
        else
        {
            m_lastSentUmpCommandSequenceNumber += 1;
        }

        return m_lastSentUmpCommandSequenceNumber;
    }
#pragma pop_macro("max")
    
        // TODO: Last n UMP packets kept in a circular buffer here for FEC
    uint16_t m_retransmitBufferMaxCommandPacketCount{ 0 };
    boost::circular_buffer<MidiRetransmitBufferEntry> m_retransmitBuffer {};

    HRESULT StoreUmpPacketInRetransmitBuffer(_In_ uint16_t const sequenceNumber, _In_ std::vector<uint32_t> const& words);

};