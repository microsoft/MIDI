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
class MidiNetworkConnection : public std::enable_shared_from_this<MidiNetworkConnection>
{
public:

    HRESULT InitializeForHost(
        _In_ winrt::guid const& configIdentifier,
        _In_ std::wstring const& hostParentInstanceId,
        _In_ winrt::Windows::Networking::Sockets::DatagramSocket const& socket,
        _In_ winrt::Windows::Networking::HostName const& remoteClientHostName,
        _In_ winrt::hstring const& remotePort,
        _In_ std::wstring const& thisEndpointName,
        _In_ std::wstring const& thisProductInstanceId,
        _In_ uint16_t const retransmitBufferMaxCommandPacketCount,
        _In_ uint8_t const maxForwardErrorCorrectionCommandPacketCount,
        _In_ bool createUmpEndpointsOnly,
        _In_ MidiNetworkAuthenticationKind const authenticationKind,
        _In_ MidiNetworkCredentialIdentifier const& credentialIdentifier
    );

    HRESULT InitializeForClient(
        _In_ winrt::guid const& configIdentifier,
        _In_ winrt::Windows::Networking::Sockets::DatagramSocket const& socket,
        _In_ winrt::Windows::Networking::HostName const& remoteHostHostName,
        _In_ winrt::hstring const& remotePort,
        _In_ std::wstring const& thisEndpointName,
        _In_ std::wstring const& thisProductInstanceId,
        _In_ uint16_t const retransmitBufferMaxCommandPacketCount,
        _In_ uint8_t const maxForwardErrorCorrectionCommandPacketCount,
        _In_ bool createUmpEndpointsOnly
    );


    HRESULT Shutdown();

    // Phase one of teardown. Fire-and-forget, idempotent, and safe to call on every connection
    // before shutting any of them down, so no remote waits behind another's teardown.
    // Shutdown() calls this itself if it has not already run.
    HRESULT SendShutdownBye();

    // Called by the endpoint creation worker once the MIDI endpoint exists, to finish accepting
    // an invitation that was answered with Invitation Reply: Pending.
    HRESULT CompleteHostSessionAfterEndpointCreated(
        _In_ std::wstring const& newDeviceInstanceId,
        _In_ std::wstring const& newEndpointDeviceInterfaceId);

    // Performs the creation itself, on the worker rather than the receive callback.
    HRESULT CreateHostEndpointForPendingInvitation(
        _In_ std::wstring const& clientUmpEndpointName,
        _In_ std::wstring const& clientProductInstanceId,
        _Out_ std::wstring& newDeviceInstanceId,
        _Out_ std::wstring& newEndpointDeviceInterfaceId);

    // Called by the same worker when the endpoint could not be created.
    HRESULT FailHostSessionEndpointCreation(_In_ HRESULT const failure);

    // Spec 6.16 "should": repeat the Bye until a Bye Reply arrives or we run out of attempts.
    // Only for a disconnect the user asked for. Shutdown paths must not block, so they use the
    // fire-and-forget Bye inside Shutdown() instead. Returns S_FALSE if no reply arrived.
    HRESULT SendUserTerminatedByeAndAwaitReply();

    // The caller has already consumed the UDP header and the first command header word, so that
    // it can decide whether this remote is allowed a connection at all before one is created.
    HRESULT ProcessIncomingMessage(
        _In_ winrt::Windows::Storage::Streams::DataReader const& reader,
        _In_ uint32_t const firstCommandHeaderWord);

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

    // if this was created from a host here
    winrt::guid ConfigIdentifier() { return m_configIdentifier; }


    bool IsSessionActive() { return m_sessionActive; }
    std::wstring GetEndpointDeviceId() { return m_sessionEndpointDeviceInterfaceId; }

    // Host role. The remote's own identity, as supplied in its invitation.
    MidiNetworkRemoteClientIdentity GetRemoteClientIdentity()
    {
        auto lock = m_remoteIdentityLock.lock();

        return MidiNetworkRemoteClientIdentity{ m_remoteEndpointName, m_remoteProductInstanceId };
    }

    // Host role. The remote has been answered with an Invitation Reply Pending and is waiting
    // on a user decision.
    bool IsAwaitingUserApproval() { return m_awaitingUserApproval; }

    // Host role. FILETIME, UTC, of the first invitation which put this remote into the pending
    // state. Zero when it has never been pending. The client re-invites on a timer while it
    // waits, so this deliberately records the first ask and not the most recent one.
    uint64_t GetUserApprovalRequestedFileTime() { return m_userApprovalRequestedFileTime.load(); }

    // Host role. The remote said Bye before its endpoint had been created, so the queued
    // creation is no longer wanted. Building it anyway costs a device node created and then
    // immediately torn down, and that teardown contends with the creations still queued.
    bool IsHostEndpointCreationAbandoned() { return m_hostEndpointCreationAbandoned; }

    // Drops a queued creation. The remote has already gone, so nothing is sent to it.
    void CancelPendingHostEndpointCreation()
    {
        m_hostEndpointCreationPending = false;
    }

    winrt::Windows::Networking::HostName GetRemoteHostName() { return m_remoteHostName; }
    std::wstring GetRemotePort() { return m_remotePort; }

    // A user allowed this pending remote. Resumes the invitation where the approval gate left it.
    HRESULT ApproveByUser();

    // A user refused this pending remote. Sends the Bye the spec has for exactly this.
    HRESULT DenyByUser();

    // True once a session existed and has now ended. The owner releases the connection at that
    // point instead of leaving it to the idle reaper: a remote normally reconnects from a new
    // ephemeral port, so the old entry would otherwise hold a slot and two threads for nothing.
    bool IsSessionFinished()
    {
        return m_sessionEverEstablished && !m_sessionActive && !m_invitationPending;
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

    uint64_t GetAndResetAverageLatencyTicks() 
    { 
        auto lock = m_latencyLock.lock();

        uint64_t totalLatency = m_latencyTotalTicks;
        uint32_t countEntries = m_latencyCountEntries;

        m_latencyTotalTicks = 0;
        m_latencyCountEntries = 0;

        auto latency = countEntries > 0 ? totalLatency / countEntries : 0;
        
        return latency; 
    }

    void AddLatencyToAverageLatencyTicks(_In_ uint64_t latencyTicks)
    {
        auto lock = m_latencyLock.lock();

        m_latencyTotalTicks += latencyTicks;
        m_latencyCountEntries++;

        // reset so we don't have overflow issues when the values aren't read
        if (m_latencyCountEntries > 5000)
        {
            m_latencyTotalTicks = 0;
            m_latencyCountEntries = 0;
        }
    }

private:
    HRESULT Initialize(
        _In_ MidiNetworkConnectionRole const role,
        _In_ winrt::guid const& configIdentifier,
        _In_ std::wstring const& hostParentInstanceId,  // host only
        _In_ winrt::Windows::Networking::Sockets::DatagramSocket const& socket,
        _In_ winrt::Windows::Networking::HostName const& remoteHostName,
        _In_ winrt::hstring const& remotePort,
        _In_ std::wstring const& thisEndpointName,
        _In_ std::wstring const& thisProductInstanceId,
        _In_ uint16_t const retransmitBufferMaxCommandPacketCount,
        _In_ uint8_t const maxForwardErrorCorrectionCommandPacketCount,
        _In_ bool createUmpEndpointsOnly,
        _In_ MidiNetworkAuthenticationKind const authenticationKind,
        _In_ MidiNetworkCredentialIdentifier const& credentialIdentifier
    );

    HRESULT SendQueuedMidiMessagesToNetwork();

    HRESULT StartOutboundMidiMessageProcessingThread();
    HRESULT StartConnectionWatchdogThread();
    HRESULT StopAndJoinWorkerThreads();

    HRESULT ResetSequenceNumbers();

    std::atomic<bool> m_shuttingDown{ false };
    std::atomic<bool> m_shutdownByeSent{ false };

    // Host role. Set while an endpoint is being created for an invitation we answered with
    // Pending, so a repeated invitation does not queue the work a second time.
    std::atomic<bool> m_hostEndpointCreationPending{ false };

    // Set when a Bye arrives for a connection whose endpoint has not been created yet.
    std::atomic<bool> m_hostEndpointCreationAbandoned{ false };

    wil::critical_section m_latencyLock;
    uint64_t m_latencyTotalTicks{ 0 };
    uint32_t m_latencyCountEntries{ 0 };
    std::atomic<uint64_t> m_totalNetworkPacketsReceived{ 0 };

    HRESULT EndActiveSession(_In_ bool respondWithByeReply);

    HRESULT RequestMissingPackets();

    // Spec: a Device receiving UMP Data, a Retransmit Request, a Retransmit Error, a Session
    // Reset or a Session Reset Reply outside an Established Session shall answer with this.
    HRESULT SendByeSessionNotEstablished(_In_ uint8_t const commandCode);

    // Invitation retry state, client role only. Touched from the watchdog thread and from
    // message parsing.
    std::atomic<bool> m_invitationPending{ false };
    std::atomic<uint16_t> m_invitationAttempts{ 0 };

    // Set once the host answers with Invitation Reply: Pending. Re-inviting after that would
    // be pestering a host which has already told us it is waiting on a person.
    std::atomic<bool> m_invitationReplyPendingReceived{ false };
    std::atomic<uint64_t> m_invitationReplyPendingTimestamp{ 0 };

    HRESULT SendInvitationCommand();
    HRESULT ServicePendingInvitation();

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
    HRESULT OutboundProcessingThreadWorker(_In_ std::stop_token stopToken);

    bool m_createUmpEndpointsOnly{ true };

    MidiNetworkConnectionRole m_role{};

    wil::critical_section m_socketWriterLock;

    std::wstring m_parentDeviceInstanceId;              // the parent under which new endpoints are created.

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

    // Identity the remote supplied in its invitation. Host role only. This is what the user
    // approves and what the allow and deny lists match on. Written on the socket receive thread
    // and read by the configuration manager, so it is guarded.
    wil::critical_section m_remoteIdentityLock;
    std::wstring m_remoteEndpointName{ };
    std::wstring m_remoteProductInstanceId{ };

    // Host role: the remote has been told its invitation is pending and is waiting for a user
    // to approve or deny it. No endpoint exists yet.
    std::atomic<bool> m_awaitingUserApproval{ false };

    // When that wait started, so a user deciding later can see how long something has been
    // asking. Set on the transition into the pending state only.
    std::atomic<uint64_t> m_userApprovalRequestedFileTime{ 0 };

    std::shared_ptr<MidiNetworkDataWriter> m_writer{ nullptr };


    HRESULT ReadUtf8String(
        _In_ winrt::Windows::Storage::Streams::DataReader const& reader,
        _In_ size_t const byteCount,
        _Out_ std::wstring& value);

    HRESULT HandleIncomingInvitation(
        _In_ MidiNetworkCommandPacketHeader const& header,
        _In_ MidiNetworkCommandInvitationCapabilities const& capabilities,
        _In_ std::wstring const& clientUmpEndpointName,
        _In_ std::wstring const& clientProductInstanceId);

    HRESULT HandleIncomingInvitationReplyAccepted(
        _In_ MidiNetworkCommandPacketHeader const& header,
        _In_ std::wstring const& remoteHostUmpEndpointName,
        _In_ std::wstring const& remoteHostProductInstanceId);

    // Authentication negotiation, spec 6.5, 6.6, 6.9 and 6.10. All of these currently refuse.
    // https://github.com/microsoft/MIDI/issues/733

    // Host side: a client answered our authentication challenge.
    HRESULT HandleIncomingInvitationWithAuthentication(
        _In_ MidiNetworkCommandPacketHeader const& header,
        _In_ MidiNetworkAuthenticationKind const kind);

    // Client side: the host is challenging us, or asking us to keep waiting.
    HRESULT HandleIncomingInvitationReplyAuthenticationRequired(
        _In_ MidiNetworkCommandPacketHeader const& header,
        _In_ MidiNetworkAuthenticationKind const kind);

    HRESULT HandleIncomingInvitationReplyPending();

    // Refuses an invitation which we cannot authenticate, per spec 6.4.
    HRESULT RefuseInvitationForAuthentication(_In_ MidiNetworkCommandByeReason const reason);

    // Declines the session when the endpoint could not be created, choosing a Bye reason which
    // reflects why. Role-appropriate: the host and client reason codes differ.
    HRESULT RefuseSessionForEndpointCreationFailure(_In_ HRESULT const creationResult);

    // Spec Appendix B. Deliberately not written from guesswork: getting this wrong produces a
    // scheme which looks like it works against our own implementation and nothing else.
    HRESULT ComputeAuthenticationDigest(
        _In_reads_bytes_(nonceByteCount) uint8_t const* nonce,
        _In_ size_t const nonceByteCount,
        _In_ MidiNetworkSecret const& secret,
        _Out_writes_bytes_(digestByteCount) uint8_t* digest,
        _In_ size_t const digestByteCount);

    MidiNetworkAuthenticationKind m_authenticationKind{ MidiNetworkAuthenticationKind::None };
    MidiNetworkCredentialIdentifier m_credentialIdentifier{ };

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
    const uint16_t m_outgoingPingIntervalMilliseconds{ 2000 };
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

    // These must remain the last members declared. Members are destroyed in reverse declaration
    // order, so declaring them last guarantees both threads are joined before anything they
    // reference (writer, locks, buffers) is torn down.
    std::jthread m_outboundProcessingThread;
    std::jthread m_connectionWatcherThread;

};