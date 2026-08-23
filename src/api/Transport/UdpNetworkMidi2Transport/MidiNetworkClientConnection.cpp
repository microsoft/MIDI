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
    bool createUmpEndpointsOnly
)
{
    return MidiNetworkConnection::Initialize(
        MidiNetworkConnectionRole::ConnectionWindowsIsClient,
        configIdentifier,
        TRANSPORT_CLIENT_PARENT_ID,
        socket,
        remoteHostHostName,
        remotePort,
        thisEndpointName,
        thisProductInstanceId,
        retransmitBufferMaxCommandPacketCount,
        maxForwardErrorCorrectionCommandPacketCount,
        createUmpEndpointsOnly
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
            // TODO: When we support authentication, advertise it in the capabilities bitmap
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
            TraceLoggingWideString(L"Invitation was never approved by the remote host. Cancelling.", MIDI_TRACE_EVENT_MESSAGE_FIELD),
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

        return S_OK;

    case MidiNetworkInvitationAction::CancelNoReply:
        TraceLoggingWrite(
            MidiNetworkMidiTransportTelemetryProvider::Provider(),
            MIDI_TRACE_EVENT_WARNING,
            TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
            TraceLoggingLevel(WINEVENT_LEVEL_WARNING),
            TraceLoggingPointer(this, "this"),
            TraceLoggingWideString(L"Remote host never answered our invitation. Cancelling.", MIDI_TRACE_EVENT_MESSAGE_FIELD),
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
        // it advertises; a direct address is parked until the app asks for it again, because
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

    if (m_sessionActive)
    {
        // per protocol, if we've already accepted this, then just ignore it
        return S_OK;
    }

    // TODO: will we accept a session invitation from the specified hostname?
    // TODO: Also need to check auth mechanism and follow instructions in 6.4 and send a Bye if not supported

    // todo: see if we already have a session active for this remote. If so, use it.
    // otherwise, we need to spin up a new session

    std::wstring newDeviceInstanceId{ };
    std::wstring newEndpointDeviceInterfaceId{ };

    // A user disconnect can land between our invitation going out and this reply arriving, and
    // this runs on the socket receive path rather than through the endpoint creator, so nothing
    // upstream has already vetted it. Building the endpoint now would leave a device node no
    // caller owns: the connection is already torn down, so nothing will ever delete it. The host
    // role has the same protection via OnSessionEndedBeforeEndpointCreated.
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

    // Captured once. It can be torn down while a datagram is in flight, and each call to
    // GetEndpointManager() is a fresh read, so re-reading it per use is a null deref waiting
    // to happen.
    auto endpointManager = TransportState::Current().GetEndpointManager();

    RETURN_HR_IF_NULL(S_FALSE, endpointManager);

    if (endpointManager->IsInitialized())
    {
        // Create the endpoint for Windows MIDI Services clients
        HRESULT hr = S_OK;

        hr = endpointManager->CreateNewClientEndpointToRemoteHost(
            internal::GuidToString(m_configIdentifier),
            remoteHostUmpEndpointName,
            remoteHostProductInstanceId,
            m_remoteHostName,
            m_remotePort,
            m_createUmpEndpointsOnly,
            newDeviceInstanceId,
            newEndpointDeviceInterfaceId
        );

        // Creation takes a noticeable amount of time, so the disconnect can also arrive while it
        // is running. Shutdown already deleted whatever it knew about, which at that point was
        // nothing, so this one has to be cleaned up here.
        if (SUCCEEDED(hr) && m_shuttingDown)
        {
            TraceLoggingWrite(
                MidiNetworkMidiTransportTelemetryProvider::Provider(),
                MIDI_TRACE_EVENT_INFO,
                TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
                TraceLoggingLevel(WINEVENT_LEVEL_INFO),
                TraceLoggingPointer(this, "this"),
                TraceLoggingWideString(L"This connection was shut down while its endpoint was being created. Removing it.", MIDI_TRACE_EVENT_MESSAGE_FIELD),
                TraceLoggingGuid(m_configIdentifier, "entry identifier")
            );

            LOG_IF_FAILED(endpointManager->DeleteEndpoint(internal::NormalizeDeviceInstanceIdWStringCopy(newDeviceInstanceId)));

            return S_OK;
        }

        if (SUCCEEDED(hr))
        {
            TraceLoggingWrite(
                MidiNetworkMidiTransportTelemetryProvider::Provider(),
                MIDI_TRACE_EVENT_INFO,
                TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
                TraceLoggingLevel(WINEVENT_LEVEL_INFO),
                TraceLoggingPointer(this, "this"),
                TraceLoggingWideString(L"Created MIDI endpoint", MIDI_TRACE_EVENT_MESSAGE_FIELD)
            );

            m_sessionEndpointDeviceInterfaceId = internal::NormalizeEndpointInterfaceIdWStringCopy(newEndpointDeviceInterfaceId);
            m_sessionDeviceInstanceId = internal::NormalizeDeviceInstanceIdWStringCopy(newDeviceInstanceId);

            m_sessionActive = true;
            m_sessionEverEstablished = true;

            // this is what the Bidi uses when it is created
            RETURN_IF_FAILED(TransportState::Current().AssociateMidiEndpointWithConnection(m_sessionEndpointDeviceInterfaceId.c_str(), m_remoteHostName, m_remotePort.c_str()));

            RETURN_IF_FAILED(StartOutboundMidiMessageProcessingThread());

            // protocol negotiation needs to happen here, not in the endpoint creation
            // because we need to wire up the connection first. Bit of a race.

            LOG_IF_FAILED(endpointManager->QueueDiscoveryAndNegotiation(m_sessionEndpointDeviceInterfaceId));
        }
        else
        {
            TraceLoggingWrite(
                MidiNetworkMidiTransportTelemetryProvider::Provider(),
                MIDI_TRACE_EVENT_ERROR,
                TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
                TraceLoggingLevel(WINEVENT_LEVEL_ERROR),
                TraceLoggingPointer(this, "this"),
                TraceLoggingWideString(L"Failed to create MIDI endpoint.", MIDI_TRACE_EVENT_MESSAGE_FIELD),
                TraceLoggingHResult(hr, "hresult")
            );

            // let the other side know that we can't create the session

            LOG_IF_FAILED(RefuseSessionForEndpointCreationFailure(hr));

            // exit out of here, and log while we're at it
            RETURN_IF_FAILED(hr);
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
MidiNetworkClientConnection::HandleIncomingInvitationReplyAuthenticationRequired(
    MidiNetworkCommandPacketHeader const& header,
    MidiNetworkAuthenticationKind const kind)
{
    UNREFERENCED_PARAMETER(header);

    TraceLoggingWrite(
        MidiNetworkMidiTransportTelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_WARNING,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_WARNING),
        TraceLoggingPointer(this, "this"),
        TraceLoggingWideString(L"Remote host requires authentication, which is not yet implemented. Cancelling the invitation.", MIDI_TRACE_EVENT_MESSAGE_FIELD),
        TraceLoggingUInt32(static_cast<uint32_t>(kind), "authentication kind")
    );

    // The host is challenging us. Once this is implemented the sequence is: resolve the secret
    // for the configured credential identifier, compute the digest over the supplied nonce, and
    // reply with InvitationWithAuthentication. Until then we withdraw politely rather than time
    // out.
    // TODO: https://github.com/microsoft/MIDI/issues/733
    return RefuseInvitationForAuthentication(MidiNetworkCommandByeReason::CommandByeReasonClientToHost_InvitationCanceled);
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
