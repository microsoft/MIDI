// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://github.com/microsoft/MIDI/
// ============================================================================

#include "pch.h"


void MidiNetworkHostConnection::OnSessionEndedBeforeEndpointCreated() noexcept
{
    m_hostEndpointCreationAbandoned = true;

    // Out of line only because TransportState is not declared yet where this is overridden.
    if (m_awaitingUserApproval.exchange(false))
    {
        TransportState::Current().NotificationSignal().SignalPendingApprovalChanged();
    }
}


_Use_decl_annotations_
HRESULT
MidiNetworkHostConnection::Initialize(
    winrt::guid const& configIdentifier,
    std::wstring const& hostParentInstanceId,
    winrt::Windows::Networking::Sockets::DatagramSocket const& socket,
    winrt::Windows::Networking::HostName const& remoteClientHostName,
    winrt::hstring const& remotePort,
    winrt::Windows::Networking::HostName const& localHostName,
    std::wstring const& thisEndpointName,
    std::wstring const& thisProductInstanceId,
    uint16_t const retransmitBufferMaxCommandPacketCount,
    uint8_t const maxForwardErrorCorrectionCommandPacketCount,
    bool createUmpEndpointsOnly,
    uint8_t const fallbackMidi1PortCount
)
{
    return MidiNetworkConnection::Initialize(
        MidiNetworkConnectionRole::ConnectionWindowsIsHost,
        configIdentifier,
        hostParentInstanceId,
        socket,
        remoteClientHostName,
        remotePort,
        localHostName,
        thisEndpointName,
        thisProductInstanceId,
        retransmitBufferMaxCommandPacketCount,
        maxForwardErrorCorrectionCommandPacketCount,
        createUmpEndpointsOnly,
        fallbackMidi1PortCount
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
        m_fallbackMidi1PortCount,
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
    // Cleared last. A repeated invitation meanwhile finds the session live and is answered,
    // instead of queuing a second creation of an endpoint which already exists.
    auto clearPending = wil::scope_exit([this]() { m_hostEndpointCreationPending = false; });

    auto const deviceInstanceId = internal::NormalizeDeviceInstanceIdWStringCopy(newDeviceInstanceId);
    auto const endpointDeviceInterfaceId = internal::NormalizeEndpointInterfaceIdWStringCopy(newEndpointDeviceInterfaceId);

    auto endpointManager = TransportState::Current().GetEndpointManager();

    RETURN_HR_IF_NULL(E_UNEXPECTED, endpointManager);

    HRESULT hr{ S_OK };
    bool claimed{ false };

    {
        auto lock = m_sessionLock.lock();

        // The remote said Bye, or this connection is being shut down
        if (!m_shuttingDown && !m_hostEndpointCreationAbandoned)
        {
            // this is what the Bidi uses when it is created
            hr = TransportState::Current().AssociateMidiEndpointWithConnection(endpointDeviceInterfaceId, m_remoteHostName, m_remotePort.c_str());

            if (SUCCEEDED(hr))
            {
                m_sessionDeviceInstanceId = deviceInstanceId;
                m_sessionEndpointDeviceInterfaceId = endpointDeviceInterfaceId;

                // Before Accepted goes out. A client can send the moment it is accepted, and
                // anything arriving outside a session is refused with a Bye.
                m_sessionActive = true;

                claimed = true;
            }
        }
    }

    if (!claimed)
    {
        // Nothing else knows this endpoint exists. This is the endpoint worker, so it goes now.
        LOG_IF_FAILED(endpointManager->DeleteEndpoint(deviceInstanceId));

        if (FAILED(hr))
        {
            LOG_IF_FAILED(FailHostSessionEndpointCreation(hr));

            RETURN_IF_FAILED(hr);
        }

        return S_FALSE;
    }

    // A Bye since the claim has already ended the session and queued the endpoint's removal
    if (!m_sessionActive)
    {
        return S_FALSE;
    }

    hr = SendInvitationReplyAccepted();

    // Only after Accepted, so no UMP Data can reach the client ahead of it
    if (SUCCEEDED(hr))
    {
        hr = StartOutboundMidiMessageProcessingThread();
    }

    if (FAILED(hr))
    {
        // Removes the endpoint claimed above
        LOG_IF_FAILED(EndActiveSession(false));
        LOG_IF_FAILED(FailHostSessionEndpointCreation(hr));

        RETURN_IF_FAILED(hr);
    }

    m_sessionEverEstablished = true;

    // negotiation needs the connection wired up first, so it cannot happen during creation
    LOG_IF_FAILED(endpointManager->QueueDiscoveryAndNegotiation(endpointDeviceInterfaceId));

    TraceLoggingWrite(
        MidiNetworkMidiTransportTelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_INFO,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this"),
        TraceLoggingWideString(L"Session accepted after deferred endpoint creation", MIDI_TRACE_EVENT_MESSAGE_FIELD),
        TraceLoggingWideString(endpointDeviceInterfaceId.c_str(), "endpoint device interface id")
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

    TransportState::Current().NotificationSignal().SignalPendingApprovalChanged();

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
    if (m_awaitingUserApproval.exchange(false))
    {
        TransportState::Current().NotificationSignal().SignalPendingApprovalChanged();
    }

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
            // Only detached here. It is shut down below.
            TransportState::Current().DetachNetworkConnection(
                remoteHostName,
                winrt::hstring{ GetRemotePort() });
        }

        // Shutdown joins this connection's threads and can remove its endpoint, which blocks on
        // the service. This is a service configuration call, so the endpoint worker does it.
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

    // A host which required authentication would challenge here, before any other decision.
    // Configuration refuses such a host today. See MidiNetworkCredentials.h.
    UNREFERENCED_PARAMETER(capabilities);

    if (m_sessionActive)
    {
        // if the session is already active, we simply accept it again
        LOG_IF_FAILED(SendInvitationReplyAccepted());

        return S_OK;
    }

    // TODO: will we accept a session invitation from the specified hostname?

    MidiNetworkRemoteClientIdentity const identity{ clientUmpEndpointName, clientProductInstanceId };

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
        ? host->EvaluateRemoteClient(identity)
        : MidiNetworkRemoteClientDecision::DecisionDeny;

    if (decision == MidiNetworkRemoteClientDecision::DecisionDeny)
    {
        return RefuseDeniedInvitation(identity);
    }

    if (decision == MidiNetworkRemoteClientDecision::DecisionRequireApproval)
    {
        return AwaitUserApproval(identity);
    }

    m_awaitingUserApproval = false;

    return AcceptInvitation(identity);
}

HRESULT
MidiNetworkHostConnection::SendInvitationReplyPending()
{
    return SendToNetwork([this](MidiNetworkDataWriter& writer)
        {
            RETURN_IF_FAILED(writer.WriteCommandInvitationReplyPending(m_thisEndpointName, m_thisProductInstanceId));

            return S_OK;
        });
}

HRESULT
MidiNetworkHostConnection::SendInvitationReplyAccepted()
{
    return SendToNetwork([this](MidiNetworkDataWriter& writer)
        {
            RETURN_IF_FAILED(writer.WriteCommandInvitationReplyAccepted(m_thisEndpointName, m_thisProductInstanceId));

            return S_OK;
        });
}

_Use_decl_annotations_
HRESULT
MidiNetworkHostConnection::RefuseDeniedInvitation(MidiNetworkRemoteClientIdentity const& identity)
{
    TraceLoggingWrite(
        MidiNetworkMidiTransportTelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_WARNING,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_WARNING),
        TraceLoggingPointer(this, "this"),
        TraceLoggingWideString(L"Invitation refused. The remote client is on the deny list, or could not be identified.", MIDI_TRACE_EVENT_MESSAGE_FIELD),
        TraceLoggingWideString(identity.UmpEndpointName.c_str(), "client endpoint name"),
        TraceLoggingWideString(identity.ProductInstanceId.c_str(), "client product instance id")
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

_Use_decl_annotations_
HRESULT
MidiNetworkHostConnection::AwaitUserApproval(MidiNetworkRemoteClientIdentity const& identity)
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
            TraceLoggingWideString(identity.UmpEndpointName.c_str(), "client endpoint name"),
            TraceLoggingWideString(identity.ProductInstanceId.c_str(), "client product instance id")
        );

        // Nothing here waits on a user. This only lets an app in the customer's session know
        // to ask what is waiting, because the service itself cannot show them anything.
        TransportState::Current().NotificationSignal().SignalPendingApprovalChanged();
    }

    LOG_IF_FAILED(SendInvitationReplyPending());

    return S_OK;
}

_Use_decl_annotations_
HRESULT
MidiNetworkHostConnection::AcceptInvitation(MidiNetworkRemoteClientIdentity const& identity)
{
    // Captured once. It can be torn down while a datagram is in flight, and each call to
    // GetEndpointManager() is a fresh read.
    auto endpointManager = TransportState::Current().GetEndpointManager();

    RETURN_HR_IF_NULL(S_FALSE, endpointManager);

    if (!endpointManager->IsInitialized())
    {
        // this shouldn't happen, but we handle it anyway
        LOG_IF_FAILED(SendToNetwork([](MidiNetworkDataWriter& writer)
            {
                RETURN_IF_FAILED(writer.WriteCommandBye(MidiNetworkCommandByeReason::CommandByeReasonCommon_Undefined, internal::ResourceGetWString(IDS_MESSAGE_HOST_CANNOT_ACCEPT_INVITATIONS)));

                return S_OK;
            }));

        return S_OK;
    }

    // Spec 6.6: the host may tell the client permission is being sought and follow with
    // an Accepted or a Bye. Endpoint creation takes over a second when invitations
    // arrive together, and doing it here would block the socket receive callback and
    // every other remote behind it, so it is queued and this returns immediately.
    if (m_hostEndpointCreationPending.exchange(true))
    {
        // a repeated invitation while the endpoint is still being created
        LOG_IF_FAILED(SendInvitationReplyPending());

        return S_OK;
    }

    // A creation which finished after HandleIncomingInvitation looked has made the session live.
    // It sets the session active before it clears the pending flag, so this cannot miss it.
    if (m_sessionActive)
    {
        m_hostEndpointCreationPending = false;

        return SendInvitationReplyAccepted();
    }

    LOG_IF_FAILED(SendInvitationReplyPending());

    // A remote which said Bye and then invited again wants an endpoint after all.
    m_hostEndpointCreationAbandoned = false;

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

_Use_decl_annotations_
HRESULT
MidiNetworkHostConnection::HandleIncomingInvitationWithAuthentication(
    MidiNetworkCommandPacketHeader const& header)
{
    UNREFERENCED_PARAMETER(header);

    // We never challenged, so a client answering a challenge is either confused or probing.
    // Spec 6.4 says to Bye rather than leave it hanging. A host which supports authentication
    // verifies the digest here instead. See MidiNetworkCredentials.h.
    return RefuseInvitationForAuthentication(MidiNetworkCommandByeReason::CommandByeReasonHostToClient_NoMatchingAuthenticationMethod);
}
