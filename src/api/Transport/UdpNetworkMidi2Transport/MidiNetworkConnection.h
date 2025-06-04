// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://github.com/microsoft/MIDI/
// ============================================================================

#pragma once



struct MidiRetransmitBufferEntry
{
    MidiSequenceNumber SequenceNumber{ 0 };
    std::vector<uint32_t> Words{ };
};

struct MidiOutgoingPingTrackingEntry
{
    uint32_t PingId{ 0 };
    uint64_t PingSendTimestamp{ 0 };
    uint64_t PingReceiveTimestamp{ 0 };

    bool Received{ false };
};

class MidiNetworkConnection
{
public:
    HRESULT Initialize(
        _In_ MidiNetworkConnectionRole const role,
        _In_ winrt::hstring configIdentifier,
        _In_ winrt::Windows::Networking::Sockets::DatagramSocket const& socket,
        _In_ winrt::Windows::Networking::HostName const& remoteHostName,
        _In_ winrt::hstring const& remotePort,
        _In_ std::wstring const& thisEndpointName,
        _In_ std::wstring const& thisProductInstanceId,
        _In_ uint16_t const retransmitBufferMaxCommandPacketCount,
        _In_ uint8_t const maxForwardErrorCorrectionCommandPacketCount,
        _In_ bool createUmpEndpointsOnly
    );

    HRESULT Shutdown();

    HRESULT ProcessIncomingMessage(
        _In_ winrt::Windows::Storage::Streams::DataReader const& reader);

    HRESULT SendInvitation();

    HRESULT ConnectMidiCallback(
        _In_ wil::com_ptr_nothrow<IMidiCallback> callback
    );

    HRESULT QueueMidiMessagesToSendToNetwork(
        _In_ std::vector<uint32_t> const& words);

    HRESULT QueueMidiMessagesToSendToNetwork(
        _In_ PVOID const bytes,
        _In_ UINT const byteCount);



    HRESULT DisconnectMidiCallback();

    // todo: session info, connection to bidi streams, etc.

private:
    HRESULT SendQueuedMidiMessagesToNetwork();

    HRESULT StartOutboundMidiMessageProcessingThread();
    HRESULT StartConnectionWatchdogThread();

    HRESULT ResetSequenceNumbers();
    HRESULT EndActiveSession(_In_ bool respondWithByeReply);

    HRESULT RequestMissingPackets();

    winrt::hstring m_configIdentifier{};
        
    wil::slim_event_manual_reset m_newMessagesInQueueEvent;
    wil::critical_section m_outgoingUmpMessageQueueLock;
    std::vector<uint32_t> m_outgoingUmpMessages{};
    std::jthread m_outboundProcessingThread;
    HRESULT OutboundProcessingThreadWorker(_In_ std::stop_token stopToken);

    bool m_createUmpEndpointsOnly{ true };


    uint32_t m_emptyUmpIterationCounter{ 0 };                   // this will increment over time
    uint32_t m_emptyUmpIterationIntervalBeforeSending{ 10 };    // number of background thread iterations with no data before we send empty UMP packets. This changes over time.

    MidiNetworkConnectionRole m_role{};

    wil::critical_section m_socketWriterLock;


    std::wstring m_sessionEndpointDeviceInterfaceId{};  // swd
    std::wstring m_sessionDeviceInstanceId{};           // what we used to create/delete the device
    bool m_sessionActive{ false };
    wil::com_ptr_nothrow<IMidiCallback> m_callback{ nullptr };

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

    HRESULT HandleIncomingInvitationReplyAccepted(
        _In_ MidiNetworkCommandPacketHeader const& header,
        _In_ std::wstring const& remoteHostUmpEndpointName,
        _In_ std::wstring const& remoteHostProductInstanceId);

    HRESULT HandleIncomingBye();

    HRESULT HandleIncomingPing(_In_ uint32_t const pingId);
    HRESULT HandleIncomingPingReply(_In_ uint32_t const pingId);
    HRESULT SendPing();

    HRESULT HandleIncomingUmpData(
        _In_ uint64_t const timestamp,
        _In_ std::vector<uint32_t> const& words
    );

    HRESULT HandleIncomingRetransmitRequest(
        _In_ uint16_t const startingSequenceNumber, 
        _In_ uint16_t const retransmitPacketCount);

    // FEC, Retransmit, and UMP integrity -----------------------------------------------

    MidiSequenceNumber m_lastSentUmpCommandSequenceNumber{ 0 };
    MidiSequenceNumber m_lastReceivedUmpCommandSequenceNumber{ 0 };

    uint8_t m_maxForwardErrorCorrectionCommandPacketCount{ 2 };
    uint16_t m_retransmitBufferMaxCommandPacketCount{ 0 };
    boost::circular_buffer<MidiRetransmitBufferEntry> m_retransmitBuffer {};


    // Connection stability -------------------------------------------------------------

    // we may eventually want these to be configurable. For now, they are const
    const uint16_t m_outgoingUmpEmptyPacketMaxIntervalMilliseconds{ 2000 };
    const uint16_t m_outgoingUmpEmptyPacketStartingIntervalMilliseconds{ 200 };
    uint16_t m_outgoingUmpEmptyPacketIntervalMilliseconds{ m_outgoingUmpEmptyPacketStartingIntervalMilliseconds };
    const uint16_t m_outgoingPingIntervalMilliseconds{ 2000 };
    const uint16_t m_outgoingPingMaxIgnoredBeforeDisconnect{ 5 };
    const uint16_t m_outgoingPingTrackingMaxEntries{ 10 };
    boost::circular_buffer<MidiOutgoingPingTrackingEntry> m_outgoingPingTracking{};

    //const uint16_t m_maxMillisecondsWithoutResponseBeforeDisconnect{ 15000 };       // Milliseconds of silence (no pings or any other message) before disconnect
    wil::slim_event_manual_reset m_connectionTimeoutEvent;
    std::atomic<uint64_t> m_lastIncomingValidUdpPacketTimestamp{ 0 };

    std::jthread m_connectionWatcherThread;
    HRESULT SignalHealthyConnectionAndUpdateArrivalTimestamp();
    HRESULT ConnectionWatcherThreadWorker(_In_ std::stop_token stopToken);
    HRESULT EndActiveSessionDueToTimeout();

    HRESULT AddUmpPacketToRetransmitBuffer(_In_ MidiSequenceNumber const sequenceNumber, _In_ std::vector<uint32_t> const& words);




};