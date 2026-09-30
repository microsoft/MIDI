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

// Always created with make_shared. The endpoint creation worker holds a reference to the
// connection while it works, so the connection has to be able to hand out its own shared_ptr.
//
// This holds only what both roles need: the socket writer, the session, the worker threads, the
// retransmit buffer and the UMP queues. Anything which is meaningful in one role only lives in
// MidiNetworkHostConnection or MidiNetworkClientConnection, so a connection cannot carry state
// belonging to the role it is not.
class MidiNetworkConnection : public std::enable_shared_from_this<MidiNetworkConnection>
{
public:
    virtual ~MidiNetworkConnection() = default;

    HRESULT Shutdown();

    // Phase one of teardown. Fire-and-forget, idempotent, and safe to call on every connection
    // before shutting any of them down, so no remote waits behind another's teardown.
    // Shutdown() calls this itself if it has not already run.
    HRESULT SendShutdownBye();

    // Called by the endpoint creation worker once the MIDI endpoint exists, to finish accepting
    // an invitation that was answered with Invitation Reply: Pending.

    // Spec 6.16 "should": repeat the Bye until a Bye Reply arrives or we run out of attempts.
    // Only for a disconnect the user asked for. Shutdown paths must not block, so they use the
    // fire-and-forget Bye inside Shutdown() instead. Returns S_FALSE if no reply arrived.
    HRESULT SendUserTerminatedByeAndAwaitReply();

    // The caller has already consumed the UDP header and the first command header word, so that
    // it can decide whether this remote is allowed a connection at all before one is created.
    HRESULT ProcessIncomingMessage(
        _In_ winrt::Windows::Storage::Streams::DataReader const& reader,
        _In_ uint32_t const firstCommandHeaderWord);

    // The newest endpoint instance wins. The service can open the endpoint again before it has
    // shut the previous instance down.
    HRESULT ConnectMidiCallback(
        _In_ wil::com_ptr_nothrow<IMidiCallback> callback
    );

    HRESULT QueueMidiMessagesToSendToNetwork(
        _In_ std::vector<uint32_t> const& words);

    HRESULT QueueMidiMessagesToSendToNetwork(
        _In_ PVOID const bytes,
        _In_ UINT const byteCount);

    // Does nothing unless this callback is still the connected one
    HRESULT DisconnectMidiCallbackIfCurrent(_In_ IMidiCallback* callback);

    // An app can open the endpoint before its creation returns. What it sends in the meantime is
    // held for the session, and dropped if no session comes of it.
    void BeginEndpointCreation() noexcept;
    void EndEndpointCreation() noexcept;

    // if this was created from a host here
    winrt::guid ConfigIdentifier() { return m_configIdentifier; }
    MidiNetworkConnectionRole Role() const noexcept { return m_role; }

    bool IsSessionActive() { return m_sessionActive; }
    std::wstring GetEndpointDeviceId()
    {
        auto lock = m_sessionLock.lock();

        return m_sessionEndpointDeviceInterfaceId;
    }

    winrt::Windows::Networking::HostName GetRemoteHostName() { return m_remoteHostName; }
    std::wstring GetRemotePort() { return m_remotePort; }

    // True once a session existed and has now ended. The owner releases the connection at that
    // point instead of leaving it to the idle reaper: a remote normally reconnects from a new
    // ephemeral port, so the old entry would otherwise hold a slot and two threads for nothing.
    virtual bool IsSessionFinished()
    {
        return m_sessionEverEstablished && !m_sessionActive;
    }

    // True when there is no session and nothing has arrived for long enough that the remote is
    // not coming back on this address and port.
    bool IsIdleAndReclaimable()
    {
        if (m_sessionActive || m_shuttingDown)
        {
            return false;
        }

        auto lastArrival = m_lastIncomingValidUdpPacketTimestamp.load();
        auto now = internal::GetCurrentMidiTimestamp();

        if (lastArrival == 0 || now <= lastArrival)
        {
            return false;
        }

        return internal::ConvertTimestampToWholeMilliseconds(now - lastArrival, internal::GetMidiTimestampFrequency())
            > MIDI_NETWORK_CONNECTION_IDLE_RECLAIM_MILLISECONDS;
    }

    // todo: session info, connection to bidi streams, etc.

    uint64_t GetTotalNetworkPacketsSent() { return m_writer ? m_writer->GetCountNetworkPacketsSent() : 0; }
    uint64_t GetTotalNetworkPacketsReceived() { return m_totalNetworkPacketsReceived; }

    // Command packets we have resent because the remote asked for them.
    uint32_t GetRetransmitCount() { return m_retransmitCount; }

    // Retransmit requests the remote has sent us.
    uint32_t GetRetransmitRequestCount() { return m_retransmitRequestCount; }

    // Averages the round trips measured since the last read and starts a fresh window. A window
    // with no ping reply in it reports the previous answer rather than zero: callers poll faster
    // than the ping interval, and reporting zero there reads as "no latency", not "no new data".
    uint64_t GetAndResetAverageLatencyTicks() 
    { 
        auto lock = m_latencyLock.lock();

        uint64_t totalLatency = m_latencyTotalTicks;
        uint32_t countEntries = m_latencyCountEntries;

        m_latencyTotalTicks = 0;
        m_latencyCountEntries = 0;

        if (countEntries > 0)
        {
            m_lastAverageLatencyTicks = totalLatency / countEntries;
        }

        return m_lastAverageLatencyTicks;
    }

    // Same value without consuming it. Only the client poll may reset the accumulator, so anything
    // inside the transport that wants the current latency uses this instead.
    uint64_t PeekAverageLatencyTicks()
    {
        auto lock = m_latencyLock.lock();

        if (m_latencyCountEntries > 0)
        {
            return m_latencyTotalTicks / m_latencyCountEntries;
        }

        return m_lastAverageLatencyTicks;
    }

    void AddLatencyToAverageLatencyTicks(_In_ uint64_t latencyTicks)    {
        auto lock = m_latencyLock.lock();

        m_latencyTotalTicks += latencyTicks;
        m_latencyCountEntries++;

        // reset so we don't have overflow issues when the values aren't read
        if (m_latencyCountEntries > 5000)
        {
            m_lastAverageLatencyTicks = m_latencyTotalTicks / m_latencyCountEntries;

            m_latencyTotalTicks = 0;
            m_latencyCountEntries = 0;
        }
    }

protected:
    HRESULT Initialize(
        _In_ MidiNetworkConnectionRole const role,
        _In_ winrt::guid const& configIdentifier,
        _In_ std::wstring const& parentDeviceInstanceId,
        _In_ winrt::Windows::Networking::Sockets::DatagramSocket const& socket,
        _In_ winrt::Windows::Networking::HostName const& remoteHostName,
        _In_ winrt::hstring const& remotePort,
        _In_ std::wstring const& thisEndpointName,
        _In_ std::wstring const& thisProductInstanceId,
        _In_ uint16_t const retransmitBufferMaxCommandPacketCount,
        _In_ uint8_t const maxForwardErrorCorrectionCommandPacketCount,
        _In_ bool createUmpEndpointsOnly,
        _In_ uint8_t const fallbackMidi1PortCount
    );

    // Role hooks. The defaults are what happens when a command arrives for the role this
    // connection is not, which spec 6.4 answers with a NAK. Overriding is how a role opts in,
    // so forgetting to override cannot silently do the wrong role's work.

    virtual HRESULT HandleIncomingInvitation(
        _In_ MidiNetworkCommandPacketHeader const& header,
        _In_ MidiNetworkCommandInvitationCapabilities const& capabilities,
        _In_ std::wstring const& clientUmpEndpointName,
        _In_ std::wstring const& clientProductInstanceId);

    // Both authentication commands arrive here. They are refused until authentication exists.
    virtual HRESULT HandleIncomingInvitationWithAuthentication(
        _In_ MidiNetworkCommandPacketHeader const& header);

    virtual HRESULT HandleIncomingInvitationReplyAccepted(
        _In_ MidiNetworkCommandPacketHeader const& header,
        _In_ std::wstring const& remoteHostUmpEndpointName,
        _In_ std::wstring const& remoteHostProductInstanceId);

    virtual HRESULT HandleIncomingInvitationReplyPending();

    // Both authentication-required replies arrive here
    virtual HRESULT HandleIncomingInvitationReplyAuthenticationRequired(
        _In_ MidiNetworkCommandPacketHeader const& header);

    // Which Bye reason this role uses when the remote is already attached.
    virtual MidiNetworkCommandByeReason ByeReasonForDeviceAlreadyAttached() const noexcept
    {
        return MidiNetworkCommandByeReason::CommandByeReasonCommon_Undefined;
    }

    // Called on the watchdog tick. Only the client has anything to do here.
    virtual HRESULT OnWatchdogTick() { return S_OK; }

    // The remote ended an established session on its own.
    virtual void OnSessionEndedByRemote() { }

    // A Bye arrived with no session, so anything queued for this remote is pointless.
    virtual void OnSessionEndedBeforeEndpointCreated() noexcept { }

    // The remote answered our invitation, whatever the answer was.
    virtual void OnInvitationAnswered() noexcept { }

    HRESULT SendQueuedMidiMessagesToNetwork();

    HRESULT StartOutboundMidiMessageProcessingThread();
    HRESULT StartConnectionWatchdogThread();
    HRESULT StopAndJoinWorkerThreads();

    HRESULT ResetSequenceNumbers();

    std::atomic<bool> m_shuttingDown{ false };
    std::atomic<bool> m_shutdownByeSent{ false };

    wil::critical_section m_latencyLock;
    uint64_t m_latencyTotalTicks{ 0 };
    uint32_t m_latencyCountEntries{ 0 };
    uint64_t m_lastAverageLatencyTicks{ 0 };
    std::atomic<uint64_t> m_totalNetworkPacketsReceived{ 0 };
    std::atomic<uint32_t> m_retransmitCount{ 0 };
    std::atomic<uint32_t> m_retransmitRequestCount{ 0 };

    HRESULT EndActiveSession(_In_ bool respondWithByeReply);

    HRESULT RequestMissingPackets();

    // Spec: a Device receiving UMP Data, a Retransmit Request, a Retransmit Error, a Session
    // Reset or a Session Reset Reply outside an Established Session shall answer with this.
    HRESULT SendByeSessionNotEstablished(_In_ uint8_t const commandCode);

    // All outbound datagrams funnel through here. Keeps the writer alive for the duration of
    // the write, and discards a half-composed packet if any step fails or throws, so that a
    // failed send can never bleed into the next one.
    template<typename TWriteCommands>
    HRESULT SendToNetwork(_In_ TWriteCommands&& writeCommands)
    {
        auto lock = m_socketWriterLock.lock();

        auto writer = m_writer;

        if (writer == nullptr)
        {
            // connection has already been torn down. Nothing to send on, and not an error.
            return S_FALSE;
        }

        HRESULT hr = S_OK;

        try
        {
            hr = writer->WriteUdpPacketHeader();

            if (SUCCEEDED(hr))
            {
                hr = writeCommands(*writer);
            }

            if (SUCCEEDED(hr))
            {
                hr = writer->Send();
            }
        }
        catch (...)
        {
            hr = wil::ResultFromCaughtException();
        }

        if (FAILED(hr))
        {
            LOG_IF_FAILED(writer->DiscardPendingData());

            LogSendFailure(hr);
        }

        return hr;
    }

    void LogSendFailure(_In_ HRESULT const hr);

    winrt::guid m_configIdentifier{};
        
    wil::critical_section m_incomingMessageLock;

    wil::slim_event_manual_reset m_newMessagesInQueueEvent;
    wil::critical_section m_outgoingUmpMessageQueueLock;
    std::vector<uint32_t> m_outgoingUmpMessages{};

    // written under m_outgoingUmpMessageQueueLock
    std::atomic<bool> m_endpointBeingCreated{ false };
    HRESULT OutboundProcessingThreadWorker(_In_ std::stop_token stopToken);

    bool m_createUmpEndpointsOnly{ true };
    uint8_t m_fallbackMidi1PortCount{ MIDI_NETWORK_MIDI_FALLBACK_MIDI1_PORT_COUNT_DEFAULT };

    MidiNetworkConnectionRole m_role{};

    wil::critical_section m_socketWriterLock;

    std::wstring m_parentDeviceInstanceId;              // the parent under which new endpoints are created

    // Guards the two ids below, and the decisions which start and end a session. An endpoint is
    // claimed and associated under it, and taken back and disassociated under it, so a session
    // which has ended can never claim an endpoint that nothing will remove. Taken before
    // TransportState's lock, never while holding it.
    wil::critical_section m_sessionLock;

    std::wstring m_sessionEndpointDeviceInterfaceId{};  // swd
    std::wstring m_sessionDeviceInstanceId{};           // what we used to create/delete the device
    std::atomic<bool> m_sessionActive{ false };
    std::atomic<bool> m_sessionEverEstablished{ false };

    wil::critical_section m_callbackLock;
    wil::com_ptr_nothrow<IMidiCallback> m_callback{ nullptr };

    // Callers must never touch m_callback directly. Taking a strong local reference here is what
    // keeps the Bidi alive across the callback while another thread is tearing the session down.
    wil::com_ptr_nothrow<IMidiCallback> GetCallback()
    {
        auto lock = m_callbackLock.lock();

        return m_callback;
    }

    wil::com_ptr_nothrow<IMidiCallback> DetachCallback()
    {
        auto lock = m_callbackLock.lock();

        wil::com_ptr_nothrow<IMidiCallback> callback{ std::move(m_callback) };
        m_callback = nullptr;

        return callback;
    }

    winrt::Windows::Networking::HostName m_remoteHostName{ nullptr };
    std::wstring m_remotePort{ };

    std::wstring m_thisEndpointName{ };
    std::wstring m_thisProductInstanceId{ };

    std::shared_ptr<MidiNetworkDataWriter> m_writer{ nullptr };


    HRESULT ReadUtf8String(
        _In_ winrt::Windows::Storage::Streams::DataReader const& reader,
        _In_ size_t const byteCount,
        _Out_ std::wstring& value);

    // Authentication negotiation, spec 6.5, 6.6, 6.9 and 6.10. All of these currently refuse.
    // https://github.com/microsoft/MIDI/issues/733

    // Refuses an invitation which we cannot authenticate, per spec 6.4.
    HRESULT RefuseInvitationForAuthentication(_In_ MidiNetworkCommandByeReason const reason);

    // Declines the session when the endpoint could not be created, choosing a Bye reason which
    // reflects why. The already-attached reason differs per role.
    HRESULT RefuseSessionForEndpointCreationFailure(_In_ HRESULT const creationResult);

    HRESULT HandleIncomingBye();
    HRESULT HandleIncomingByeReply();

    HRESULT HandleIncomingNAK(
        _In_ MidiNetworkCommandNAKReason const reason,
        _In_ MidiNetworkCommandPacketHeader const& originalCommandHeader,
        _In_ std::wstring const& text);

    HRESULT HandleIncomingRetransmitError(
        _In_ MidiNetworkCommandRetransmitErrorReason const reason,
        _In_ uint16_t const sequenceNumber);

    HRESULT HandleIncomingSessionReset();
    HRESULT HandleIncomingSessionResetReply();

    HRESULT HandleIncomingPing(_In_ uint32_t const pingId);
    HRESULT HandleIncomingPingReply(_In_ uint32_t const pingId);
    HRESULT SendPing();

    HRESULT HandleIncomingUmpData(
        _In_ uint64_t const timestamp,
        _In_ std::vector<uint32_t> const& words
    );

    // What one datagram has already provoked. Each of these goes out at most once per datagram,
    // however many of its commands would ask for another.
    struct DatagramReplyState
    {
        bool RetransmitRequested{ false };
        bool SessionNotEstablishedSent{ false };
    };

    // Reads one command's payload and hands it to its handler. The payload length has already
    // been checked against the datagram.
    void DispatchIncomingCommand(
        _In_ winrt::Windows::Storage::Streams::DataReader const& reader,
        _In_ MidiNetworkCommandPacketHeader const& commandHeader,
        _In_ uint32_t const payloadLengthInBytes,
        _Inout_ DatagramReplyState& replyState);

    // The UMP Endpoint Name and Product Instance Id which an invitation and an invitation reply
    // both carry. False when the payload is malformed.
    bool TryReadIdentityPayload(
        _In_ winrt::Windows::Storage::Streams::DataReader const& reader,
        _In_ MidiNetworkCommandPacketHeader const& commandHeader,
        _In_ uint32_t const payloadLengthInBytes,
        _Out_ std::wstring& umpEndpointName,
        _Out_ std::wstring& productInstanceId);

    void ReadIncomingNAK(
        _In_ winrt::Windows::Storage::Streams::DataReader const& reader,
        _In_ MidiNetworkCommandPacketHeader const& commandHeader,
        _In_ uint32_t const payloadLengthInBytes);

    void ReadIncomingUmpData(
        _In_ winrt::Windows::Storage::Streams::DataReader const& reader,
        _In_ MidiNetworkCommandPacketHeader const& commandHeader,
        _Inout_ DatagramReplyState& replyState);

    // Spec: UMP Data, a Retransmit Request or Error, a Session Reset or a Session Reset Reply
    // outside an established session is answered with Bye Session Not Established. True when
    // there is no session, so the caller goes no further.
    bool RefuseCommandOutsideSession(
        _In_ MidiNetworkCommandPacketHeader const& commandHeader,
        _Inout_ DatagramReplyState& replyState);

    // Spec 6.15, for a command code we don't know
    void ReplyCommandNotSupported(_In_ MidiNetworkCommandPacketHeader const& commandHeader);

    HRESULT HandleIncomingRetransmitRequest(
        _In_ MidiNetworkCommandPacketHeader const& header,
        _In_ uint16_t const startingSequenceNumber, 
        _In_ uint16_t const retransmitPacketCount);

    // FEC, Retransmit, and UMP integrity -----------------------------------------------

    MidiSequenceNumber m_lastSentUmpCommandSequenceNumber{ 0 };
    MidiSequenceNumber m_lastReceivedUmpCommandSequenceNumber{ 0 };

    uint8_t m_maxForwardErrorCorrectionCommandPacketCount{ 2 };
    uint16_t m_retransmitBufferMaxCommandPacketCount{ 0 };

    // guarded by m_socketWriterLock
    boost::circular_buffer<MidiRetransmitBufferEntry> m_retransmitBuffer {};

    // Retransmit request state. Only touched while parsing, so m_incomingMessageLock covers it.
    // A remote that cannot or will not retransmit must never be able to stall the session.
    bool m_remoteSupportsRetransmit{ true };
    bool m_retransmitRequestOutstanding{ false };
    uint16_t m_retransmitRequestAttempts{ 0 };
    MidiSequenceNumber m_retransmitRequestSequenceNumber{ 0 };

    // Stop asking for the current gap. The next UMP Data command resynchronizes past it.
    void AbandonCurrentRetransmitRequest();
    void ResetRetransmitRequestState();


    // Connection stability -------------------------------------------------------------

    // we may eventually want these to be configurable. For now, they are const
    const uint16_t m_outgoingUmpEmptyPacketMaxIntervalMilliseconds{ 2000 };
    const uint16_t m_outgoingUmpEmptyPacketStartingIntervalMilliseconds{ 200 };
    uint16_t m_outgoingUmpEmptyPacketIntervalMilliseconds{ m_outgoingUmpEmptyPacketStartingIntervalMilliseconds };
    const uint16_t m_outgoingPingMaxIgnoredBeforeDisconnect{ 5 };
    const uint16_t m_outgoingPingTrackingMaxEntries{ 10 };

    // guarded by m_pingTrackingLock
    wil::critical_section m_pingTrackingLock;
    boost::circular_buffer<MidiOutgoingPingTrackingEntry> m_outgoingPingTracking{};

    // Caps replies this peer can provoke by repeatedly sending commands we have to refuse.
    MidiNetworkReplyRateLimiter m_replyRateLimiter;

    //const uint16_t m_maxMillisecondsWithoutResponseBeforeDisconnect{ 15000 };       // Milliseconds of silence (no pings or any other message) before disconnect
    wil::slim_event_manual_reset m_connectionTimeoutEvent;

    // Only waited on by SendUserTerminatedByeAndAwaitReply. Also set by Shutdown so a disconnect
    // in progress gives up immediately rather than holding the shutdown for its full retries.
    wil::slim_event_manual_reset m_byeReplyEvent;
    std::atomic<uint64_t> m_lastIncomingValidUdpPacketTimestamp{ 0 };

    HRESULT SignalHealthyConnectionAndUpdateArrivalTimestamp();
    HRESULT ConnectionWatcherThreadWorker(_In_ std::stop_token stopToken);
    HRESULT EndActiveSessionDueToTimeout();

    // Puts an outbound client definition back in front of the creator worker after the remote
    // host went away on its own. Deliberate teardowns do not call this.
    HRESULT RequestClientReconnect();

    HRESULT AddUmpPacketToRetransmitBuffer(_In_ MidiSequenceNumber const sequenceNumber, _In_ std::vector<uint32_t> const& words);

    HRESULT AddUmpPacketToRetransmitBuffer(
        _In_ MidiSequenceNumber const sequenceNumber,
        _In_reads_(wordCount) uint32_t const* words,
        _In_ size_t const wordCount);

    // Number of words starting at position which form whole UMP messages and fit within maxWords.
    // Zero means the next message will not fit, or the tail is truncated.
    static size_t CalculateWholeUmpMessageWordCount(
        _In_ std::vector<uint32_t> const& words,
        _In_ size_t const position,
        _In_ size_t const maxWords);

    // One UMP Data command in an outbound datagram: a run of whole messages from the queue
    struct OutboundUmpChunk
    {
        size_t Offset{ 0 };
        size_t WordCount{ 0 };
        MidiSequenceNumber SequenceNumber{ 0 };
    };

    // The newest retransmit buffer entries which fit in the budget, oldest first, for forward
    // error correction. Needs m_socketWriterLock.
    std::vector<size_t> ChooseForwardErrorCorrectionPackets(_Inout_ size_t& budgetBytes);

    // Whole messages from the outbound queue which fit in the budget, each numbered after the
    // last one sent. Moves position past what it took. Needs both queue and writer locks.
    std::vector<OutboundUmpChunk> TakeOutboundChunks(
        _Inout_ size_t& position,
        _Inout_ size_t& budgetBytes);

    // The endpoint worker can start the send thread while a host stop, on another thread, stops
    // and joins it. Declared before the threads so it outlives them.
    wil::critical_section m_workerThreadsLock;

    // These must remain the last members declared. Members are destroyed in reverse declaration
    // order, so declaring them last guarantees both threads are joined before anything they
    // reference (writer, locks, buffers) is torn down.
    std::jthread m_outboundProcessingThread;
    std::jthread m_connectionWatcherThread;

};