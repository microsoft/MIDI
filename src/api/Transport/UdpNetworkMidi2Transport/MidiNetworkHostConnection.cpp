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
MidiNetworkHostConnection::Initialize(
    winrt::guid const& configIdentifier,
    std::wstring const& hostParentInstanceId,
    winrt::Windows::Networking::Sockets::DatagramSocket const& socket,
    winrt::Windows::Networking::HostName const& remoteClientHostName,
    winrt::hstring const& remotePort,
    std::wstring const& thisEndpointName,
    std::wstring const& thisProductInstanceId,
    uint16_t const retransmitBufferMaxCommandPacketCount,
    uint8_t const maxForwardErrorCorrectionCommandPacketCount,
    bool createUmpEndpointsOnly,
    MidiNetworkAuthenticationKind const authenticationKind,
    MidiNetworkCredentialIdentifier const& credentialIdentifier
)
{
    m_authenticationKind = authenticationKind;
    m_credentialIdentifier = credentialIdentifier;

    return MidiNetworkConnection::Initialize(
        MidiNetworkConnectionRole::ConnectionWindowsIsHost,
        configIdentifier,
        hostParentInstanceId,
        socket,
        remoteClientHostName,
        remotePort,
        thisEndpointName,
        thisProductInstanceId,
        retransmitBufferMaxCommandPacketCount,
        maxForwardErrorCorrectionCommandPacketCount,
        createUmpEndpointsOnly
    );
}

_Use_decl_annotations_
HRESULT
MidiNetworkHostConnection::CreateHostEndpointForPendingInvitation(
    std::wstring const& clientUmpEndpointName,
    std::wstring const& clientProductInstanceId,
    std::wstring& newDeviceInstanceId,
    std::wstring& newEndpointDeviceInterfaceId
)
{
    auto endpointManager = TransportState::Current().GetEndpointManager();

    RETURN_HR_IF_NULL(E_UNEXPECTED, endpointManager);

    RETURN_IF_FAILED(endpointManager->CreateNewHostEndpointToRemoteClient(
        internal::GuidToString(m_configIdentifier),
        m_parentDeviceInstanceId,
        clientUmpEndpointName,
        clientProductInstanceId,
        m_remoteHostName,
        m_remotePort,
        m_createUmpEndpointsOnly,
        newDeviceInstanceId,
        newEndpointDeviceInterfaceId));

    return S_OK;
}

_Use_decl_annotations_
HRESULT
MidiNetworkHostConnection::CompleteHostSessionAfterEndpointCreated(
    std::wstring const& newDeviceInstanceId,
    std::wstring const& newEndpointDeviceInterfaceId
)
{
    m_hostEndpointCreationPending = false;

    if (m_shuttingDown)
    {
        return S_FALSE;
    }

    m_sessionEndpointDeviceInterfaceId = internal::NormalizeEndpointInterfaceIdWStringCopy(newEndpointDeviceInterfaceId);
    m_sessionDeviceInstanceId = internal::NormalizeDeviceInstanceIdWStringCopy(newDeviceInstanceId);

    // this is what the Bidi uses when it is created
    RETURN_IF_FAILED(TransportState::Current().AssociateMidiEndpointWithConnection(m_sessionEndpointDeviceInterfaceId.c_str(), m_remoteHostName, m_remotePort.c_str()));

    RETURN_IF_FAILED(StartOutboundMidiMessageProcessingThread());

    auto endpointManager = TransportState::Current().GetEndpointManager();

    if (endpointManager != nullptr)
    {
        // negotiation needs the connection wired up first, so it cannot happen during creation
        LOG_IF_FAILED(endpointManager->QueueDiscoveryAndNegotiation(m_sessionEndpointDeviceInterfaceId));
    }

    RETURN_IF_FAILED(SendToNetwork([this](MidiNetworkDataWriter& writer)
        {
            RETURN_IF_FAILED(writer.WriteCommandInvitationReplyAccepted(m_thisEndpointName, m_thisProductInstanceId));

            return S_OK;
        }));

    m_sessionActive = true;
    m_sessionEverEstablished = true;

    TraceLoggingWrite(
        MidiNetworkMidiTransportTelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_INFO,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this"),
        TraceLoggingWideString(L"Session accepted after deferred endpoint creation", MIDI_TRACE_EVENT_MESSAGE_FIELD),
        TraceLoggingWideString(m_sessionEndpointDeviceInterfaceId.c_str(), "endpoint device interface id")
    );

    return S_OK;
}

_Use_decl_annotations_
HRESULT
MidiNetworkHostConnection::FailHostSessionEndpointCreation(HRESULT const failure)
{
    m_hostEndpointCreationPending = false;

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
        TraceLoggingWideString(L"Endpoint could not be created for a pending invitation", MIDI_TRACE_EVENT_MESSAGE_FIELD),
        TraceLoggingHResult(failure, MIDI_TRACE_EVENT_HRESULT_FIELD)
    );

    // Spec 6.6: a Pending reply is followed by an Accepted or a Bye. This is the Bye.
    LOG_IF_FAILED(RefuseSessionForEndpointCreationFailure(failure));

    return S_OK;
}

HRESULT
MidiNetworkHostConnection::ApproveByUser()
{
    // Only a remote we actually parked is resumable. Anything else means the approval raced a
    // Bye or a second approval, and there is nothing left to resume.
    if (!m_awaitingUserApproval.exchange(false))
    {
        return S_FALSE;
    }

    auto identity = GetRemoteClientIdentity();

    TraceLoggingWrite(
        MidiNetworkMidiTransportTelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_INFO,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this"),
        TraceLoggingWideString(L"A user approved this remote client.", MIDI_TRACE_EVENT_MESSAGE_FIELD),
        TraceLoggingWideString(identity.UmpEndpointName.c_str(), "client endpoint name"),
        TraceLoggingWideString(identity.ProductInstanceId.c_str(), "client product instance id")
    );

    auto endpointManager = TransportState::Current().GetEndpointManager();

    RETURN_HR_IF_NULL(E_UNEXPECTED, endpointManager);

    // Picks up exactly where the approval gate stopped: the client has already had its Pending
    // reply, so all that is left is the endpoint and the Accepted which follows it.
    if (m_hostEndpointCreationPending.exchange(true))
    {
        return S_FALSE;
    }

    auto queueHr = endpointManager->QueueHostEndpointCreation(
        std::static_pointer_cast<MidiNetworkHostConnection>(shared_from_this()),
        identity.UmpEndpointName,
        identity.ProductInstanceId);

    if (FAILED(queueHr))
    {
        m_hostEndpointCreationPending = false;

        LOG_IF_FAILED(RefuseSessionForEndpointCreationFailure(queueHr));

        RETURN_IF_FAILED(queueHr);
    }

    return S_OK;
}

HRESULT
MidiNetworkHostConnection::DenyByUser()
{
    m_awaitingUserApproval = false;

    TraceLoggingWrite(
        MidiNetworkMidiTransportTelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_INFO,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this"),
        TraceLoggingWideString(L"A user denied this remote client.", MIDI_TRACE_EVENT_MESSAGE_FIELD)
    );

    auto message = internal::ResourceGetWString(IDS_MESSAGE_INVITATION_DENIED);

    // Spec 6.6: the Pending reply is closed out with a Bye.
    LOG_IF_FAILED(SendToNetwork([&message](MidiNetworkDataWriter& writer)
        {
            RETURN_IF_FAILED(writer.WriteCommandBye(MidiNetworkCommandByeReason::CommandByeReasonHostToClient_InvitationRejectedUserDidNotAccept, message));

            return S_OK;
        }));

    return EndActiveSession(false);
}

HRESULT
MidiNetworkHostConnection::DisconnectByUser()
{
    // Declared HRESULT, so it must not throw: callers use RETURN_IF_FAILED and an
    // escaping WinRT exception would unwind past them into a worker thread.
    try
    {
        auto identity = GetRemoteClientIdentity();

        TraceLoggingWrite(
            MidiNetworkMidiTransportTelemetryProvider::Provider(),
            MIDI_TRACE_EVENT_INFO,
            TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
            TraceLoggingLevel(WINEVENT_LEVEL_INFO),
            TraceLoggingPointer(this, "this"),
            TraceLoggingWideString(L"A user disconnected this remote client.", MIDI_TRACE_EVENT_MESSAGE_FIELD),
            TraceLoggingWideString(identity.UmpEndpointName.c_str(), "client endpoint name"),
            TraceLoggingWideString(identity.ProductInstanceId.c_str(), "client product instance id")
        );

        // A remote which is still parked on an approval decision has had a Pending reply and nothing
        // since. Spec 6.6 says that has to be closed out with a Bye, which is what DenyByUser sends.
        if (m_awaitingUserApproval)
        {
            RETURN_IF_FAILED(DenyByUser());
        }
        else
        {
            LOG_IF_FAILED(SendUserTerminatedByeAndAwaitReply());
        }

        // Releasing the connection is what makes the remote disappear from the enumerateHosts feed.
        // Left registered it would linger until the idle reaper happened to run, which needs a new
        // invitation to arrive, so a disconnected client could sit in the list indefinitely.
        auto remoteHostName = GetRemoteHostName();

        if (remoteHostName != nullptr)
        {
            LOG_IF_FAILED(TransportState::Current().RemoveNetworkConnection(
                remoteHostName,
                winrt::hstring{ GetRemotePort() }));
        }

        // Teardown deletes the MIDI endpoint, which blocks on the device manager. This is called
        // from a service configuration call, so it goes to the worker rather than blocking it.
        auto endpointManager = TransportState::Current().GetEndpointManager();

        if (endpointManager != nullptr)
        {
            return endpointManager->QueueConnectionShutdown(
                std::static_pointer_cast<MidiNetworkConnection>(shared_from_this()));
        }

        return Shutdown();
    }
    CATCH_RETURN()
}

_Use_decl_annotations_
HRESULT
MidiNetworkHostConnection::HandleIncomingInvitation(
    MidiNetworkCommandPacketHeader const& header,
    MidiNetworkCommandInvitationCapabilities const& capabilities,
    std::wstring const& clientUmpEndpointName,
    std::wstring const& clientProductInstanceId
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

    RETURN_HR_IF_NULL(E_UNEXPECTED, TransportState::Current().GetEndpointManager());

    // Spec 6.4. If this host was configured to require authentication we must challenge,
    // never accept. Configuration validation refuses to start such a host today, so this is
    // defense in depth rather than the primary control.
    // TODO: https://github.com/microsoft/MIDI/issues/733
    if (m_authenticationKind != MidiNetworkAuthenticationKind::None)
    {
        TraceLoggingWrite(
            MidiNetworkMidiTransportTelemetryProvider::Provider(),
            MIDI_TRACE_EVENT_WARNING,
            TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
            TraceLoggingLevel(WINEVENT_LEVEL_WARNING),
            TraceLoggingPointer(this, "this"),
            TraceLoggingWideString(L"Host requires authentication, which is not yet implemented. Refusing the invitation.", MIDI_TRACE_EVENT_MESSAGE_FIELD),
            TraceLoggingUInt8(capabilities, "client advertised capabilities")
        );

        return RefuseInvitationForAuthentication(MidiNetworkCommandByeReason::CommandByeReasonHostToClient_NoMatchingAuthenticationMethod);
    }

    if (m_sessionActive)
    {
        // if the session is already active, we simply accept it again

        LOG_IF_FAILED(SendToNetwork([this](MidiNetworkDataWriter& writer)
            {
                RETURN_IF_FAILED(writer.WriteCommandInvitationReplyAccepted(m_thisEndpointName, m_thisProductInstanceId));

                return S_OK;
            }));

        return S_OK;
    }

    // TODO: will we accept a session invitation from the specified hostname?

    // Remember who this is. The approval command and the enumeration feed both need it, and
    // a re-invitation can arrive on another thread while a user is deciding.
    {
        auto lock = m_remoteIdentityLock.lock();

        m_remoteEndpointName = clientUmpEndpointName;
        m_remoteProductInstanceId = clientProductInstanceId;
    }

    auto host = TransportState::Current().GetHost(m_configIdentifier);

    // No host means it was stopped between the datagram arriving and now. Nothing can
    // approve this, so it is refused rather than accepted by default.
    auto decision = host != nullptr
        ? host->EvaluateRemoteClient(MidiNetworkRemoteClientIdentity{ clientUmpEndpointName, clientProductInstanceId })
        : MidiNetworkRemoteClientDecision::DecisionDeny;

    if (decision == MidiNetworkRemoteClientDecision::DecisionDeny)
    {
        TraceLoggingWrite(
            MidiNetworkMidiTransportTelemetryProvider::Provider(),
            MIDI_TRACE_EVENT_WARNING,
            TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
            TraceLoggingLevel(WINEVENT_LEVEL_WARNING),
            TraceLoggingPointer(this, "this"),
            TraceLoggingWideString(L"Invitation refused. The remote client is on the deny list, or could not be identified.", MIDI_TRACE_EVENT_MESSAGE_FIELD),
            TraceLoggingWideString(clientUmpEndpointName.c_str(), "client endpoint name"),
            TraceLoggingWideString(clientProductInstanceId.c_str(), "client product instance id")
        );

        m_awaitingUserApproval = false;

        auto message = internal::ResourceGetWString(IDS_MESSAGE_INVITATION_DENIED);

        LOG_IF_FAILED(SendToNetwork([&message](MidiNetworkDataWriter& writer)
            {
                RETURN_IF_FAILED(writer.WriteCommandBye(MidiNetworkCommandByeReason::CommandByeReasonHostToClient_InvitationRejectedUserDidNotAccept, message));

                return S_OK;
            }));

        return S_OK;
    }

    if (decision == MidiNetworkRemoteClientDecision::DecisionRequireApproval)
    {
        // Spec 6.6. The client is told its invitation is pending and keeps re-inviting while
        // it waits. Nothing is created for it until a user decides, so an unapproved remote
        // costs us no endpoint and no device node.
        auto alreadyPending = m_awaitingUserApproval.exchange(true);

        if (!alreadyPending)
        {
            FILETIME requestedTime{};
            GetSystemTimeAsFileTime(&requestedTime);

            m_userApprovalRequestedFileTime.store(
                (static_cast<uint64_t>(requestedTime.dwHighDateTime) << 32) | requestedTime.dwLowDateTime);

            TraceLoggingWrite(
                MidiNetworkMidiTransportTelemetryProvider::Provider(),
                MIDI_TRACE_EVENT_INFO,
                TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
                TraceLoggingLevel(WINEVENT_LEVEL_INFO),
                TraceLoggingPointer(this, "this"),
                TraceLoggingWideString(L"Invitation is awaiting user approval.", MIDI_TRACE_EVENT_MESSAGE_FIELD),
                TraceLoggingWideString(clientUmpEndpointName.c_str(), "client endpoint name"),
                TraceLoggingWideString(clientProductInstanceId.c_str(), "client product instance id")
            );
        }

        LOG_IF_FAILED(SendToNetwork([this](MidiNetworkDataWriter& writer)
            {
                RETURN_IF_FAILED(writer.WriteCommandInvitationReplyPending(m_thisEndpointName, m_thisProductInstanceId));

                return S_OK;
            }));

        return S_OK;
    }

    m_awaitingUserApproval = false;

    // Captured once. It can be torn down while a datagram is in flight, and each call to
    // GetEndpointManager() is a fresh read.
    auto endpointManager = TransportState::Current().GetEndpointManager();

    RETURN_HR_IF_NULL(S_FALSE, endpointManager);

    if (endpointManager->IsInitialized())
    {
        // Spec 6.6: the host may tell the client permission is being sought and follow with
        // an Accepted or a Bye. Endpoint creation takes over a second when invitations
        // arrive together, and doing it here would block the socket receive callback and
        // every other remote behind it, so it is queued and this returns immediately.
        if (m_hostEndpointCreationPending.exchange(true))
        {
            // a repeated invitation while the endpoint is still being created
            LOG_IF_FAILED(SendToNetwork([this](MidiNetworkDataWriter& writer)
                {
                    RETURN_IF_FAILED(writer.WriteCommandInvitationReplyPending(m_thisEndpointName, m_thisProductInstanceId));

                    return S_OK;
                }));

            return S_OK;
        }

        LOG_IF_FAILED(SendToNetwork([this](MidiNetworkDataWriter& writer)
            {
                RETURN_IF_FAILED(writer.WriteCommandInvitationReplyPending(m_thisEndpointName, m_thisProductInstanceId));

                return S_OK;
            }));

        // A remote which said Bye and then invited again wants an endpoint after all.
        m_hostEndpointCreationAbandoned = false;

        auto queueHr = endpointManager->QueueHostEndpointCreation(
            std::static_pointer_cast<MidiNetworkHostConnection>(shared_from_this()),
            clientUmpEndpointName,
            clientProductInstanceId);

        if (FAILED(queueHr))
        {
            m_hostEndpointCreationPending = false;

            LOG_IF_FAILED(RefuseSessionForEndpointCreationFailure(queueHr));

            RETURN_IF_FAILED(queueHr);
        }
    }
    else
    {
        // this shouldn't happen, but we handle it anyway

        LOG_IF_FAILED(SendToNetwork([](MidiNetworkDataWriter& writer)
            {
                RETURN_IF_FAILED(writer.WriteCommandBye(MidiNetworkCommandByeReason::CommandByeReasonCommon_Undefined, internal::ResourceGetWString(IDS_MESSAGE_HOST_CANNOT_ACCEPT_INVITATIONS)));

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
MidiNetworkHostConnection::HandleIncomingInvitationWithAuthentication(
    MidiNetworkCommandPacketHeader const& header,
    MidiNetworkAuthenticationKind const kind)
{
    UNREFERENCED_PARAMETER(header);
    UNREFERENCED_PARAMETER(kind);

    // We never challenged, so a client answering a challenge is either confused or probing.
    // Spec 6.4 says to Bye rather than leave it hanging.
    // TODO: https://github.com/microsoft/MIDI/issues/733
    return RefuseInvitationForAuthentication(MidiNetworkCommandByeReason::CommandByeReasonHostToClient_NoMatchingAuthenticationMethod);
}
