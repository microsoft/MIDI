// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://github.com/microsoft/MIDI/
// ============================================================================

#pragma once

#include "pch.h"


_Use_decl_annotations_
HRESULT 
MidiNetworkConnection::Initialize(
    MidiNetworkConnectionRole const role,
    winrt::guid const& configIdentifier,
    std::wstring const& parentDeviceInstanceId,
    winrt::Windows::Networking::Sockets::DatagramSocket const& socket,
    winrt::Windows::Networking::HostName const& hostName,
    winrt::hstring const& port,
    std::wstring const& thisEndpointName,
    std::wstring const& thisProductInstanceId,
    uint16_t const retransmitBufferMaxCommandPacketCount,
    uint8_t const maxForwardErrorCorrectionCommandPacketCount,
    bool createUmpEndpointsOnly
)
{
    TraceLoggingWrite(
        MidiNetworkMidiTransportTelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_INFO,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this"),
        TraceLoggingWideString(L"Enter", MIDI_TRACE_EVENT_MESSAGE_FIELD)
    );

    m_configIdentifier = configIdentifier;
    m_parentDeviceInstanceId = parentDeviceInstanceId;

    m_sessionActive = false;

    m_role = role;

    m_remoteHostName = hostName;
    m_remotePort = port;

    m_createUmpEndpointsOnly = createUmpEndpointsOnly;

    m_thisEndpointName = thisEndpointName;
    m_thisProductInstanceId = thisProductInstanceId;

    m_retransmitBufferMaxCommandPacketCount = retransmitBufferMaxCommandPacketCount;
    m_maxForwardErrorCorrectionCommandPacketCount = maxForwardErrorCorrectionCommandPacketCount;

    // gives the idle reclaim check a meaningful starting point
    m_lastIncomingValidUdpPacketTimestamp = internal::GetCurrentMidiTimestamp();

    m_outgoingUmpMessages.reserve(MIDI_NETWORK_STARTING_OUTBOUND_UMP_QUEUE_CAPACITY);

    // build out the retransmit buffer used for FEC and retransmit requests
    try
    {
        m_retransmitBuffer.set_capacity(max(m_retransmitBufferMaxCommandPacketCount, m_maxForwardErrorCorrectionCommandPacketCount));
    }
    catch (...)
    {
        RETURN_IF_FAILED(E_OUTOFMEMORY);
    }

    try
    {
        m_outgoingPingTracking.set_capacity(m_outgoingPingTrackingMaxEntries);
    }
    catch (...)
    {
        RETURN_IF_FAILED(E_OUTOFMEMORY);
    }


    RETURN_IF_FAILED(ResetSequenceNumbers());

    // create the data writer
    m_writer = std::make_shared<MidiNetworkDataWriter>();
    RETURN_IF_NULL_ALLOC(m_writer);

    try
    {
        // A client socket has already been ConnectAsync'd to the remote, so it has a single
        // output stream. Only a host socket, which serves many remotes from one bound port,
        // needs a per-remote stream.
        if (role == MidiNetworkConnectionRole::ConnectionWindowsIsClient)
        {
            RETURN_IF_FAILED(m_writer->Initialize(socket.OutputStream()));
        }
        else
        {
            RETURN_IF_FAILED(m_writer->Initialize(socket.GetOutputStreamAsync(hostName, port).get()));
        }
    }
    catch (...)
    {
        auto hr = wil::ResultFromCaughtException();

        TraceLoggingWrite(
            MidiNetworkMidiTransportTelemetryProvider::Provider(),
            MIDI_TRACE_EVENT_ERROR,
            TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
            TraceLoggingLevel(WINEVENT_LEVEL_ERROR),
            TraceLoggingPointer(this, "this"),
            TraceLoggingWideString(L"Exception obtaining the output stream for the remote endpoint", MIDI_TRACE_EVENT_MESSAGE_FIELD),
            TraceLoggingWideString(hostName.ToString().c_str(), "remote hostname"),
            TraceLoggingWideString(port.c_str(), "remote port"),
            TraceLoggingHResult(hr, MIDI_TRACE_EVENT_HRESULT_FIELD)
        );

        m_writer.reset();

        RETURN_IF_FAILED(hr);
    }

    RETURN_IF_FAILED(StartConnectionWatchdogThread());

    TraceLoggingWrite(
        MidiNetworkMidiTransportTelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_INFO,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this"),
        TraceLoggingWideString(L"Exit", MIDI_TRACE_EVENT_MESSAGE_FIELD)
    );

    return S_OK;
}


_Use_decl_annotations_
HRESULT
MidiNetworkConnection::ConnectionWatcherThreadWorker(std::stop_token stopToken)
{
    // if we haven't received any UDP messages in a certain amount of time,
    // send a ping to the remote

    while (!m_shuttingDown && !stopToken.stop_requested())
    {
        auto threadWaitStartTimestamp = internal::GetCurrentMidiTimestamp();
        m_connectionTimeoutEvent.wait(m_outgoingPingIntervalMilliseconds);

        if (m_shuttingDown || stopToken.stop_requested())
        {
            break;
        }

        // We only monitor the liveness of an established session. Pinging a remote which has
        // never established a session would let a single spoofed datagram generate traffic
        // toward a forged address.
        if (!m_sessionActive)
        {
            {
                auto lock = m_pingTrackingLock.lock();
                m_outgoingPingTracking.clear();
            }

            // an invitation we sent may still be unanswered
            LOG_IF_FAILED(OnWatchdogTick());

            continue;
        }

        if (m_lastIncomingValidUdpPacketTimestamp > threadWaitStartTimestamp)
        {
            // all good. Wait again
            m_connectionTimeoutEvent.ResetEvent();

            continue;
        }

        uint16_t consecutiveFailures{ 0 };

        {
            auto lock = m_pingTrackingLock.lock();

            // check our ping entries. We want to check the last N entries and if all of them
            // have been ignored, we will take action.
            for (auto pingEntry = m_outgoingPingTracking.rbegin();
                pingEntry != m_outgoingPingTracking.rend() && consecutiveFailures <= m_outgoingPingMaxIgnoredBeforeDisconnect; pingEntry++)
            {
                if (!pingEntry->Received)
                {
                    consecutiveFailures++;
                }
                else
                {
                    // the first time we find one that has been received, we bail
                    break;
                }
            }
        }

        if (m_shuttingDown || stopToken.stop_requested())
        {
            break;
        }

        if (consecutiveFailures >= m_outgoingPingMaxIgnoredBeforeDisconnect)
        {
            TraceLoggingWrite(
                MidiNetworkMidiTransportTelemetryProvider::Provider(),
                MIDI_TRACE_EVENT_WARNING,
                TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
                TraceLoggingLevel(WINEVENT_LEVEL_WARNING),
                TraceLoggingPointer(this, "this"),
                TraceLoggingWideString(L"Remote endpoint stopped responding to pings. Ending session.", MIDI_TRACE_EVENT_MESSAGE_FIELD),
                TraceLoggingUInt16(consecutiveFailures, "consecutive missed pings")
            );

            LOG_IF_FAILED(EndActiveSessionDueToTimeout());
        }
        else
        {
            LOG_IF_FAILED(SendPing());
        }
    }

    return S_OK;
}

HRESULT
MidiNetworkConnection::SignalHealthyConnectionAndUpdateArrivalTimestamp()
{
    m_lastIncomingValidUdpPacketTimestamp = internal::GetCurrentMidiTimestamp();

    m_connectionTimeoutEvent.ResetEvent();

    return S_OK;
}


HRESULT
MidiNetworkConnection::StartOutboundMidiMessageProcessingThread()
{
    TraceLoggingWrite(
        MidiNetworkMidiTransportTelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_INFO,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this"),
        TraceLoggingWideString(L"Enter", MIDI_TRACE_EVENT_MESSAGE_FIELD)
    );

    m_newMessagesInQueueEvent.ResetEvent();

    // The stop token must come from jthread itself, not from reading the member back out of
    // the object we are in the middle of assigning to.
    m_outboundProcessingThread = std::jthread([this](std::stop_token stopToken)
        {
            try
            {
                LOG_IF_FAILED(OutboundProcessingThreadWorker(stopToken));
            }
            CATCH_LOG();
        });

    TraceLoggingWrite(
        MidiNetworkMidiTransportTelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_INFO,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this"),
        TraceLoggingWideString(L"Exit", MIDI_TRACE_EVENT_MESSAGE_FIELD)
    );

    return S_OK;
}


HRESULT
MidiNetworkConnection::StartConnectionWatchdogThread()
{
    TraceLoggingWrite(
        MidiNetworkMidiTransportTelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_INFO,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this"),
        TraceLoggingWideString(L"Enter", MIDI_TRACE_EVENT_MESSAGE_FIELD)
    );

    m_connectionWatcherThread = std::jthread([this](std::stop_token stopToken)
        {
            try
            {
                LOG_IF_FAILED(ConnectionWatcherThreadWorker(stopToken));
            }
            CATCH_LOG();
        });

    return S_OK;
}

_Use_decl_annotations_
void
MidiNetworkConnection::LogSendFailure(HRESULT const hr)
{
    TraceLoggingWrite(
        MidiNetworkMidiTransportTelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_ERROR,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_ERROR),
        TraceLoggingPointer(this, "this"),
        TraceLoggingWideString(L"Unable to send datagram to remote endpoint", MIDI_TRACE_EVENT_MESSAGE_FIELD),
        TraceLoggingWideString(m_remoteHostName != nullptr ? m_remoteHostName.ToString().c_str() : L"", "remote hostname"),
        TraceLoggingWideString(m_remotePort.c_str(), "remote port"),
        TraceLoggingHResult(hr, MIDI_TRACE_EVENT_HRESULT_FIELD)
    );
}

HRESULT
MidiNetworkConnection::StopAndJoinWorkerThreads()
{
    m_connectionWatcherThread.request_stop();
    m_outboundProcessingThread.request_stop();

    // wake both workers so they see the stop request instead of sleeping out their intervals
    m_connectionTimeoutEvent.SetEvent();
    m_newMessagesInQueueEvent.SetEvent();

    // A worker can reach here indirectly (session teardown on the watchdog thread), and joining
    // ourselves would deadlock. The remaining teardown is safe against a still-running worker.
    if (m_connectionWatcherThread.joinable() && m_connectionWatcherThread.get_id() != std::this_thread::get_id())
    {
        m_connectionWatcherThread.join();
    }

    if (m_outboundProcessingThread.joinable() && m_outboundProcessingThread.get_id() != std::this_thread::get_id())
    {
        m_outboundProcessingThread.join();
    }

    return S_OK;
}

_Use_decl_annotations_
HRESULT 
MidiNetworkConnection::ConnectMidiCallback(
    wil::com_ptr_nothrow<IMidiCallback> callback
)
{
    RETURN_HR_IF_NULL(E_INVALIDARG, callback);

    TraceLoggingWrite(
        MidiNetworkMidiTransportTelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_INFO,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this"),
        TraceLoggingWideString(L"Enter", MIDI_TRACE_EVENT_MESSAGE_FIELD),
        TraceLoggingPointer(callback.get(), "callback")
    );

    // the previous callback wasn't disconnected. Something 
    // is not as it should be, so we'll fail.
    {
        auto lock = m_callbackLock.lock();

        if (m_callback != nullptr)
        {
            RETURN_IF_FAILED(E_UNEXPECTED);
        }

        m_callback = callback;
    }

    TraceLoggingWrite(
        MidiNetworkMidiTransportTelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_INFO,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this"),
        TraceLoggingWideString(L"Exit", MIDI_TRACE_EVENT_MESSAGE_FIELD)
    );

    return S_OK;
}

HRESULT
MidiNetworkConnection::DisconnectMidiCallback()
{
    TraceLoggingWrite(
        MidiNetworkMidiTransportTelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_INFO,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this"),
        TraceLoggingWideString(L"Enter", MIDI_TRACE_EVENT_MESSAGE_FIELD)
    );

    // Released outside the lock: this drops our reference on the Bidi, which can be the last one.
    auto callback = DetachCallback();
    callback.reset();

    TraceLoggingWrite(
        MidiNetworkMidiTransportTelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_INFO,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this"),
        TraceLoggingWideString(L"Exit", MIDI_TRACE_EVENT_MESSAGE_FIELD)
    );

    return S_OK;
}



_Use_decl_annotations_
HRESULT 
MidiNetworkConnection::HandleIncomingUmpData(
    uint64_t const timestamp,
    std::vector<uint32_t> const& words
)
{
    TraceLoggingWrite(
        MidiNetworkMidiTransportTelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_INFO,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this"),
        TraceLoggingWideString(L"Enter", MIDI_TRACE_EVENT_MESSAGE_FIELD)
    );


    // A strong local reference keeps the Bidi alive for the duration of the callback even if
    // another thread tears the session down underneath us.
    auto callback = GetCallback();

    // empty UMP packets are a keep-alive approach
    // callback can be null if there are no open connections
    // from the client, but the remote device is sending messages
    if (m_sessionActive && words.size() > 0 && callback != nullptr)
    {
        // this may have more than one message, so we need to tease it apart here
        // and send the individual messages

        size_t index{ 0 };

        while (index < words.size())
        {
            uint8_t messageWordCount = internal::GetUmpLengthInMidiWordsFromFirstWord(words[index]);

            // The message type nibble is remote-supplied and can claim a length longer than what
            // was actually sent. Reading it would hand adjacent heap memory to every client.
            if (messageWordCount == 0 || index + messageWordCount > words.size())
            {
                TraceLoggingWrite(
                    MidiNetworkMidiTransportTelemetryProvider::Provider(),
                    MIDI_TRACE_EVENT_WARNING,
                    TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
                    TraceLoggingLevel(WINEVENT_LEVEL_WARNING),
                    TraceLoggingPointer(this, "this"),
                    TraceLoggingWideString(L"Truncated or malformed UMP message from remote endpoint. Discarding remainder of command.", MIDI_TRACE_EVENT_MESSAGE_FIELD),
                    TraceLoggingUInt8(messageWordCount, "declared word count"),
                    TraceLoggingUInt64(static_cast<uint64_t>(words.size() - index), "words remaining")
                );

                break;
            }

            LOG_IF_FAILED(callback->Callback(
                MessageOptionFlags::MessageOptionFlags_None,
                (PVOID)(&words[index]),
                (UINT)(messageWordCount * sizeof(uint32_t)),
                timestamp, 
                (LONGLONG)0));            // todo: may need to pass along the context

            index += messageWordCount;
        }
    }

    TraceLoggingWrite(
        MidiNetworkMidiTransportTelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_INFO,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this"),
        TraceLoggingWideString(L"Exit", MIDI_TRACE_EVENT_MESSAGE_FIELD)
    );

    return S_OK;
}

HRESULT 
MidiNetworkConnection::ResetSequenceNumbers()
{
    // reset the last sent sequence number
    m_lastSentUmpCommandSequenceNumber = 0;
    m_lastSentUmpCommandSequenceNumber--;       // prepare for next

    // reset the last received sequence number.
    m_lastReceivedUmpCommandSequenceNumber = 0;
    m_lastReceivedUmpCommandSequenceNumber--;

    // clear out retransmit buffer
    m_retransmitBuffer.clear();

    return S_OK;
}



HRESULT
MidiNetworkConnection::EndActiveSessionDueToTimeout()
{
    TraceLoggingWrite(
        MidiNetworkMidiTransportTelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_WARNING,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_WARNING),
        TraceLoggingPointer(this, "this"),
        TraceLoggingWideString(L"Ending session due to timeout", MIDI_TRACE_EVENT_MESSAGE_FIELD)
    );

    // Tell the remote first. Ending the session tears down the state this needs.
    LOG_IF_FAILED(SendToNetwork([](MidiNetworkDataWriter& writer)
        {
            RETURN_IF_FAILED(writer.WriteCommandBye(MidiNetworkCommandByeReason::CommandByeReasonCommon_Timeout, internal::ResourceGetWString(IDS_MESSAGE_SESSION_TIMED_OUT)));

            return S_OK;
        }));

    LOG_IF_FAILED(EndActiveSession(false));

    OnSessionEndedByRemote();

    return S_OK;
}

_Use_decl_annotations_
HRESULT
MidiNetworkConnection::EndActiveSession(bool respondWithByeReply)
{
    TraceLoggingWrite(
        MidiNetworkMidiTransportTelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_INFO,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this"),
        TraceLoggingWideString(L"Enter", MIDI_TRACE_EVENT_MESSAGE_FIELD)
    );

    m_sessionActive = false;

    // Release our reference to the client callback before anything that can re-enter. Deleting
    // the endpoint synchronously shuts down the Bidi, which calls back into this connection.
    auto callback = DetachCallback();
    callback.reset();

    // clear the outbound queue
    {
        auto queueLock = m_outgoingUmpMessageQueueLock.lock();
        m_outgoingUmpMessages.clear();
    }

    {
        auto lock = m_socketWriterLock.lock();
        LOG_IF_FAILED(ResetSequenceNumbers());
    }

    {
        auto lock = m_pingTrackingLock.lock();
        m_outgoingPingTracking.clear();
    }

    if (respondWithByeReply)
    {
        LOG_IF_FAILED(SendToNetwork([](MidiNetworkDataWriter& writer)
            {
                RETURN_IF_FAILED(writer.WriteCommandByeReply());

                return S_OK;
            }));
    }

    if (!m_sessionDeviceInstanceId.empty())
    {
        auto endpointManager = TransportState::Current().GetEndpointManager();

        if (endpointManager != nullptr)
        {
            LOG_IF_FAILED(endpointManager->DeleteEndpoint(m_sessionDeviceInstanceId));
        }

        m_sessionDeviceInstanceId.clear();
    }

    // clear the association with the SWD
    if (!m_sessionEndpointDeviceInterfaceId.empty())
    {
        LOG_IF_FAILED(TransportState::Current().DisassociateMidiEndpointFromConnection(m_sessionEndpointDeviceInterfaceId));
        m_sessionEndpointDeviceInterfaceId.clear();
    }

    // The writer deliberately outlives the session. A session ending is not the connection
    // ending: the same remote address and port may send a fresh invitation, and destroying the
    // writer here is what previously made the connection permanently unusable.

    TraceLoggingWrite(
        MidiNetworkMidiTransportTelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_INFO,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this"),
        TraceLoggingWideString(L"Exit", MIDI_TRACE_EVENT_MESSAGE_FIELD)
    );

    return S_OK;
}


HRESULT
MidiNetworkConnection::SendShutdownBye()
{
    // Nothing to say if the session already ended - the remote either sent us the Bye or timed
    // out, and an unsolicited Bye would just earn a "session not established" refusal.
    if (!m_sessionActive)
    {
        return S_FALSE;
    }

    // teardown is two-phase, so this can arrive twice for the same connection
    if (m_shutdownByeSent.exchange(true))
    {
        return S_FALSE;
    }

    // Fire and forget. Waiting for a Bye Reply here only delays a shutdown that is already
    // under way, and with many sessions those waits add up against the service stop timeout.
    // If the datagram is lost the remote falls back to its own ping timeout.
    LOG_IF_FAILED(SendToNetwork([](MidiNetworkDataWriter& writer)
        {
            // "Power Down" rather than "User terminated", because the latter tends to make the
            // remote discard what it knows about us. This path covers both service shutdown and
            // an explicit disconnect, so the message says neither.
            RETURN_IF_FAILED(writer.WriteCommandBye(
                MidiNetworkCommandByeReason::CommandByeReasonCommon_PowerDown,
                internal::ResourceGetWString(IDS_MESSAGE_CONNECTION_ENDED).c_str()));

            return S_OK;
        }));

    return S_OK;
}

HRESULT
MidiNetworkConnection::SendUserTerminatedByeAndAwaitReply()
{
    if (!m_sessionActive || m_shuttingDown)
    {
        return S_FALSE;
    }

    m_byeReplyEvent.ResetEvent();

    bool replyReceived{ false };
    uint16_t attempts{ 0 };

    while (attempts < MIDI_NETWORK_BYE_MAX_ATTEMPTS && !m_shuttingDown)
    {
        attempts++;

        LOG_IF_FAILED(SendToNetwork([](MidiNetworkDataWriter& writer)
            {
                RETURN_IF_FAILED(writer.WriteCommandBye(
                    MidiNetworkCommandByeReason::CommandByeReasonCommon_UserTerminated,
                    internal::ResourceGetWString(IDS_MESSAGE_USER_DISCONNECTED).c_str()));

                return S_OK;
            }));

        if (m_byeReplyEvent.wait(MIDI_NETWORK_BYE_REPLY_TIMEOUT_MILLISECONDS))
        {
            // Shutdown sets this too, so a wake is only a reply if we are not shutting down
            replyReceived = !m_shuttingDown;
            break;
        }
    }

    TraceLoggingWrite(
        MidiNetworkMidiTransportTelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_INFO,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this"),
        TraceLoggingWideString(L"User-initiated disconnect", MIDI_TRACE_EVENT_MESSAGE_FIELD),
        TraceLoggingBoolean(replyReceived, "bye reply received"),
        TraceLoggingUInt16(attempts, "attempts"),
        TraceLoggingBoolean(m_shuttingDown, "shutting down")
    );

    // The session is over whether or not the remote acknowledged, and marking it ended here is
    // what stops Shutdown() from sending a second Bye with a different reason.
    LOG_IF_FAILED(EndActiveSession(false));

    return replyReceived ? S_OK : S_FALSE;
}

HRESULT
MidiNetworkConnection::HandleIncomingByeReply()
{
    // Only a user-initiated disconnect waits on this. The shutdown Bye is fire-and-forget.
    m_byeReplyEvent.SetEvent();

    return S_OK;
}

HRESULT
MidiNetworkConnection::HandleIncomingBye()
{
    TraceLoggingWrite(
        MidiNetworkMidiTransportTelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_INFO,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this"),
        TraceLoggingWideString(L"Enter", MIDI_TRACE_EVENT_MESSAGE_FIELD)
    );

    // whatever the outcome, the remote has answered us
    OnInvitationAnswered();

    if (m_sessionActive)
    {
        LOG_IF_FAILED(EndActiveSession(true));

        // the remote said goodbye on its own, so this is one to re-establish
        OnSessionEndedByRemote();
    }
    else
    {
        // No session means any endpoint we were told to build for this remote is now pointless.
        OnSessionEndedBeforeEndpointCreated();

        // Spec 6.16: "Because the Bye Command might be repeated, the Bye Reply shall also be
        // sent if there is no Pending or Established Session." Staying silent here leaves the
        // sender repeating until its own timeout.
        LOG_IF_FAILED(SendToNetwork([](MidiNetworkDataWriter& writer)
            {
                RETURN_IF_FAILED(writer.WriteCommandByeReply());

                return S_OK;
            }));
    }

    TraceLoggingWrite(
        MidiNetworkMidiTransportTelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_INFO,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this"),
        TraceLoggingWideString(L"Exit", MIDI_TRACE_EVENT_MESSAGE_FIELD)
    );

    return S_OK;
}


_Use_decl_annotations_
HRESULT
MidiNetworkConnection::HandleIncomingInvitationReplyAccepted(
    MidiNetworkCommandPacketHeader const& header,
    std::wstring const& remoteHostUmpEndpointName,
    std::wstring const& remoteHostProductInstanceId
)
{
    UNREFERENCED_PARAMETER(remoteHostUmpEndpointName);
    UNREFERENCED_PARAMETER(remoteHostProductInstanceId);

    TraceLoggingWrite(
        MidiNetworkMidiTransportTelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_ERROR,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_ERROR),
        TraceLoggingPointer(this, "this"),
        TraceLoggingWideString(L"We are not in the client role, but received an invitation accept. Not normal.", MIDI_TRACE_EVENT_MESSAGE_FIELD)
    );

    // we are a host, not a client, so NAK this per spec 6.4
    LOG_IF_FAILED(SendToNetwork([&header](MidiNetworkDataWriter& writer)
        {
            RETURN_IF_FAILED(writer.WriteCommandNAK(header.HeaderWord, MidiNetworkCommandNAKReason::CommandNAKReason_CommandNotExpected, internal::ResourceGetWString(IDS_MESSAGE_UNEXPECTED_INVITATION_ACCEPT)));

            return S_OK;
        }));

    return S_OK;
}

_Use_decl_annotations_
HRESULT
MidiNetworkConnection::HandleIncomingInvitation(
    MidiNetworkCommandPacketHeader const& header,
    MidiNetworkCommandInvitationCapabilities const& capabilities,
    std::wstring const& clientUmpEndpointName,
    std::wstring const& clientProductInstanceId
)
{
    UNREFERENCED_PARAMETER(capabilities);
    UNREFERENCED_PARAMETER(clientUmpEndpointName);
    UNREFERENCED_PARAMETER(clientProductInstanceId);

    // we are a client, not a host, so NAK this per spec 6.4
    LOG_IF_FAILED(SendToNetwork([&header](MidiNetworkDataWriter& writer)
        {
            RETURN_IF_FAILED(writer.WriteCommandNAK(header.HeaderWord, MidiNetworkCommandNAKReason::CommandNAKReason_CommandNotExpected, internal::ResourceGetWString(IDS_MESSAGE_UNEXPECTED_INVITATION)));

            return S_OK;
        }));

    return S_OK;
}

_Use_decl_annotations_
HRESULT
MidiNetworkConnection::HandleIncomingInvitationWithAuthentication(
    MidiNetworkCommandPacketHeader const& header,
    MidiNetworkAuthenticationKind const kind)
{
    UNREFERENCED_PARAMETER(header);
    UNREFERENCED_PARAMETER(kind);

    // Only a host is ever answered with this. A client receiving one has no challenge in
    // flight, so it withdraws rather than leaving the remote waiting.
    return RefuseInvitationForAuthentication(MidiNetworkCommandByeReason::CommandByeReasonClientToHost_InvitationCanceled);
}

void
MidiNetworkConnection::AbandonCurrentRetransmitRequest()
{
    // Forcing the attempt count to the limit makes the next gapped UMP Data command
    // resynchronize instead of asking again.
    m_retransmitRequestAttempts = MIDI_NETWORK_MAX_RETRANSMIT_REQUEST_ATTEMPTS;
}

void
MidiNetworkConnection::ResetRetransmitRequestState()
{
    m_retransmitRequestOutstanding = false;
    m_retransmitRequestAttempts = 0;
    m_retransmitRequestSequenceNumber = 0;
}

_Use_decl_annotations_
HRESULT
MidiNetworkConnection::HandleIncomingNAK(
    MidiNetworkCommandNAKReason const reason,
    MidiNetworkCommandPacketHeader const& originalCommandHeader,
    std::wstring const& text)
{
    TraceLoggingWrite(
        MidiNetworkMidiTransportTelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_WARNING,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_WARNING),
        TraceLoggingPointer(this, "this"),
        TraceLoggingWideString(L"Remote endpoint sent a NAK", MIDI_TRACE_EVENT_MESSAGE_FIELD),
        TraceLoggingUInt8(reason, "reason"),
        TraceLoggingUInt8(originalCommandHeader.HeaderData.CommandCode, "NAKed command code"),
        TraceLoggingWideString(text.c_str(), "text")
    );

    if (originalCommandHeader.HeaderData.CommandCode == MidiNetworkCommandCode::CommandCommon_RetransmitRequest)
    {
        if (reason == MidiNetworkCommandNAKReason::CommandNAKReason_CommandNotSupported)
        {
            // Spec 7.2.3: the remote does not implement retransmit, so we must stop asking
            // for the rest of the session and just live with the gaps.
            m_remoteSupportsRetransmit = false;

            TraceLoggingWrite(
                MidiNetworkMidiTransportTelemetryProvider::Provider(),
                MIDI_TRACE_EVENT_INFO,
                TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
                TraceLoggingLevel(WINEVENT_LEVEL_INFO),
                TraceLoggingPointer(this, "this"),
                TraceLoggingWideString(L"Remote endpoint does not support retransmit. No further retransmit requests will be sent for this session.", MIDI_TRACE_EVENT_MESSAGE_FIELD)
            );
        }

        AbandonCurrentRetransmitRequest();
    }

    return S_OK;
}

_Use_decl_annotations_
HRESULT
MidiNetworkConnection::HandleIncomingRetransmitError(
    MidiNetworkCommandRetransmitErrorReason const reason,
    uint16_t const sequenceNumber)
{
    TraceLoggingWrite(
        MidiNetworkMidiTransportTelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_WARNING,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_WARNING),
        TraceLoggingPointer(this, "this"),
        TraceLoggingWideString(L"Remote endpoint cannot fulfill the retransmit request. Accepting the loss.", MIDI_TRACE_EVENT_MESSAGE_FIELD),
        TraceLoggingUInt8(reason, "reason"),
        TraceLoggingUInt16(sequenceNumber, "earliest available sequence number")
    );

    // The data is gone. Waiting for it would stall the session permanently.
    AbandonCurrentRetransmitRequest();

    return S_OK;
}

HRESULT
MidiNetworkConnection::HandleIncomingSessionReset()
{
    TraceLoggingWrite(
        MidiNetworkMidiTransportTelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_INFO,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this"),
        TraceLoggingWideString(L"Remote endpoint requested a session reset", MIDI_TRACE_EVENT_MESSAGE_FIELD)
    );

    {
        auto lock = m_socketWriterLock.lock();
        LOG_IF_FAILED(ResetSequenceNumbers());
    }

    ResetRetransmitRequestState();

    // spec 6.13: the reset is only complete once we have acknowledged it
    RETURN_IF_FAILED(SendToNetwork([](MidiNetworkDataWriter& writer)
        {
            RETURN_IF_FAILED(writer.WriteCommandSessionResetReply());

            return S_OK;
        }));

    return S_OK;
}

HRESULT
MidiNetworkConnection::HandleIncomingSessionResetReply()
{
    TraceLoggingWrite(
        MidiNetworkMidiTransportTelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_INFO,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this"),
        TraceLoggingWideString(L"Remote endpoint acknowledged our session reset", MIDI_TRACE_EVENT_MESSAGE_FIELD)
    );

    {
        auto lock = m_socketWriterLock.lock();
        LOG_IF_FAILED(ResetSequenceNumbers());
    }

    ResetRetransmitRequestState();

    return S_OK;
}


_Use_decl_annotations_
HRESULT
MidiNetworkConnection::RefuseSessionForEndpointCreationFailure(HRESULT const creationResult)
{
    bool alreadyAttached = (creationResult == HRESULT_FROM_WIN32(ERROR_DEVICE_ALREADY_ATTACHED));

    MidiNetworkCommandByeReason reason{ MidiNetworkCommandByeReason::CommandByeReasonCommon_Undefined };
    std::wstring message{ internal::ResourceGetWString(IDS_MESSAGE_ENDPOINT_CREATION_FAILED) };

    if (alreadyAttached)
    {
        reason = ByeReasonForDeviceAlreadyAttached();

        message = internal::ResourceGetWString(IDS_MESSAGE_DEVICE_ALREADY_CONNECTED);
    }

    TraceLoggingWrite(
        MidiNetworkMidiTransportTelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_WARNING,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_WARNING),
        TraceLoggingPointer(this, "this"),
        TraceLoggingWideString(L"Declining session because the MIDI endpoint could not be created", MIDI_TRACE_EVENT_MESSAGE_FIELD),
        TraceLoggingWideString(message.c_str(), "reason text"),
        TraceLoggingHResult(creationResult, MIDI_TRACE_EVENT_HRESULT_FIELD)
    );

    LOG_IF_FAILED(SendToNetwork([&reason, &message](MidiNetworkDataWriter& writer)
        {
            RETURN_IF_FAILED(writer.WriteCommandBye(reason, message));

            return S_OK;
        }));

    return S_OK;
}

_Use_decl_annotations_
HRESULT
MidiNetworkConnection::RefuseInvitationForAuthentication(MidiNetworkCommandByeReason const reason)
{
    TraceLoggingWrite(
        MidiNetworkMidiTransportTelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_WARNING,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_WARNING),
        TraceLoggingPointer(this, "this"),
        TraceLoggingWideString(L"Refusing invitation because authentication is not supported", MIDI_TRACE_EVENT_MESSAGE_FIELD),
        TraceLoggingUInt8(reason, "bye reason")
    );

    RETURN_IF_FAILED(SendToNetwork([&reason](MidiNetworkDataWriter& writer)
        {
            RETURN_IF_FAILED(writer.WriteCommandBye(reason, internal::ResourceGetWString(IDS_MESSAGE_AUTHENTICATION_NOT_SUPPORTED)));

            return S_OK;
        }));

    return S_OK;
}

_Use_decl_annotations_
HRESULT
MidiNetworkConnection::HandleIncomingInvitationReplyAuthenticationRequired(
    MidiNetworkCommandPacketHeader const& header,
    MidiNetworkAuthenticationKind const kind)
{
    UNREFERENCED_PARAMETER(header);
    UNREFERENCED_PARAMETER(kind);

    // Only a client has an invitation in flight to be challenged over.
    return RefuseInvitationForAuthentication(MidiNetworkCommandByeReason::CommandByeReasonClientToHost_InvitationCanceled);
}

HRESULT
MidiNetworkConnection::HandleIncomingInvitationReplyPending()
{
    // Only a client is answered with this, and it has nothing to wait for.
    return S_OK;
}

_Use_decl_annotations_
HRESULT
MidiNetworkConnection::ComputeAuthenticationDigest(
    uint8_t const* nonce,
    size_t const nonceByteCount,
    MidiNetworkSecret const& secret,
    uint8_t* digest,
    size_t const digestByteCount)
{
    UNREFERENCED_PARAMETER(nonce);
    UNREFERENCED_PARAMETER(nonceByteCount);
    UNREFERENCED_PARAMETER(secret);

    RETURN_HR_IF_NULL(E_INVALIDARG, digest);
    RETURN_HR_IF(E_INVALIDARG, digestByteCount == 0);

    SecureZeroMemory(digest, digestByteCount);

    // Must follow spec Appendix B exactly, including the order the nonce and secret are fed to
    // the hash. Approximating it would interoperate with nothing and would look like it worked.
    // TODO: https://github.com/microsoft/MIDI/issues/733
    return E_NOTIMPL;
}


_Use_decl_annotations_
HRESULT
MidiNetworkConnection::SendByeSessionNotEstablished(uint8_t const commandCode)
{
    // One remote per connection, so a constant key is all that is needed here. Without this a
    // peer can make us emit a refusal for every command in every datagram it sends.
    if (!m_replyRateLimiter.ShouldSend(0))
    {
        return S_FALSE;
    }

    TraceLoggingWrite(
        MidiNetworkMidiTransportTelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_WARNING,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_WARNING),
        TraceLoggingPointer(this, "this"),
        TraceLoggingWideString(L"Command received outside an established session", MIDI_TRACE_EVENT_MESSAGE_FIELD),
        TraceLoggingUInt8(commandCode, "Command Code")
    );

    RETURN_IF_FAILED(SendToNetwork([](MidiNetworkDataWriter& writer)
        {
            RETURN_IF_FAILED(writer.WriteCommandBye(MidiNetworkCommandByeReason::CommandByeReasonCommon_SessionNotEstablished, internal::ResourceGetWString(IDS_MESSAGE_NO_SESSION_ESTABLISHED)));

            return S_OK;
        }));

    return S_OK;
}




_Use_decl_annotations_
HRESULT
MidiNetworkConnection::HandleIncomingPing(uint32_t const pingId)
{
    TraceLoggingWrite(
        MidiNetworkMidiTransportTelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_INFO,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this"),
        TraceLoggingWideString(L"Enter", MIDI_TRACE_EVENT_MESSAGE_FIELD)
    );

    RETURN_IF_FAILED(SendToNetwork([&pingId](MidiNetworkDataWriter& writer)
        {
            RETURN_IF_FAILED(writer.WriteCommandPingReply(pingId));

            return S_OK;
        }));

    TraceLoggingWrite(
        MidiNetworkMidiTransportTelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_INFO,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this"),
        TraceLoggingWideString(L"Exit", MIDI_TRACE_EVENT_MESSAGE_FIELD)
    );

    return S_OK;
}

_Use_decl_annotations_
HRESULT
MidiNetworkConnection::HandleIncomingPingReply(uint32_t const pingId)
{
    TraceLoggingWrite(
        MidiNetworkMidiTransportTelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_INFO,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this"),
        TraceLoggingWideString(L"Enter", MIDI_TRACE_EVENT_MESSAGE_FIELD)
    );

    auto timestamp = internal::GetCurrentMidiTimestamp();

    {
        auto lock = m_pingTrackingLock.lock();

        for (auto& pingEntry : m_outgoingPingTracking)
        {
            if (pingEntry.PingId == pingId)
            {
                pingEntry.PingReceiveTimestamp = timestamp;
                pingEntry.Received = true;

                // calculate latency
                AddLatencyToAverageLatencyTicks(pingEntry.PingReceiveTimestamp - pingEntry.PingSendTimestamp);

                // we may want to do a running average here instead of just the last.

                // todo: update the latency properties used in the scheduler

                break;
            }
        }
    }

    TraceLoggingWrite(
        MidiNetworkMidiTransportTelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_INFO,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this"),
        TraceLoggingWideString(L"Exit", MIDI_TRACE_EVENT_MESSAGE_FIELD)
    );

    return S_OK;

}


_Use_decl_annotations_
HRESULT
MidiNetworkConnection::ReadUtf8String(
    winrt::Windows::Storage::Streams::DataReader const& reader, 
    size_t const byteCount,
    std::wstring& value)
{
    value.clear();

    if (byteCount == 0)
    {
        return S_OK;
    }

    // the length is remote-supplied, so it is never trusted against the actual datagram
    RETURN_HR_IF(E_INVALIDARG, byteCount > reader.UnconsumedBufferLength());

    auto bytes = std::vector<byte>(byteCount);

    try
    {
        reader.ReadBytes(bytes);
    }
    catch (...)
    {
        RETURN_IF_FAILED(wil::ResultFromCaughtException());
    }

    // strings on the wire are zero-padded out to a 32-bit word boundary. Without trimming, the
    // padding becomes embedded nulls and every later comparison against the string fails.
    auto stringEnd = std::find(bytes.begin(), bytes.end(), (byte)0);

    std::string s(bytes.begin(), stringEnd);

    try
    {
#pragma warning (push)
#pragma warning (disable: 4996)
        std::wstring_convert<std::codecvt_utf8<wchar_t>> convert;
        value = convert.from_bytes(s);
#pragma warning (pop)
    }
    catch (...)
    {
        // malformed UTF-8 from the remote. Recoverable, but we can't use the string.
        value.clear();

        TraceLoggingWrite(
            MidiNetworkMidiTransportTelemetryProvider::Provider(),
            MIDI_TRACE_EVENT_WARNING,
            TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
            TraceLoggingLevel(WINEVENT_LEVEL_WARNING),
            TraceLoggingPointer(this, "this"),
            TraceLoggingWideString(L"Remote endpoint sent a string which is not valid UTF-8", MIDI_TRACE_EVENT_MESSAGE_FIELD)
        );

        RETURN_IF_FAILED(HRESULT_FROM_WIN32(ERROR_NO_UNICODE_TRANSLATION));
    }

    return S_OK;
}

// Attempt counting and give-up live in the caller, which is the only place that knows the
// sequence numbers involved.
HRESULT
MidiNetworkConnection::RequestMissingPackets()
{
    TraceLoggingWrite(
        MidiNetworkMidiTransportTelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_INFO,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this"),
        TraceLoggingWideString(L"Enter", MIDI_TRACE_EVENT_MESSAGE_FIELD)
    );

    // this requests all packets after the last one we received
    auto startingSequenceNumber = m_lastReceivedUmpCommandSequenceNumber + 1;

    RETURN_IF_FAILED(SendToNetwork([&startingSequenceNumber](MidiNetworkDataWriter& writer)
        {
            RETURN_IF_FAILED(writer.WriteCommandRetransmitRequest(startingSequenceNumber, 0));

            return S_OK;
        }));

    TraceLoggingWrite(
        MidiNetworkMidiTransportTelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_INFO,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this"),
        TraceLoggingWideString(L"Exit", MIDI_TRACE_EVENT_MESSAGE_FIELD)
    );

    return S_OK;
}

_Use_decl_annotations_
HRESULT
MidiNetworkConnection::ProcessIncomingMessage(
    winrt::Windows::Storage::Streams::DataReader const& reader,
    uint32_t const firstCommandHeaderWord
)
{
    TraceLoggingWrite(
        MidiNetworkMidiTransportTelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_INFO,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this"),
        TraceLoggingWideString(L"Enter", MIDI_TRACE_EVENT_MESSAGE_FIELD)
    );

    m_totalNetworkPacketsReceived++;

    // Datagrams for the same remote can be dispatched on more than one thread pool thread, so
    // parsing is serialized here. Sequence tracking and the retransmit buffer are not reentrant.
    auto incomingLock = m_incomingMessageLock.lock();

    // we've received a new message, so reset our disconnect event
    // this also sets the timestamp of the incoming
    LOG_IF_FAILED(SignalHealthyConnectionAndUpdateArrivalTimestamp());

    // one retransmit request per datagram at most, no matter how many gaps it exposes
    bool alreadyRequestedRetransmit{ false };

    // and one Bye per datagram, so a peer talking to a dead session can't make us flood it
    bool alreadySentSessionNotEstablished{ false };

    try
    {
        uint32_t commandHeaderWord = firstCommandHeaderWord;
        bool haveCommand = true;

        while (haveCommand)
        {
            MidiNetworkCommandPacketHeader commandHeader;
            commandHeader.HeaderWord = commandHeaderWord;

            // Everything below this point is remote-supplied. Validating the declared payload
            // length against what actually arrived is what keeps every handler in bounds.
            uint32_t const payloadLengthInBytes = static_cast<uint32_t>(commandHeader.HeaderData.CommandPayloadLength) * sizeof(uint32_t);

            if (payloadLengthInBytes > reader.UnconsumedBufferLength())
            {
                TraceLoggingWrite(
                    MidiNetworkMidiTransportTelemetryProvider::Provider(),
                    MIDI_TRACE_EVENT_WARNING,
                    TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
                    TraceLoggingLevel(WINEVENT_LEVEL_WARNING),
                    TraceLoggingPointer(this, "this"),
                    TraceLoggingWideString(L"Command declares a payload longer than the datagram. Discarding remainder.", MIDI_TRACE_EVENT_MESSAGE_FIELD),
                    TraceLoggingUInt8(commandHeader.HeaderData.CommandCode, "Command Code"),
                    TraceLoggingUInt32(payloadLengthInBytes, "declared payload bytes"),
                    TraceLoggingUInt32(reader.UnconsumedBufferLength(), "bytes remaining")
                );

                break;
            }

            uint32_t const unconsumedLengthAfterCommand = reader.UnconsumedBufferLength() - payloadLengthInBytes;

            switch (commandHeader.HeaderData.CommandCode)
            {
            case CommandCommon_NAK:
            {
                auto reason = static_cast<MidiNetworkCommandNAKReason>(commandHeader.HeaderData.CommandSpecificData.AsBytes.Byte1);

                // payload is the original command header word, optionally followed by text
                if (payloadLengthInBytes >= sizeof(uint32_t))
                {
                    MidiNetworkCommandPacketHeader originalCommandHeader;
                    originalCommandHeader.HeaderWord = reader.ReadUInt32();

                    std::wstring text{ };
                    LOG_IF_FAILED(ReadUtf8String(reader, payloadLengthInBytes - sizeof(uint32_t), text));

                    LOG_IF_FAILED(HandleIncomingNAK(reason, originalCommandHeader, text));
                }
            }
                break;

            case CommandCommon_Ping:
                if (payloadLengthInBytes >= sizeof(uint32_t))
                {
                    LOG_IF_FAILED(HandleIncomingPing(reader.ReadUInt32()));
                }
                break;

            case CommandCommon_PingReply:
                if (payloadLengthInBytes >= sizeof(uint32_t))
                {
                    LOG_IF_FAILED(HandleIncomingPingReply(reader.ReadUInt32()));
                }
                break;

            case CommandCommon_Bye:
                LOG_IF_FAILED(HandleIncomingBye());
                break;

            case CommandCommon_ByeReply:
                LOG_IF_FAILED(HandleIncomingByeReply());
                break;

            case CommandClientToHost_Invitation:
            {
                uint32_t endpointNameLengthInBytes = static_cast<uint32_t>(commandHeader.HeaderData.CommandSpecificData.AsBytes.Byte1) * sizeof(uint32_t);
                auto capabilities = static_cast<MidiNetworkCommandInvitationCapabilities>(commandHeader.HeaderData.CommandSpecificData.AsBytes.Byte2);

                // the name length is a portion of the payload. If it claims more, the product
                // instance id length would underflow
                if (endpointNameLengthInBytes <= payloadLengthInBytes)
                {
                    uint32_t productInstanceIdLengthInBytes = payloadLengthInBytes - endpointNameLengthInBytes;

                    std::wstring clientEndpointName{ };
                    std::wstring clientProductInstanceId{ };

                    if (SUCCEEDED(ReadUtf8String(reader, endpointNameLengthInBytes, clientEndpointName)) &&
                        SUCCEEDED(ReadUtf8String(reader, productInstanceIdLengthInBytes, clientProductInstanceId)))
                    {
                        LOG_IF_FAILED(HandleIncomingInvitation(commandHeader, capabilities, clientEndpointName, clientProductInstanceId));
                    }
                }
                else
                {
                    TraceLoggingWrite(
                        MidiNetworkMidiTransportTelemetryProvider::Provider(),
                        MIDI_TRACE_EVENT_WARNING,
                        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
                        TraceLoggingLevel(WINEVENT_LEVEL_WARNING),
                        TraceLoggingPointer(this, "this"),
                        TraceLoggingWideString(L"Invitation declares an endpoint name longer than its payload. Ignoring.", MIDI_TRACE_EVENT_MESSAGE_FIELD)
                    );
                }
            }
                break;


            case CommandClientToHost_InvitationWithAuthentication:
                LOG_IF_FAILED(HandleIncomingInvitationWithAuthentication(commandHeader, MidiNetworkAuthenticationKind::SharedSecret));
                break;

            case CommandClientToHost_InvitationWithUserAuthentication:
                LOG_IF_FAILED(HandleIncomingInvitationWithAuthentication(commandHeader, MidiNetworkAuthenticationKind::UserCredential));
                break;

            case CommandCommon_UmpData:
            {
                if (!m_sessionActive)
                {
                    if (!alreadySentSessionNotEstablished)
                    {
                        alreadySentSessionNotEstablished = true;
                        LOG_IF_FAILED(SendByeSessionNotEstablished(commandHeader.HeaderData.CommandCode));
                    }

                    // the payload is skipped by the resynchronization below
                    break;
                }

                uint8_t numberOfWords = commandHeader.HeaderData.CommandPayloadLength;
                MidiSequenceNumber sequenceNumber(commandHeader.HeaderData.CommandSpecificData.AsUInt16);

                std::vector<uint32_t> words{ };

                if (sequenceNumber <= m_lastReceivedUmpCommandSequenceNumber)
                {
                    // already seen. This is FEC or a retransmit, so the payload is skipped below.
                }
                else if (sequenceNumber == m_lastReceivedUmpCommandSequenceNumber + 1)
                {
                    // Process UMP data because this is the next expected sequence number
                    // a command with zero words is a valid keep-alive and still advances the sequence

                    m_lastReceivedUmpCommandSequenceNumber = sequenceNumber;

                    // we're back in sequence, so any gap we were chasing is resolved
                    ResetRetransmitRequestState();

                    words.reserve(numberOfWords);

                    for (uint8_t i = 0; i < numberOfWords; i++)
                    {
                        words.push_back(reader.ReadUInt32());
                    }
                }
                else
                {
                    // A gap, which means we lost more datagrams than the forward error correction
                    // window covers. We ask for a retransmit a bounded number of times, then accept
                    // the loss and carry on. A remote that cannot or will not retransmit must never
                    // be able to wedge the session by leaving us stuck on a sequence number.

                    auto expectedSequenceNumber = m_lastReceivedUmpCommandSequenceNumber + 1;

                    if (!m_retransmitRequestOutstanding || !(m_retransmitRequestSequenceNumber == expectedSequenceNumber))
                    {
                        // a different gap than the one we were chasing
                        m_retransmitRequestOutstanding = true;
                        m_retransmitRequestSequenceNumber = expectedSequenceNumber;
                        m_retransmitRequestAttempts = 0;
                    }

                    bool waitForRetransmit{ false };

                    if (m_remoteSupportsRetransmit && m_retransmitRequestAttempts < MIDI_NETWORK_MAX_RETRANSMIT_REQUEST_ATTEMPTS)
                    {
                        if (alreadyRequestedRetransmit)
                        {
                            // already asked once for this datagram. Wait for the answer.
                            waitForRetransmit = true;
                        }
                        else
                        {
                            m_retransmitRequestAttempts++;
                            alreadyRequestedRetransmit = true;

                            waitForRetransmit = SUCCEEDED(RequestMissingPackets());
                        }
                    }

                    if (!waitForRetransmit)
                    {
                        TraceLoggingWrite(
                            MidiNetworkMidiTransportTelemetryProvider::Provider(),
                            MIDI_TRACE_EVENT_WARNING,
                            TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
                            TraceLoggingLevel(WINEVENT_LEVEL_WARNING),
                            TraceLoggingPointer(this, "this"),
                            TraceLoggingWideString(L"Giving up on missing UMP data and resynchronizing to the current sequence number", MIDI_TRACE_EVENT_MESSAGE_FIELD),
                            TraceLoggingUInt16(expectedSequenceNumber.Value(), "expected sequence number"),
                            TraceLoggingUInt16(sequenceNumber.Value(), "received sequence number"),
                            TraceLoggingBoolean(m_remoteSupportsRetransmit, "remote supports retransmit")
                        );

                        m_lastReceivedUmpCommandSequenceNumber = sequenceNumber;

                        ResetRetransmitRequestState();

                        words.reserve(numberOfWords);

                        for (uint8_t i = 0; i < numberOfWords; i++)
                        {
                            words.push_back(reader.ReadUInt32());
                        }
                    }
                }

                if (words.size() > 0)
                {
                    LOG_IF_FAILED(HandleIncomingUmpData(m_lastIncomingValidUdpPacketTimestamp, words));
                }
            }
                break;

            case CommandCommon_RetransmitRequest:
            {
                if (!m_sessionActive)
                {
                    if (!alreadySentSessionNotEstablished)
                    {
                        alreadySentSessionNotEstablished = true;
                        LOG_IF_FAILED(SendByeSessionNotEstablished(commandHeader.HeaderData.CommandCode));
                    }
                }
                else if (payloadLengthInBytes >= sizeof(uint32_t))
                {
                    uint16_t sequenceNumber = commandHeader.HeaderData.CommandSpecificData.AsUInt16;
                    uint16_t numberOfUmpCommands = reader.ReadUInt16();

                    reader.ReadUInt16();    // reserved

                    LOG_IF_FAILED(HandleIncomingRetransmitRequest(commandHeader, sequenceNumber, numberOfUmpCommands));
                }
            }
                break;
            case CommandCommon_RetransmitError:
            {
                if (!m_sessionActive)
                {
                    if (!alreadySentSessionNotEstablished)
                    {
                        alreadySentSessionNotEstablished = true;
                        LOG_IF_FAILED(SendByeSessionNotEstablished(commandHeader.HeaderData.CommandCode));
                    }
                }
                else if (payloadLengthInBytes >= sizeof(uint32_t))
                {
                    auto reason = static_cast<MidiNetworkCommandRetransmitErrorReason>(commandHeader.HeaderData.CommandSpecificData.AsBytes.Byte1);
                    uint16_t earliestAvailableSequenceNumber = reader.ReadUInt16();

                    reader.ReadUInt16();    // reserved

                    LOG_IF_FAILED(HandleIncomingRetransmitError(reason, earliestAvailableSequenceNumber));
                }
            }
                break;

            case CommandCommon_SessionReset:
                if (m_sessionActive)
                {
                    LOG_IF_FAILED(HandleIncomingSessionReset());
                }
                else if (!alreadySentSessionNotEstablished)
                {
                    alreadySentSessionNotEstablished = true;
                    LOG_IF_FAILED(SendByeSessionNotEstablished(commandHeader.HeaderData.CommandCode));
                }
                break;

            case CommandCommon_SessionResetReply:
                if (m_sessionActive)
                {
                    LOG_IF_FAILED(HandleIncomingSessionResetReply());
                }
                else if (!alreadySentSessionNotEstablished)
                {
                    alreadySentSessionNotEstablished = true;
                    LOG_IF_FAILED(SendByeSessionNotEstablished(commandHeader.HeaderData.CommandCode));
                }
                break;

            case CommandHostToClient_InvitationReplyAccepted:
            {
                uint32_t endpointNameLengthInBytes = static_cast<uint32_t>(commandHeader.HeaderData.CommandSpecificData.AsBytes.Byte1) * sizeof(uint32_t);

                if (endpointNameLengthInBytes <= payloadLengthInBytes)
                {
                    uint32_t productInstanceIdLengthInBytes = payloadLengthInBytes - endpointNameLengthInBytes;

                    std::wstring hostEndpointName{ };
                    std::wstring hostProductInstanceId{ };

                    if (SUCCEEDED(ReadUtf8String(reader, endpointNameLengthInBytes, hostEndpointName)) &&
                        SUCCEEDED(ReadUtf8String(reader, productInstanceIdLengthInBytes, hostProductInstanceId)))
                    {
                        LOG_IF_FAILED(HandleIncomingInvitationReplyAccepted(commandHeader, hostEndpointName, hostProductInstanceId));
                    }
                }
                else
                {
                    TraceLoggingWrite(
                        MidiNetworkMidiTransportTelemetryProvider::Provider(),
                        MIDI_TRACE_EVENT_WARNING,
                        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
                        TraceLoggingLevel(WINEVENT_LEVEL_WARNING),
                        TraceLoggingPointer(this, "this"),
                        TraceLoggingWideString(L"Invitation reply declares an endpoint name longer than its payload. Ignoring.", MIDI_TRACE_EVENT_MESSAGE_FIELD)
                    );
                }
            }
                break;

            case CommandHostToClient_InvitationReplyPending:
                LOG_IF_FAILED(HandleIncomingInvitationReplyPending());
                break;

            case CommandHostToClient_InvitationReplyAuthenticationRequired:
                LOG_IF_FAILED(HandleIncomingInvitationReplyAuthenticationRequired(commandHeader, MidiNetworkAuthenticationKind::SharedSecret));
                break;

            case CommandHostToClient_InvitationReplyUserAuthenticationRequired:
                LOG_IF_FAILED(HandleIncomingInvitationReplyAuthenticationRequired(commandHeader, MidiNetworkAuthenticationKind::UserCredential));
                break;


            default:
                TraceLoggingWrite(
                    MidiNetworkMidiTransportTelemetryProvider::Provider(),
                    MIDI_TRACE_EVENT_WARNING,
                    TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
                    TraceLoggingLevel(WINEVENT_LEVEL_WARNING),
                    TraceLoggingPointer(this, "this"),
                    TraceLoggingWideString(L"Unexpected network MIDI 2.0 command code", MIDI_TRACE_EVENT_MESSAGE_FIELD),
                    TraceLoggingUInt8(commandHeader.HeaderData.CommandCode, "Command Code")
                );

                // Spec 6.15. Only answered inside an established session, so unsolicited junk
                // from an arbitrary source doesn't get a reply.
                if (m_sessionActive)
                {
                    LOG_IF_FAILED(SendToNetwork([&commandHeader](MidiNetworkDataWriter& writer)
                        {
                            RETURN_IF_FAILED(writer.WriteCommandNAK(
                                commandHeader.HeaderWord,
                                MidiNetworkCommandNAKReason::CommandNAKReason_CommandNotSupported,
                                internal::ResourceGetWString(IDS_MESSAGE_COMMAND_NOT_SUPPORTED)));

                            return S_OK;
                        }));
                }

                break;

            }

            if (reader.UnconsumedBufferLength() < unconsumedLengthAfterCommand)
            {
                // a handler read past its own payload. We can no longer locate the next command.
                TraceLoggingWrite(
                    MidiNetworkMidiTransportTelemetryProvider::Provider(),
                    MIDI_TRACE_EVENT_ERROR,
                    TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
                    TraceLoggingLevel(WINEVENT_LEVEL_ERROR),
                    TraceLoggingPointer(this, "this"),
                    TraceLoggingWideString(L"Command handler over-consumed its payload. Discarding remainder of datagram.", MIDI_TRACE_EVENT_MESSAGE_FIELD),
                    TraceLoggingUInt8(commandHeader.HeaderData.CommandCode, "Command Code")
                );

                break;
            }

            // Skip whatever this command's payload holds that the handler didn't consume. Without
            // this, payload bytes of an unhandled command get parsed as the next command header.
            while (reader.UnconsumedBufferLength() > unconsumedLengthAfterCommand)
            {
                auto excess = reader.UnconsumedBufferLength() - unconsumedLengthAfterCommand;

                if (excess >= sizeof(uint32_t))
                {
                    reader.ReadUInt32();
                }
                else if (excess >= sizeof(uint16_t))
                {
                    reader.ReadUInt16();
                }
                else
                {
                    reader.ReadByte();
                }
            }

            if (reader.UnconsumedBufferLength() >= sizeof(uint32_t))
            {
                commandHeaderWord = reader.ReadUInt32();
            }
            else
            {
                haveCommand = false;
            }
        }
    }
    catch (...)
    {
        // a malformed datagram must never take the service down
        auto hr = wil::ResultFromCaughtException();

        TraceLoggingWrite(
            MidiNetworkMidiTransportTelemetryProvider::Provider(),
            MIDI_TRACE_EVENT_ERROR,
            TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
            TraceLoggingLevel(WINEVENT_LEVEL_ERROR),
            TraceLoggingPointer(this, "this"),
            TraceLoggingWideString(L"Exception processing inbound datagram. Datagram discarded.", MIDI_TRACE_EVENT_MESSAGE_FIELD),
            TraceLoggingHResult(hr, MIDI_TRACE_EVENT_HRESULT_FIELD)
        );
    }

    TraceLoggingWrite(
        MidiNetworkMidiTransportTelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_INFO,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this"),
        TraceLoggingWideString(L"Exit", MIDI_TRACE_EVENT_MESSAGE_FIELD)
    );

    return S_OK;
}

_Use_decl_annotations_
HRESULT
MidiNetworkConnection::AddUmpPacketToRetransmitBuffer(MidiSequenceNumber const sequenceNumber, std::vector<uint32_t> const& words)
{
    return AddUmpPacketToRetransmitBuffer(sequenceNumber, words.data(), words.size());
}

_Use_decl_annotations_
HRESULT
MidiNetworkConnection::AddUmpPacketToRetransmitBuffer(
    MidiSequenceNumber const sequenceNumber,
    uint32_t const* words,
    size_t const wordCount)
{
    if (m_retransmitBuffer.capacity() == 0)
    {
        return S_OK;
    }

    MidiRetransmitBufferEntry entry;
    entry.SequenceNumber = sequenceNumber;

    if (wordCount > 0 && words != nullptr)
    {
        entry.Words.assign(words, words + wordCount);
    }

    m_retransmitBuffer.push_back(std::move(entry));

    return S_OK;
}

_Use_decl_annotations_
size_t
MidiNetworkConnection::CalculateWholeUmpMessageWordCount(
    std::vector<uint32_t> const& words,
    size_t const position,
    size_t const maxWords)
{
    size_t count{ 0 };

    while (position + count < words.size())
    {
        auto messageWordCount = internal::GetUmpLengthInMidiWordsFromFirstWord(words[position + count]);

        if (messageWordCount == 0)
        {
            break;
        }

        // never split a UMP message across two commands
        if (position + count + messageWordCount > words.size())
        {
            break;
        }

        if (count + messageWordCount > maxWords)
        {
            break;
        }

        count += messageWordCount;
    }

    return count;
}

_Use_decl_annotations_
HRESULT
MidiNetworkConnection::HandleIncomingRetransmitRequest(
    MidiNetworkCommandPacketHeader const& header,
    uint16_t const startingSequenceNumber, 
    uint16_t const retransmitPacketCount)
{
    TraceLoggingWrite(
        MidiNetworkMidiTransportTelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_INFO,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this"),
        TraceLoggingWideString(L"Enter", MIDI_TRACE_EVENT_MESSAGE_FIELD)
    );

    // the retransmit buffer is guarded by the socket writer lock
    auto lock = m_socketWriterLock.lock();

    // Spec 7.2.3: if we don't implement retransmit at all, the answer is a NAK rather than a
    // retransmit error, and the remote is expected to stop asking.
    if (m_retransmitBuffer.capacity() == 0)
    {
        RETURN_IF_FAILED(SendToNetwork([&header](MidiNetworkDataWriter& writer)
            {
                RETURN_IF_FAILED(writer.WriteCommandNAK(
                    header.HeaderWord,
                    MidiNetworkCommandNAKReason::CommandNAKReason_CommandNotSupported,
                    internal::ResourceGetWString(IDS_MESSAGE_RETRANSMIT_DISABLED)));

                return S_OK;
            }));

        return S_OK;
    }

    // find the starting sequence number in the circular buffer
    auto firstPacket = std::find_if(m_retransmitBuffer.begin(), m_retransmitBuffer.end(), [&](const MidiRetransmitBufferEntry& s) { return s.SequenceNumber == startingSequenceNumber; });

    if (firstPacket == m_retransmitBuffer.end())
    {
        // Send a retransmit error

        auto earliestAvailable = m_retransmitBuffer.size() > 0 ? m_retransmitBuffer.begin()->SequenceNumber : MidiSequenceNumber(0);

        RETURN_IF_FAILED(SendToNetwork([&earliestAvailable](MidiNetworkDataWriter& writer)
            {
                RETURN_IF_FAILED(writer.WriteCommandRetransmitError(earliestAvailable, MidiNetworkCommandRetransmitErrorReason::RetransmitErrorReason_DataNotAvailable));

                return S_OK;
            }));
    }
    else
    {
        // A count larger than what we hold, or the "send everything" value of zero, is clamped
        // to what is actually in the buffer. Advancing the iterator past end() is undefined.
        size_t const availableCount = static_cast<size_t>(std::distance(firstPacket, m_retransmitBuffer.end()));
        size_t countRemaining = (retransmitPacketCount == 0) ? availableCount : min(static_cast<size_t>(retransmitPacketCount), availableCount);

        auto it = firstPacket;

        // Spread the reply across as many datagrams as it takes. DontFragment is set, so one
        // oversized datagram would simply be dropped and the remote would ask again forever.
        while (countRemaining > 0)
        {
            size_t budgetBytes{ MIDI_NETWORK_MAX_UDP_PAYLOAD_BYTES - sizeof(uint32_t) };
            size_t countThisDatagram{ 0 };

            for (auto probe = it; countThisDatagram < countRemaining && probe != m_retransmitBuffer.end(); probe++)
            {
                size_t cost = sizeof(uint32_t) + (probe->Words.size() * sizeof(uint32_t));

                if (cost > budgetBytes)
                {
                    break;
                }

                budgetBytes -= cost;
                countThisDatagram++;
            }

            if (countThisDatagram == 0)
            {
                // a single stored packet larger than a whole datagram should be impossible
                break;
            }

            RETURN_IF_FAILED(SendToNetwork([&it, &countThisDatagram](MidiNetworkDataWriter& writer)
                {
                    auto writeIterator = it;

                    for (size_t i = 0; i < countThisDatagram; i++, writeIterator++)
                    {
                        RETURN_IF_FAILED(writer.WriteCommandUmpMessages(writeIterator->SequenceNumber, writeIterator->Words.data(), static_cast<uint8_t>(writeIterator->Words.size())));
                    }

                    return S_OK;
                }));

            std::advance(it, countThisDatagram);
            countRemaining -= countThisDatagram;
        }
    }

    TraceLoggingWrite(
        MidiNetworkMidiTransportTelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_INFO,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this"),
        TraceLoggingWideString(L"Exit", MIDI_TRACE_EVENT_MESSAGE_FIELD)
    );

    return S_OK;
}


HRESULT
MidiNetworkConnection::SendPing()
{
    TraceLoggingWrite(
        MidiNetworkMidiTransportTelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_INFO,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this"),
        TraceLoggingWideString(L"Enter", MIDI_TRACE_EVENT_MESSAGE_FIELD)
    );

    MidiOutgoingPingTrackingEntry pingInfo;

    pingInfo.PingSendTimestamp = internal::GetCurrentMidiTimestamp();
    pingInfo.PingId = (uint32_t)(pingInfo.PingSendTimestamp & 0xFFFFFFFF);

    {
        auto lock = m_pingTrackingLock.lock();
        m_outgoingPingTracking.push_back(pingInfo);
    }

    RETURN_IF_FAILED(SendToNetwork([&pingInfo](MidiNetworkDataWriter& writer)
        {
            RETURN_IF_FAILED(writer.WriteCommandPing(pingInfo.PingId));

            return S_OK;
        }));

    TraceLoggingWrite(
        MidiNetworkMidiTransportTelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_INFO,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this"),
        TraceLoggingWideString(L"Exit", MIDI_TRACE_EVENT_MESSAGE_FIELD)
    );

    return S_OK;
}







_Use_decl_annotations_
HRESULT
MidiNetworkConnection::OutboundProcessingThreadWorker(std::stop_token stopToken)
{
    TraceLoggingWrite(
        MidiNetworkMidiTransportTelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_INFO,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this"),
        TraceLoggingWideString(L"Enter", MIDI_TRACE_EVENT_MESSAGE_FIELD),
        TraceLoggingBoolean(m_sessionActive, "Session active")
    );

    // loop until we're told not to
    while (!m_shuttingDown && !stopToken.stop_requested()/* && m_sessionActive */)
    {
        // if no new outbound MIDI messages, and it has been longer than the
        // amount of time we currently have set for min midi message interval,
        // call function to send midi messages with an empty vector
        // then double the time for this type of message until we reach a
        // maximum interval

        // consider using std::condition_variable_any for the waits here
        // https://www.nextptr.com/tutorial/ta1588653702/stdjthread-and-cooperative-cancellation-with-stop-token
        // https://en.cppreference.com/w/cpp/thread/condition_variable_any

        // wait for the minimum transmit interval, or a signal that we have new outbound UMPs
        if (!stopToken.stop_requested() && !m_newMessagesInQueueEvent.is_signaled())
        {
            m_newMessagesInQueueEvent.wait(m_outgoingUmpEmptyPacketIntervalMilliseconds);
        }

        // we only send messages if there's an active session
        if (!stopToken.stop_requested() && m_sessionActive)
        {
            if (m_outgoingUmpMessages.empty())
            {
                // increase the empty packet interval until we get to the max interval value
                m_outgoingUmpEmptyPacketIntervalMilliseconds = min(m_outgoingUmpEmptyPacketIntervalMilliseconds + 200, m_outgoingUmpEmptyPacketMaxIntervalMilliseconds);
            }
            else
            {
                // reset the interval
                m_outgoingUmpEmptyPacketIntervalMilliseconds = m_outgoingUmpEmptyPacketStartingIntervalMilliseconds;
            }

            TraceLoggingWrite(
                MidiNetworkMidiTransportTelemetryProvider::Provider(),
                MIDI_TRACE_EVENT_INFO,
                TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
                TraceLoggingLevel(WINEVENT_LEVEL_INFO),
                TraceLoggingPointer(this, "this"),
                TraceLoggingWideString(L"Sending Message", MIDI_TRACE_EVENT_MESSAGE_FIELD)
            );

            LOG_IF_FAILED(SendQueuedMidiMessagesToNetwork());
        }

        // we're done processing, so reset the event for the next round
        if (!stopToken.stop_requested())
        {
            m_newMessagesInQueueEvent.ResetEvent();
        }
    }

    TraceLoggingWrite(
        MidiNetworkMidiTransportTelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_INFO,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this"),
        TraceLoggingWideString(L"Exit", MIDI_TRACE_EVENT_MESSAGE_FIELD),
        TraceLoggingBoolean(m_sessionActive, "Session active")
    );

    return S_OK;
}




HRESULT
MidiNetworkConnection::SendQueuedMidiMessagesToNetwork()
{
    TraceLoggingWrite(
        MidiNetworkMidiTransportTelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_INFO,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this"),
        TraceLoggingWideString(L"Enter", MIDI_TRACE_EVENT_MESSAGE_FIELD)
    );

    if (m_shuttingDown)
    {
        return S_OK;
    }

    auto queueLock = m_outgoingUmpMessageQueueLock.lock();
    auto lock = m_socketWriterLock.lock();

    constexpr size_t commandHeaderBytes{ sizeof(uint32_t) };

    HRESULT hr = S_OK;
    size_t position{ 0 };
    bool sentAtLeastOneDatagram{ false };

    // Fill datagrams with as many whole UMP messages as fit, and keep going until the queue is
    // drained. An empty queue still produces one datagram, which is the keep-alive.
    while (SUCCEEDED(hr) && !m_shuttingDown && (position < m_outgoingUmpMessages.size() || !sentAtLeastOneDatagram))
    {
        size_t budgetBytes{ MIDI_NETWORK_MAX_UDP_PAYLOAD_BYTES - sizeof(uint32_t) };   // less the UDP packet header

        // Forward error correction repeats the most recent packets, so when the budget is tight
        // we keep the newest and drop the oldest.
        std::vector<size_t> forwardErrorCorrectionIndexes;

        if (m_retransmitBuffer.size() > 0)
        {
            size_t maxCount = min(m_retransmitBuffer.size(), static_cast<size_t>(m_maxForwardErrorCorrectionCommandPacketCount));

            for (size_t i = 0; i < maxCount; i++)
            {
                size_t index = m_retransmitBuffer.size() - 1 - i;
                size_t cost = commandHeaderBytes + (m_retransmitBuffer.at(index).Words.size() * sizeof(uint32_t));

                if (cost > budgetBytes)
                {
                    break;
                }

                budgetBytes -= cost;
                forwardErrorCorrectionIndexes.push_back(index);
            }

            // the receiver processes in sequence order, so write oldest first
            std::reverse(forwardErrorCorrectionIndexes.begin(), forwardErrorCorrectionIndexes.end());
        }

        struct OutboundChunk
        {
            size_t Offset;
            size_t WordCount;
            MidiSequenceNumber SequenceNumber;
        };

        std::vector<OutboundChunk> chunks;
        auto nextSequenceNumber = m_lastSentUmpCommandSequenceNumber;

        while (position < m_outgoingUmpMessages.size() && budgetBytes > commandHeaderBytes)
        {
            size_t maxWordsForBudget = (budgetBytes - commandHeaderBytes) / sizeof(uint32_t);
            size_t maxWords = min(maxWordsForBudget, static_cast<size_t>(MIDI_MAX_UMP_WORDS_PER_PACKET));

            size_t wordCount = CalculateWholeUmpMessageWordCount(m_outgoingUmpMessages, position, maxWords);

            if (wordCount == 0)
            {
                // Either the next message needs a fresh datagram, or the tail of the queue is a
                // partial message we can never send. Only the latter can stall the loop.
                if (maxWords >= MIDI_MAX_UMP_WORDS_PER_PACKET)
                {
                    TraceLoggingWrite(
                        MidiNetworkMidiTransportTelemetryProvider::Provider(),
                        MIDI_TRACE_EVENT_WARNING,
                        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
                        TraceLoggingLevel(WINEVENT_LEVEL_WARNING),
                        TraceLoggingPointer(this, "this"),
                        TraceLoggingWideString(L"Incomplete UMP message at the end of the outbound queue. Discarding it.", MIDI_TRACE_EVENT_MESSAGE_FIELD),
                        TraceLoggingUInt64(static_cast<uint64_t>(m_outgoingUmpMessages.size() - position), "words discarded")
                    );

                    position = m_outgoingUmpMessages.size();
                }

                break;
            }

            nextSequenceNumber = nextSequenceNumber + 1;

            chunks.push_back({ position, wordCount, nextSequenceNumber });

            budgetBytes -= commandHeaderBytes + (wordCount * sizeof(uint32_t));
            position += wordCount;
        }

        if (chunks.empty())
        {
            if (sentAtLeastOneDatagram)
            {
                // nothing left that we can send
                break;
            }

            // keep-alive: a UMP Data command with no words, which still advances the sequence
            nextSequenceNumber = nextSequenceNumber + 1;
            chunks.push_back({ 0, 0, nextSequenceNumber });
        }

        hr = SendToNetwork([&](MidiNetworkDataWriter& writer)
            {
                for (auto const& index : forwardErrorCorrectionIndexes)
                {
                    auto const& entry = m_retransmitBuffer.at(index);

                    RETURN_IF_FAILED(writer.WriteCommandUmpMessages(entry.SequenceNumber, entry.Words.data(), static_cast<uint8_t>(entry.Words.size())));
                }

                for (auto const& chunk : chunks)
                {
                    RETURN_IF_FAILED(writer.WriteCommandUmpMessages(
                        chunk.SequenceNumber,
                        chunk.WordCount > 0 ? m_outgoingUmpMessages.data() + chunk.Offset : nullptr,
                        static_cast<uint8_t>(chunk.WordCount)));
                }

                return S_OK;
            });

        if (hr == S_OK)
        {
            // only committed once the datagram is actually on the wire
            for (auto const& chunk : chunks)
            {
                m_lastSentUmpCommandSequenceNumber = chunk.SequenceNumber;

                LOG_IF_FAILED(AddUmpPacketToRetransmitBuffer(
                    chunk.SequenceNumber,
                    chunk.WordCount > 0 ? m_outgoingUmpMessages.data() + chunk.Offset : nullptr,
                    chunk.WordCount));
            }
        }

        sentAtLeastOneDatagram = true;
    }

    // The queue is drained either way. Holding on to messages we could not send would let an
    // unreachable remote grow it without bound.
    m_outgoingUmpMessages.clear();

    TraceLoggingWrite(
        MidiNetworkMidiTransportTelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_INFO,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this"),
        TraceLoggingWideString(L"Exit", MIDI_TRACE_EVENT_MESSAGE_FIELD)
    );

    return hr;
}


_Use_decl_annotations_
HRESULT
MidiNetworkConnection::QueueMidiMessagesToSendToNetwork(
    std::vector<uint32_t> const& words)
{
    TraceLoggingWrite(
        MidiNetworkMidiTransportTelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_INFO,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this"),
        TraceLoggingWideString(L"Enter", MIDI_TRACE_EVENT_MESSAGE_FIELD),
        TraceLoggingUInt32(static_cast<uint32_t>(words.size()), "Word count")
    );

    if (!m_sessionActive)
    {
        return S_OK;
    }

    auto lock = m_outgoingUmpMessageQueueLock.lock();

    m_outgoingUmpMessages.insert(m_outgoingUmpMessages.end(), words.begin(), words.end());

    lock.reset();

    // wakeup sender thread
    m_newMessagesInQueueEvent.SetEvent();

    TraceLoggingWrite(
        MidiNetworkMidiTransportTelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_INFO,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this"),
        TraceLoggingWideString(L"Exit", MIDI_TRACE_EVENT_MESSAGE_FIELD)
    );

    return S_OK;
}



_Use_decl_annotations_
HRESULT
MidiNetworkConnection::QueueMidiMessagesToSendToNetwork(
    PVOID const bytes,
    UINT const byteCount)
{
    TraceLoggingWrite(
        MidiNetworkMidiTransportTelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_INFO,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this"),
        TraceLoggingWideString(L"Enter", MIDI_TRACE_EVENT_MESSAGE_FIELD),
        TraceLoggingUInt32(byteCount, "Byte count")
    );

    RETURN_HR_IF_NULL(E_INVALIDARG, bytes);
    RETURN_HR_IF(E_INVALIDARG, byteCount < sizeof(uint32_t));

    if (!m_sessionActive)
    {
        return S_OK;
    }

    std::vector<uint32_t> words{ };
    uint32_t* wordPointer{ static_cast<uint32_t*>(bytes) };
    size_t wordCount{ byteCount / sizeof(uint32_t) };

    words.insert(words.end(), wordPointer, wordPointer + wordCount);

    // TODO: Can optimize this to not create the temporary vector and instead
    // insert directly into m_outgoingUmpMessages. Duplicates some code.

    return QueueMidiMessagesToSendToNetwork(words);
}



HRESULT
MidiNetworkConnection::Shutdown()
{
    TraceLoggingWrite(
        MidiNetworkMidiTransportTelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_INFO,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this"),
        TraceLoggingWideString(L"Enter", MIDI_TRACE_EVENT_MESSAGE_FIELD)
    );


    if (m_shuttingDown.exchange(true))
    {
        // already shut down. This is called from both the owning host/client and from
        // TransportState cleanup, so it has to be safe to repeat.
        return S_OK;
    }

    // release a user-initiated disconnect which is mid-retry
    m_byeReplyEvent.SetEvent();

    // don't process any more MIDI UMP messages
    auto callback = DetachCallback();
    callback.reset();

    // say bye while the writer is still up
    LOG_IF_FAILED(SendShutdownBye());

    // cleanup. Does not tear down the writer.
    LOG_IF_FAILED(EndActiveSession(false));

    // Both workers must be stopped before anything they touch is released.
    LOG_IF_FAILED(StopAndJoinWorkerThreads());

    {
        auto lock = m_socketWriterLock.lock();

        m_retransmitBuffer.clear();

        if (m_writer != nullptr)
        {
            LOG_IF_FAILED(m_writer->Shutdown());
            m_writer.reset();
        }
    }

    {
        auto queueLock = m_outgoingUmpMessageQueueLock.lock();
        m_outgoingUmpMessages.clear();
    }

    TraceLoggingWrite(
        MidiNetworkMidiTransportTelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_INFO,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this"),
        TraceLoggingWideString(L"Exit", MIDI_TRACE_EVENT_MESSAGE_FIELD)
    );

    return S_OK;
}
