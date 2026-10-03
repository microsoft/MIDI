// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://github.com/microsoft/MIDI/
// ============================================================================

#include "pch.h"


_Use_decl_annotations_
HRESULT
MidiNetworkClientConnection::Initialize(
    winrt::guid const& configIdentifier,
    winrt::Windows::Networking::Sockets::DatagramSocket const& socket,
    winrt::Windows::Networking::HostName const& remoteHostHostName,
    winrt::hstring const& remotePort,
    std::wstring const& thisEndpointName,
    std::wstring const& thisProductInstanceId,
    uint16_t const retransmitBufferMaxCommandPacketCount,
    uint8_t const maxForwardErrorCorrectionCommandPacketCount,
    bool createUmpEndpointsOnly,
    uint8_t const fallbackMidi1PortCount
)
{
    return MidiNetworkConnection::Initialize(
        MidiNetworkConnectionRole::ConnectionWindowsIsClient,
        configIdentifier,
        TRANSPORT_CLIENT_PARENT_ID,
        socket,
        remoteHostHostName,
        remotePort,
        winrt::Windows::Networking::HostName{ nullptr },
        thisEndpointName,
        thisProductInstanceId,
        retransmitBufferMaxCommandPacketCount,
        maxForwardErrorCorrectionCommandPacketCount,
        createUmpEndpointsOnly,
        fallbackMidi1PortCount
    );
}

HRESULT
MidiNetworkClientConnection::SendInvitation()
{
    TraceLoggingWrite(
        MidiNetworkMidiTransportTelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_INFO,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this"),
        TraceLoggingWideString(L"Enter", MIDI_TRACE_EVENT_MESSAGE_FIELD)
    );

    m_invitation.Begin();

    RETURN_IF_FAILED(SendInvitationCommand());

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
MidiNetworkClientConnection::SendInvitationCommand()
{
    RETURN_IF_FAILED(SendToNetwork([this](MidiNetworkDataWriter& writer)
        {
            // Authentication support would be advertised in these capability bits. See MidiNetworkCredentials.h.
            RETURN_IF_FAILED(writer.WriteCommandInvitation(MidiNetworkCommandInvitationCapabilities::Capabilities_None, m_thisEndpointName, m_thisProductInstanceId));

            return S_OK;
        }));

    return S_OK;
}

// Driven by the watchdog tick. The rules live in MidiNetworkInvitationState; this turns the
// action it returns into network traffic.
HRESULT
MidiNetworkClientConnection::OnWatchdogTick()
{
    uint64_t elapsedMilliseconds{ 0 };

    if (m_invitation.ReplyPendingReceived())
    {
        elapsedMilliseconds = internal::ConvertTimestampToWholeMilliseconds(
            internal::GetCurrentMidiTimestamp() - m_invitation.ReplyPendingTimestamp(),
            internal::GetMidiTimestampFrequency());
    }

    auto const action = m_invitation.Tick(
        m_sessionActive,
        elapsedMilliseconds,
        TransportState::Current().TransportSettings.InvitationPendingTimeout,
        MIDI_NETWORK_MAX_INVITATION_ATTEMPTS);

    switch (action)
    {
    case MidiNetworkInvitationAction::None:
        return S_OK;

    case MidiNetworkInvitationAction::SendInvitation:
        LOG_IF_FAILED(SendInvitationCommand());
        return S_OK;

    case MidiNetworkInvitationAction::CancelNotApproved:
        TraceLoggingWrite(
            MidiNetworkMidiTransportTelemetryProvider::Provider(),
            MIDI_TRACE_EVENT_WARNING,
            TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
            TraceLoggingLevel(WINEVENT_LEVEL_WARNING),
            TraceLoggingPointer(this, "this"),
            TraceLoggingWideString(L"Invitation was never approved by the remote host. Canceling.", MIDI_TRACE_EVENT_MESSAGE_FIELD),
            TraceLoggingWideString(m_remoteHostName != nullptr ? m_remoteHostName.ToString().c_str() : L"", "remote hostname"),
            TraceLoggingWideString(m_remotePort.c_str(), "remote port"),
            TraceLoggingUInt64(elapsedMilliseconds, "elapsed milliseconds")
        );

        LOG_IF_FAILED(SendToNetwork([](MidiNetworkDataWriter& writer)
            {
                RETURN_IF_FAILED(writer.WriteCommandBye(
                    MidiNetworkCommandByeReason::CommandByeReasonClientToHost_InvitationCanceled,
                    internal::ResourceGetWString(IDS_ERROR_INVITATION_NOT_APPROVED).c_str()));

                return S_OK;
            }));

        // Nobody decided, which is treated like no answer at all
        if (!m_shuttingDown)
        {
            LOG_IF_FAILED(TransportState::Current().MarkClientDefinitionUnavailableOrRetry(m_configIdentifier));
        }

        return S_OK;

    case MidiNetworkInvitationAction::CancelNoReply:
        TraceLoggingWrite(
            MidiNetworkMidiTransportTelemetryProvider::Provider(),
            MIDI_TRACE_EVENT_WARNING,
            TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
            TraceLoggingLevel(WINEVENT_LEVEL_WARNING),
            TraceLoggingPointer(this, "this"),
            TraceLoggingWideString(L"Remote host never answered our invitation. Canceling.", MIDI_TRACE_EVENT_MESSAGE_FIELD),
            TraceLoggingWideString(m_remoteHostName != nullptr ? m_remoteHostName.ToString().c_str() : L"", "remote hostname"),
            TraceLoggingWideString(m_remotePort.c_str(), "remote port")
        );

        LOG_IF_FAILED(SendToNetwork([](MidiNetworkDataWriter& writer)
            {
                RETURN_IF_FAILED(writer.WriteCommandBye(
                    MidiNetworkCommandByeReason::CommandByeReasonClientToHost_InvitationCanceled,
                    internal::ResourceGetWString(IDS_ERROR_NO_REPLY_TO_INVITATION).c_str()));

                return S_OK;
            }));

        // The host may simply not be switched on yet. An advertised host is picked up again when
        // it advertises, and a host name is tried again after the scan interval, because it is
        // looked up each time. An IP address is parked until the app asks for it again, because
        // nothing announces its return and every configured dead address would be retried.
        if (!m_shuttingDown)
        {
            LOG_IF_FAILED(TransportState::Current().MarkClientDefinitionUnavailableOrRetry(m_configIdentifier));
        }

        return S_OK;
    }

    return S_OK;
}

void
MidiNetworkClientConnection::OnSessionEndedByRemote()
{
    LOG_IF_FAILED(RequestReconnect());
}

_Use_decl_annotations_
void
MidiNetworkClientConnection::OnByeReceived(MidiNetworkCommandByeReason const reason) noexcept
{
    // Only a Bye which ends our own invitation says anything about trying again
    if (!m_invitation.Answered() || m_shuttingDown)
    {
        return;
    }

    try
    {
        TraceLoggingWrite(
            MidiNetworkMidiTransportTelemetryProvider::Provider(),
            MIDI_TRACE_EVENT_WARNING,
            TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
            TraceLoggingLevel(WINEVENT_LEVEL_WARNING),
            TraceLoggingPointer(this, "this"),
            TraceLoggingWideString(L"Remote host refused the invitation with a Bye.", MIDI_TRACE_EVENT_MESSAGE_FIELD),
            TraceLoggingUInt8(reason, "bye reason"),
            TraceLoggingGuid(m_configIdentifier, "entry identifier")
        );

        switch (reason)
        {
        case MidiNetworkCommandByeReason::CommandByeReasonHostToClient_TooManyOpenSessions:
            LOG_IF_FAILED(TransportState::Current().MarkClientDefinitionForRetryAfter(
                m_configIdentifier,
                MIDI_NETWORK_CLIENT_BUSY_RETRY_DELAY_MILLISECONDS));
            break;

        // The host's owner said no, or it wants authentication this client cannot give
        case MidiNetworkCommandByeReason::CommandByeReasonHostToClient_InvitationWithAuthRejectedMissingPriorAttempt:
        case MidiNetworkCommandByeReason::CommandByeReasonHostToClient_InvitationRejectedUserDidNotAccept:
        case MidiNetworkCommandByeReason::CommandByeReasonHostToClient_InvitationRejectedAuthFailed:
        case MidiNetworkCommandByeReason::CommandByeReasonHostToClient_InvitationRejectedUsernameNotFound:
        case MidiNetworkCommandByeReason::CommandByeReasonHostToClient_NoMatchingAuthenticationMethod:
            LOG_IF_FAILED(TransportState::Current().MarkClientDefinitionFailed(m_configIdentifier));
            break;

        default:
            LOG_IF_FAILED(TransportState::Current().MarkClientDefinitionUnavailableOrRetry(m_configIdentifier));
            break;
        }
    }
    CATCH_LOG();
}

HRESULT
MidiNetworkClientConnection::RequestReconnect()
{
    if (m_shuttingDown)
    {
        return S_FALSE;
    }

    auto markResult = TransportState::Current().MarkClientDefinitionForReconnect(m_configIdentifier);

    if (markResult != S_OK)
    {
        // no definition, or it was disabled, so nothing should be rebuilt
        return S_FALSE;
    }

    TraceLoggingWrite(
        MidiNetworkMidiTransportTelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_INFO,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this"),
        TraceLoggingWideString(L"Session to the remote host ended. Queued for reconnect.", MIDI_TRACE_EVENT_MESSAGE_FIELD),
        TraceLoggingGuid(m_configIdentifier, "entry identifier")
    );

    auto endpointManager = TransportState::Current().GetEndpointManager();

    if (endpointManager != nullptr)
    {
        LOG_IF_FAILED(endpointManager->WakeupBackgroundEndpointCreatorThread());
    }

    return S_OK;
}

_Use_decl_annotations_
HRESULT
MidiNetworkClientConnection::HandleIncomingInvitationReplyAccepted(
    MidiNetworkCommandPacketHeader const& header,
    std::wstring const& remoteHostUmpEndpointName,
    std::wstring const& remoteHostProductInstanceId
)
{
    UNREFERENCED_PARAMETER(header);

    TraceLoggingWrite(
        MidiNetworkMidiTransportTelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_INFO,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this"),
        TraceLoggingWideString(L"Enter", MIDI_TRACE_EVENT_MESSAGE_FIELD)
    );

    // the host answered, so stop repeating the invitation
    m_invitation.Answered();

    // Captured once. It can be torn down while a datagram is in flight, and each call to
    // GetEndpointManager() is a fresh read.
    auto endpointManager = TransportState::Current().GetEndpointManager();

    RETURN_HR_IF_NULL(S_FALSE, endpointManager);

    if (!endpointManager->IsInitialized())
    {
        return S_FALSE;
    }

    {
        auto lock = m_sessionLock.lock();

        // per protocol, if we've already accepted this, then just ignore it
        if (m_sessionActive)
        {
            return S_OK;
        }

        // A user disconnect can land between our invitation going out and this reply arriving.
        // Nothing would own an endpoint built now.
        if (m_shuttingDown)
        {
            TraceLoggingWrite(
                MidiNetworkMidiTransportTelemetryProvider::Provider(),
                MIDI_TRACE_EVENT_INFO,
                TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
                TraceLoggingLevel(WINEVENT_LEVEL_INFO),
                TraceLoggingPointer(this, "this"),
                TraceLoggingWideString(L"Invitation was answered after this connection was shut down. Not creating an endpoint.", MIDI_TRACE_EVENT_MESSAGE_FIELD),
                TraceLoggingGuid(m_configIdentifier, "entry identifier")
            );

            return S_OK;
        }

        // Active now, not once the endpoint exists, or the host's first UMP Data would be
        // answered with Bye Session Not Established while the worker builds the endpoint.
        m_sessionActive = true;
        m_sessionEverEstablished = true;
    }

    // Creating the endpoint blocks on the service, and this is the socket receive callback
    auto queueHr = endpointManager->QueueClientEndpointCreation(
        std::static_pointer_cast<MidiNetworkClientConnection>(shared_from_this()),
        remoteHostUmpEndpointName,
        remoteHostProductInstanceId);

    if (FAILED(queueHr))
    {
        LOG_IF_FAILED(FailClientSessionEndpointCreation(queueHr));

        RETURN_IF_FAILED(queueHr);
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
MidiNetworkClientConnection::CreateClientEndpointForAcceptedInvitation(
    std::wstring const& remoteHostUmpEndpointName,
    std::wstring const& remoteHostProductInstanceId,
    std::wstring& newDeviceInstanceId,
    std::wstring& newEndpointDeviceInterfaceId
)
{
    auto endpointManager = TransportState::Current().GetEndpointManager();

    RETURN_HR_IF_NULL(E_UNEXPECTED, endpointManager);

    RETURN_IF_FAILED(endpointManager->CreateNewClientEndpointToRemoteHost(
        internal::GuidToString(m_configIdentifier),
        remoteHostUmpEndpointName,
        remoteHostProductInstanceId,
        m_remoteHostName,
        m_remotePort,
        m_createUmpEndpointsOnly,
        m_fallbackMidi1PortCount,
        newDeviceInstanceId,
        newEndpointDeviceInterfaceId));

    return S_OK;
}

_Use_decl_annotations_
HRESULT
MidiNetworkClientConnection::CompleteClientSessionAfterEndpointCreated(
    std::wstring const& newDeviceInstanceId,
    std::wstring const& newEndpointDeviceInterfaceId
)
{
    auto const deviceInstanceId = internal::NormalizeDeviceInstanceIdWStringCopy(newDeviceInstanceId);
    auto const endpointDeviceInterfaceId = internal::NormalizeEndpointInterfaceIdWStringCopy(newEndpointDeviceInterfaceId);

    auto endpointManager = TransportState::Current().GetEndpointManager();

    RETURN_HR_IF_NULL(E_UNEXPECTED, endpointManager);

    HRESULT hr{ S_OK };
    bool claimed{ false };

    {
        auto lock = m_sessionLock.lock();

        // The host said Bye, the session timed out, or this connection is being shut down
        if (m_sessionActive && !m_shuttingDown)
        {
            // this is what the Bidi uses when it is created
            hr = TransportState::Current().AssociateMidiEndpointWithConnection(endpointDeviceInterfaceId, m_remoteHostName, m_remotePort.c_str());

            if (SUCCEEDED(hr))
            {
                m_sessionDeviceInstanceId = deviceInstanceId;
                m_sessionEndpointDeviceInterfaceId = endpointDeviceInterfaceId;

                claimed = true;
            }
        }
    }

    if (!claimed)
    {
        TraceLoggingWrite(
            MidiNetworkMidiTransportTelemetryProvider::Provider(),
            MIDI_TRACE_EVENT_INFO,
            TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
            TraceLoggingLevel(WINEVENT_LEVEL_INFO),
            TraceLoggingPointer(this, "this"),
            TraceLoggingWideString(L"The session ended while its endpoint was being created. Removing it.", MIDI_TRACE_EVENT_MESSAGE_FIELD),
            TraceLoggingGuid(m_configIdentifier, "entry identifier"),
            TraceLoggingHResult(hr, MIDI_TRACE_EVENT_HRESULT_FIELD)
        );

        // Nothing else knows this endpoint exists. This is the endpoint worker, so it goes now.
        LOG_IF_FAILED(endpointManager->DeleteEndpoint(deviceInstanceId));

        if (FAILED(hr))
        {
            LOG_IF_FAILED(FailClientSessionEndpointCreation(hr));

            RETURN_IF_FAILED(hr);
        }

        return S_FALSE;
    }

    hr = StartOutboundMidiMessageProcessingThread();

    if (FAILED(hr))
    {
        // Ending the session removes the endpoint claimed above
        LOG_IF_FAILED(FailClientSessionEndpointCreation(hr));

        RETURN_IF_FAILED(hr);
    }

    TraceLoggingWrite(
        MidiNetworkMidiTransportTelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_INFO,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this"),
        TraceLoggingWideString(L"Created MIDI endpoint", MIDI_TRACE_EVENT_MESSAGE_FIELD),
        TraceLoggingWideString(endpointDeviceInterfaceId.c_str(), "endpoint device interface id")
    );

    // negotiation needs the connection wired up first, so it cannot happen during creation
    LOG_IF_FAILED(endpointManager->QueueDiscoveryAndNegotiation(endpointDeviceInterfaceId));

    return S_OK;
}

_Use_decl_annotations_
HRESULT
MidiNetworkClientConnection::FailClientSessionEndpointCreation(HRESULT const failure)
{
    if (m_shuttingDown)
    {
        return S_FALSE;
    }

    TraceLoggingWrite(
        MidiNetworkMidiTransportTelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_ERROR,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_ERROR),
        TraceLoggingPointer(this, "this"),
        TraceLoggingWideString(L"Failed to create MIDI endpoint.", MIDI_TRACE_EVENT_MESSAGE_FIELD),
        TraceLoggingHResult(failure, MIDI_TRACE_EVENT_HRESULT_FIELD)
    );

    // let the other side know that we can't create the session
    LOG_IF_FAILED(RefuseSessionForEndpointCreationFailure(failure));

    // The session went active when the host accepted. Ending it here is not the remote leaving,
    // so it is not reconnected.
    return EndActiveSession(false);
}

_Use_decl_annotations_
HRESULT
MidiNetworkClientConnection::HandleIncomingInvitationReplyAuthenticationRequired(
    MidiNetworkCommandPacketHeader const& header)
{
    TraceLoggingWrite(
        MidiNetworkMidiTransportTelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_WARNING,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_WARNING),
        TraceLoggingPointer(this, "this"),
        TraceLoggingWideString(L"Remote host requires authentication, which is not yet implemented. Canceling the invitation.", MIDI_TRACE_EVENT_MESSAGE_FIELD),
        TraceLoggingUInt8(header.HeaderData.CommandCode, "Command Code")
    );

    // Only the first answer counts. The host answers every copy of the invitation that was
    // already on its way, and a session or a timeout may have ended the invitation first.
    if (!m_invitation.Answered())
    {
        return S_OK;
    }

    // A client which supports authentication answers the challenge here instead of withdrawing.
    // See MidiNetworkCredentials.h.
    //
    // Spec 6.4: a client which gives up on its invitation ends it with Bye 0x80 Invitation
    // Canceled. The spec has no client reason for "authentication not supported". A host which
    // follows it does not ask this client at all, because the invitation offers no
    // authentication method. It sends Bye 0x45 No Matching Authentication Method instead, which
    // OnByeReceived turns into the same failed entry.
    LOG_IF_FAILED(RefuseInvitationForAuthentication(MidiNetworkCommandByeReason::CommandByeReasonClientToHost_InvitationCanceled));

    // Asking again gets the same answer
    if (!m_shuttingDown)
    {
        LOG_IF_FAILED(TransportState::Current().MarkClientDefinitionFailed(m_configIdentifier));
    }

    return S_OK;
}

HRESULT
MidiNetworkClientConnection::HandleIncomingInvitationReplyPending()
{
    // Spec 6.8. The host is telling us it needs more time, typically because a person has to
    // approve the connection. Re-inviting now would just make it ask again, so the retry loop
    // stops here and we wait for Accepted or Bye.
    bool const firstPendingReply = m_invitation.NoteReplyPending(internal::GetCurrentMidiTimestamp());

    TraceLoggingWrite(
        MidiNetworkMidiTransportTelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_INFO,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this"),
        TraceLoggingWideString(L"Remote host accepted the invitation as pending. Waiting for it to be approved.", MIDI_TRACE_EVENT_MESSAGE_FIELD),
        TraceLoggingBoolean(!firstPendingReply, "repeat pending reply")
    );

    return S_OK;
}
