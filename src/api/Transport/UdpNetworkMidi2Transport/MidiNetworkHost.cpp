// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://github.com/microsoft/MIDI/
// ============================================================================

#include "pch.h"

#include <chrono>

_Use_decl_annotations_
HRESULT 
MidiNetworkHost::Initialize(
    MidiNetworkHostDefinition& hostDefinition
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

    RETURN_HR_IF(E_INVALIDARG, hostDefinition.ServiceInstanceName.empty());

    // An empty host name is not fatal. It means no resolvable .local name was found for this
    // machine, and DNS-SD still advertises correctly against a null host name. Logged because
    // it is otherwise invisible and changes which name remote peers resolve.
    if (hostDefinition.HostName.empty())
    {
        TraceLoggingWrite(
            MidiNetworkMidiTransportTelemetryProvider::Provider(),
            MIDI_TRACE_EVENT_WARNING,
            TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
            TraceLoggingLevel(WINEVENT_LEVEL_WARNING),
            TraceLoggingPointer(this, "this"),
            TraceLoggingWideString(L"No .local host name was resolved for this machine. Advertising without an explicit host name.", MIDI_TRACE_EVENT_MESSAGE_FIELD),
            TraceLoggingWideString(hostDefinition.ServiceInstanceName.c_str(), "service instance name")
        );
    }

    RETURN_HR_IF(E_INVALIDARG, hostDefinition.UmpEndpointName.empty());
    RETURN_HR_IF(E_INVALIDARG, hostDefinition.UmpEndpointName.size() > MIDI_MAX_UMP_ENDPOINT_NAME_BYTE_COUNT);

    RETURN_HR_IF(E_INVALIDARG, hostDefinition.ProductInstanceId.empty());
    RETURN_HR_IF(E_INVALIDARG, hostDefinition.ProductInstanceId.size() > MIDI_MAX_UMP_PRODUCT_INSTANCE_ID_BYTE_COUNT);

    //m_configIdentifier = hostDefinition.EntryIdentifier;

    m_started = false;

    m_createUmpEndpointsOnly = !hostDefinition.CreateMidi1Ports;

    m_hostEndpointName = hostDefinition.UmpEndpointName;
    m_hostProductInstanceId = hostDefinition.ProductInstanceId;

    if (!hostDefinition.UseAutomaticPortAllocation)
    {
        RETURN_HR_IF(E_INVALIDARG, hostDefinition.Port.empty());
    }

    m_hostDefinition = hostDefinition;

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
bool
MidiNetworkHost::IsSessionOpeningCommand(uint8_t const commandCode)
{
    switch (commandCode)
    {
    case MidiNetworkCommandCode::CommandClientToHost_Invitation:
    case MidiNetworkCommandCode::CommandClientToHost_InvitationWithAuthentication:
    case MidiNetworkCommandCode::CommandClientToHost_InvitationWithUserAuthentication:
        return true;

    default:
        return false;
    }
}

_Use_decl_annotations_
bool
MidiNetworkHost::WarrantsSessionNotEstablishedBye(uint8_t const commandCode)
{
    // The five commands the spec calls out as requiring Bye 0x05 when no session exists.
    switch (commandCode)
    {
    case MidiNetworkCommandCode::CommandCommon_UmpData:
    case MidiNetworkCommandCode::CommandCommon_RetransmitRequest:
    case MidiNetworkCommandCode::CommandCommon_RetransmitError:
    case MidiNetworkCommandCode::CommandCommon_SessionReset:
    case MidiNetworkCommandCode::CommandCommon_SessionResetReply:
        return true;

    default:
        return false;
    }
}

_Use_decl_annotations_
HRESULT
MidiNetworkHost::SendUnconnectedBye(
    winrt::Windows::Networking::HostName const& remoteHostName,
    winrt::hstring const& remotePort,
    MidiNetworkCommandByeReason const reason,
    std::wstring const& message)
{
    auto socket = GetSocket();

    RETURN_HR_IF_NULL(S_FALSE, socket);
    RETURN_HR_IF_NULL(S_FALSE, remoteHostName);

    try
    {
        MidiNetworkDataWriter writer;

        RETURN_IF_FAILED(writer.Initialize(socket.GetOutputStreamAsync(remoteHostName, remotePort).get()));
        RETURN_IF_FAILED(writer.WriteUdpPacketHeader());
        RETURN_IF_FAILED(writer.WriteCommandBye(reason, message));
        RETURN_IF_FAILED(writer.Send());
    }
    catch (...)
    {
        auto hr = wil::ResultFromCaughtException();

        TraceLoggingWrite(
            MidiNetworkMidiTransportTelemetryProvider::Provider(),
            MIDI_TRACE_EVENT_WARNING,
            TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
            TraceLoggingLevel(WINEVENT_LEVEL_WARNING),
            TraceLoggingPointer(this, "this"),
            TraceLoggingWideString(L"Unable to send Bye to unconnected remote", MIDI_TRACE_EVENT_MESSAGE_FIELD),
            TraceLoggingHResult(hr, MIDI_TRACE_EVENT_HRESULT_FIELD)
        );

        return hr;
    }

    return S_OK;
}

_Use_decl_annotations_
MidiNetworkRemoteClientDecision
MidiNetworkHost::EvaluateRemoteClient(MidiNetworkRemoteClientIdentity const& identity)
{
    // Spec 6.4 requires both fields in an invitation. Without them there is nothing a user
    // could recognise or a list could match, so it is refused rather than approved.
    if (!identity.IsValid())
    {
        return MidiNetworkRemoteClientDecision::DecisionDeny;
    }

    auto key = identity.Key();

    {
        auto lock = m_remoteClientListsLock.lock();

        if (std::find(m_hostDefinition.DeniedClientKeys.begin(), m_hostDefinition.DeniedClientKeys.end(), key) != m_hostDefinition.DeniedClientKeys.end())
        {
            return MidiNetworkRemoteClientDecision::DecisionDeny;
        }

        if (std::find(m_hostDefinition.AllowedClientKeys.begin(), m_hostDefinition.AllowedClientKeys.end(), key) != m_hostDefinition.AllowedClientKeys.end())
        {
            return MidiNetworkRemoteClientDecision::DecisionAllow;
        }
    }

    if (m_hostDefinition.RemoteClientPolicy == MidiNetworkRemoteClientPolicy::PolicyAllowAny)
    {
        return MidiNetworkRemoteClientDecision::DecisionAllow;
    }

    return MidiNetworkRemoteClientDecision::DecisionRequireApproval;
}

_Use_decl_annotations_
HRESULT
MidiNetworkHost::AddRemoteClientToAllowList(MidiNetworkRemoteClientIdentity const& identity)
{
    RETURN_HR_IF(E_INVALIDARG, !identity.IsValid());

    auto key = identity.Key();

    auto lock = m_remoteClientListsLock.lock();

    std::erase(m_hostDefinition.DeniedClientKeys, key);

    if (std::find(m_hostDefinition.AllowedClientKeys.begin(), m_hostDefinition.AllowedClientKeys.end(), key) == m_hostDefinition.AllowedClientKeys.end())
    {
        m_hostDefinition.AllowedClientKeys.push_back(key);
    }

    return S_OK;
}

_Use_decl_annotations_
HRESULT
MidiNetworkHost::AddRemoteClientToDenyList(MidiNetworkRemoteClientIdentity const& identity)
{
    RETURN_HR_IF(E_INVALIDARG, !identity.IsValid());

    auto key = identity.Key();

    auto lock = m_remoteClientListsLock.lock();

    std::erase(m_hostDefinition.AllowedClientKeys, key);

    if (std::find(m_hostDefinition.DeniedClientKeys.begin(), m_hostDefinition.DeniedClientKeys.end(), key) == m_hostDefinition.DeniedClientKeys.end())
    {
        m_hostDefinition.DeniedClientKeys.push_back(key);
    }

    return S_OK;
}

static MidiNetworkAuthenticationKind AuthenticationKindFromHostAuthentication(_In_ MidiNetworkHostAuthentication const authentication)
{
    switch (authentication)
    {
    case MidiNetworkHostAuthentication::PasswordAuthentication:
        return MidiNetworkAuthenticationKind::SharedSecret;

    case MidiNetworkHostAuthentication::UserAuthentication:
        return MidiNetworkAuthenticationKind::UserCredential;

    default:
        return MidiNetworkAuthenticationKind::None;
    }
}

_Use_decl_annotations_
HRESULT
MidiNetworkHost::CreateNetworkConnection(
    HostName const& remoteHostName, 
    winrt::hstring const& remotePort,
    std::shared_ptr<MidiNetworkConnection>& connection)
{
    TraceLoggingWrite(
        MidiNetworkMidiTransportTelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_INFO,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this"),
        TraceLoggingWideString(L"Enter", MIDI_TRACE_EVENT_MESSAGE_FIELD)
    );

    connection = nullptr;

    auto socket = GetSocket();
    RETURN_HR_IF_NULL(E_UNEXPECTED, socket);

    auto conn = std::make_shared<MidiNetworkConnection>();
    RETURN_IF_NULL_ALLOC(conn);

    RETURN_IF_FAILED(conn->InitializeForHost(
        m_hostDefinition.EntryIdentifier,
        m_parentDeviceInstanceId,
        socket,
        remoteHostName,
        remotePort,
        m_hostEndpointName,
        m_hostProductInstanceId,
        TransportState::Current().TransportSettings.RetransmitBufferMaxCommandPacketCount,
        TransportState::Current().TransportSettings.ForwardErrorCorrectionMaxCommandPacketCount,
        m_createUmpEndpointsOnly,
        AuthenticationKindFromHostAuthentication(m_hostDefinition.Authentication),
        MidiNetworkCredentialIdentifier{ std::wstring{ m_hostDefinition.AuthenticationCredentialIdentifier } }
    ));

    // Another thread pool thread may have created one for this same remote while we were
    // initializing. Whichever landed in the map first wins, and the loser is torn down.
    auto winner = TransportState::Current().AddNetworkConnectionIfAbsent(remoteHostName, remotePort, conn);

    if (winner != conn)
    {
        LOG_IF_FAILED(conn->Shutdown());
    }

    connection = winner;

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
MidiNetworkHost::Stop()
{
    TraceLoggingWrite(
        MidiNetworkMidiTransportTelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_INFO,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this"),
        TraceLoggingWideString(L"Enter", MIDI_TRACE_EVENT_MESSAGE_FIELD)
    );

    // First step: stop advertising so no one is encouraged to bug us
    if (m_advertiser)
    {
        RETURN_IF_FAILED(m_advertiser->Shutdown());
        m_advertiser.reset();
    }

    // Two phases. Every remote is told first, because a Bye held up behind another connection's
    // endpoint teardown is a Bye the remote may never see before its own timeout.
    auto connections = TransportState::Current().GetAllNetworkConnectionsForHost(m_hostDefinition.EntryIdentifier);

    for (auto& connection : connections)
    {
        LOG_IF_FAILED(connection->SendShutdownBye());
    }

    for (auto& connection : connections)
    {
        LOG_IF_FAILED(connection->Shutdown());
    }

    // now remove all those connections
    RETURN_IF_FAILED(TransportState::Current().RemoveAllNetworkConnectionsForHost(m_hostDefinition.EntryIdentifier));


    // unbind the port
    DatagramSocket socket{ nullptr };

    {
        auto lock = m_socketLock.lock();
        std::swap(socket, m_socket);
    }

    if (socket)
    {
        try
        {
            socket.MessageReceived(m_messageReceivedEventToken);
            socket.Close();
        }
        CATCH_LOG();
    }

    // NOTE: This doesn't currently work properly because no function in device manager for this.
    // It doesn't remove the parent device, just the children / UMP endpoints. 
    auto endpointManager = TransportState::Current().GetEndpointManager();

    if (endpointManager != nullptr && !m_parentDeviceInstanceId.empty())
    {
        LOG_IF_FAILED(endpointManager->DeleteParentHostDevice(m_parentDeviceInstanceId));
    }

    m_started = false;

    return S_OK;
}


HRESULT
MidiNetworkHost::Start()
{
    TraceLoggingWrite(
        MidiNetworkMidiTransportTelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_INFO,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this"),
        TraceLoggingWideString(L"Enter", MIDI_TRACE_EVENT_MESSAGE_FIELD)
    );


    {
        DatagramSocket socket;
        socket.Control().DontFragment(true);
        //socket.Control().InboundBufferSizeInBytes(10000);
        socket.Control().QualityOfService(SocketQualityOfService::LowLatency);

        auto lock = m_socketLock.lock();
        m_socket = socket;
    }


    auto socket = GetSocket();
    RETURN_HR_IF_NULL(E_UNEXPECTED, socket);

    auto endpointManager = TransportState::Current().GetEndpointManager();
    RETURN_HR_IF_NULL(E_UNEXPECTED, endpointManager);

    std::wstring parentDeviceInstanceId{};
    auto createParentHR = endpointManager->CreateParentDeviceForHost(
        m_hostDefinition.UmpEndpointName,
        m_hostDefinition.ServiceInstanceName,
        parentDeviceInstanceId
    );

    // Every endpoint this host creates is parented to this id. Continuing without one used to be
    // tolerated, and produced a host which looked healthy but failed to create any endpoint.
    RETURN_IF_FAILED(createParentHR);
    RETURN_HR_IF(E_UNEXPECTED, parentDeviceInstanceId.empty());

    m_parentDeviceInstanceId = parentDeviceInstanceId;
   
    // HostName's constructor throws on an empty string, which would escape this HRESULT
    // function. A null HostName is valid for DNS-SD registration.
    HostName hostName{ nullptr };

    if (!m_hostDefinition.HostName.empty())
    {
        try
        {
            hostName = HostName(m_hostDefinition.HostName);
        }
        catch (...)
        {
            TraceLoggingWrite(
                MidiNetworkMidiTransportTelemetryProvider::Provider(),
                MIDI_TRACE_EVENT_WARNING,
                TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
                TraceLoggingLevel(WINEVENT_LEVEL_WARNING),
                TraceLoggingPointer(this, "this"),
                TraceLoggingWideString(L"Host name is not a valid HostName. Advertising without an explicit host name.", MIDI_TRACE_EVENT_MESSAGE_FIELD),
                TraceLoggingWideString(m_hostDefinition.HostName.c_str(), "host name"),
                TraceLoggingHResult(wil::ResultFromCaughtException(), MIDI_TRACE_EVENT_HRESULT_FIELD)
            );
        }
    }

    // wire up to handle incoming events
    // The delegate holds a weak reference, not a raw this. Revoking the token does not drain
    // handlers already dispatched, so the object has to be able to outlive the revoke.
    std::weak_ptr<MidiNetworkHost> weakThis{ weak_from_this() };
    RETURN_HR_IF_MSG(E_UNEXPECTED, weakThis.expired(), "Host must be owned by a shared_ptr before Start");

    auto messageReceivedHandler = winrt::Windows::Foundation::TypedEventHandler<DatagramSocket, DatagramSocketMessageReceivedEventArgs>(
        [weakThis](DatagramSocket const& sender, DatagramSocketMessageReceivedEventArgs const& args)
        {
            if (auto strongThis = weakThis.lock())
            {
                strongThis->OnMessageReceived(sender, args);
            }
        });

    m_messageReceivedEventToken = socket.MessageReceived(messageReceivedHandler);

    uint16_t boundPort{ 0 };

    try
    {
        socket.BindServiceNameAsync(winrt::to_hstring(m_hostDefinition.Port)).get();

        boundPort = static_cast<uint16_t>(std::stoi(winrt::to_string(socket.Information().LocalPort())));
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
            TraceLoggingWideString(L"Unable to bind host socket to the requested port. Host not started.", MIDI_TRACE_EVENT_MESSAGE_FIELD),
            TraceLoggingWideString(m_hostDefinition.Port.c_str(), "port"),
            TraceLoggingHResult(hr, MIDI_TRACE_EVENT_HRESULT_FIELD)
        );

        try
        {
            socket.MessageReceived(m_messageReceivedEventToken);
            socket.Close();
        }
        CATCH_LOG();

        {
            auto lock = m_socketLock.lock();
            m_socket = nullptr;
        }

        RETURN_IF_FAILED(hr);
    }

    // advertise
    if (m_hostDefinition.Advertise)
    {
        m_advertiser = std::make_shared<MidiNetworkAdvertiser>();
        RETURN_IF_NULL_ALLOC(m_advertiser);
        RETURN_IF_FAILED(m_advertiser->Initialize());

        RETURN_IF_FAILED(m_advertiser->Advertise(
            m_hostDefinition.ServiceInstanceName,
            hostName,
            socket,
            boundPort,
            m_hostDefinition.UmpEndpointName,
            m_hostDefinition.ProductInstanceId
        ));
    }

    m_started = true;

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

// "message" here means UDP packet message, not a MIDI message
_Use_decl_annotations_
void MidiNetworkHost::OnMessageReceived(
    DatagramSocket const& sender,
    DatagramSocketMessageReceivedEventArgs const& args)
{
    TraceLoggingWrite(
        MidiNetworkMidiTransportTelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_INFO,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this"),
        TraceLoggingWideString(L"Enter", MIDI_TRACE_EVENT_MESSAGE_FIELD)
    );


    UNREFERENCED_PARAMETER(sender);

    try
    {
        auto reader = args.GetDataReader();

        // the null check has to short-circuit, otherwise a null reader falls through to the read below
        if (reader == nullptr || reader.UnconsumedBufferLength() < MINIMUM_VALID_UDP_PACKET_SIZE)
        {
            // not a message we understand. Needs to be at least the size of the 
            // MIDI header plus a command packet header. Really it needs to be larger, but
            // just trying to weed out blips

            TraceLoggingWrite(
                MidiNetworkMidiTransportTelemetryProvider::Provider(),
                MIDI_TRACE_EVENT_WARNING,
                TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
                TraceLoggingLevel(WINEVENT_LEVEL_WARNING),
                TraceLoggingPointer(this, "this"),
                TraceLoggingWideString(L"Undersized packet", MIDI_TRACE_EVENT_MESSAGE_FIELD)
            );

            return;
        }

        uint32_t udpHeader = reader.ReadUInt32();

        if (udpHeader != MIDI_UDP_PAYLOAD_HEADER)
        {
            // not a message we understand

            return;
        }

        // Read the first command header here so we can decide whether this remote gets a
        // connection at all before we allocate one for it.
        uint32_t firstCommandHeaderWord = reader.ReadUInt32();

        MidiNetworkCommandPacketHeader firstCommandHeader;
        firstCommandHeader.HeaderWord = firstCommandHeaderWord;

        auto conn = TransportState::Current().GetNetworkConnection(args.RemoteAddress(), args.RemotePort());

        if (conn == nullptr)
        {
            // Spec 6.4: a client with no session must open with an invitation. Anything else
            // gets at most a rate-limited refusal, never a connection object or a thread.
            if (!IsSessionOpeningCommand(firstCommandHeader.HeaderData.CommandCode))
            {
                bool refused{ false };

                if (WarrantsSessionNotEstablishedBye(firstCommandHeader.HeaderData.CommandCode) &&
                    args.RemoteAddress() != nullptr)
                {
                    // Rate limited because an unsolicited reply to an unverified source address
                    // is a reflection vector. See MidiNetworkRateLimiter.h.
                    auto key = MidiNetworkReplyRateLimiter::MakeRemoteKey(
                        std::wstring{ args.RemoteAddress().CanonicalName() },
                        std::wstring{ args.RemotePort() });

                    if (m_refusalRateLimiter.ShouldSend(key))
                    {
                        // TODO: Move string to resources for localization
                        LOG_IF_FAILED(SendUnconnectedBye(
                            args.RemoteAddress(),
                            args.RemotePort(),
                            MidiNetworkCommandByeReason::CommandByeReasonCommon_SessionNotEstablished,
                            L"No session is established with this endpoint."));

                        refused = true;
                    }
                }

                TraceLoggingWrite(
                    MidiNetworkMidiTransportTelemetryProvider::Provider(),
                    MIDI_TRACE_EVENT_WARNING,
                    TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
                    TraceLoggingLevel(WINEVENT_LEVEL_WARNING),
                    TraceLoggingPointer(this, "this"),
                    TraceLoggingWideString(L"First command from an unknown remote was not an invitation.", MIDI_TRACE_EVENT_MESSAGE_FIELD),
                    TraceLoggingBool(refused, "refused with Bye"),
                    TraceLoggingUInt8(firstCommandHeader.HeaderData.CommandCode, "Command Code"),
                    TraceLoggingWideString(args.RemoteAddress() != nullptr ? args.RemoteAddress().CanonicalName().c_str() : L"", "remote address")
                );

                return;
            }

            // Reclaim connections abandoned by earlier sessions before we add another. Remotes
            // reconnect from a new ephemeral port, so this is where growth would otherwise happen.
            LOG_IF_FAILED(TransportState::Current().ReapIdleNetworkConnections(m_hostDefinition.EntryIdentifier));

            if (TransportState::Current().CountNetworkConnectionsForConfigIdentifier(m_hostDefinition.EntryIdentifier) >= TransportState::Current().TransportSettings.MaxHostConnections)
            {
                // The spec has a reason code for precisely this. Staying silent leaves the
                // client unable to tell a full host from a dead one.
                if (args.RemoteAddress() != nullptr)
                {
                    auto key = MidiNetworkReplyRateLimiter::MakeRemoteKey(
                        std::wstring{ args.RemoteAddress().CanonicalName() },
                        std::wstring{ args.RemotePort() });

                    if (m_refusalRateLimiter.ShouldSend(key))
                    {
                        // TODO: Move string to resources for localization
                        LOG_IF_FAILED(SendUnconnectedBye(
                            args.RemoteAddress(),
                            args.RemotePort(),
                            MidiNetworkCommandByeReason::CommandByeReasonHostToClient_TooManyOpenSessions,
                            L"This host already has the maximum number of open sessions."));
                    }
                }

                TraceLoggingWrite(
                    MidiNetworkMidiTransportTelemetryProvider::Provider(),
                    MIDI_TRACE_EVENT_WARNING,
                    TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
                    TraceLoggingLevel(WINEVENT_LEVEL_WARNING),
                    TraceLoggingPointer(this, "this"),
                    TraceLoggingWideString(L"Host is at its connection limit. Invitation refused.", MIDI_TRACE_EVENT_MESSAGE_FIELD),
                    TraceLoggingWideString(args.RemoteAddress() != nullptr ? args.RemoteAddress().CanonicalName().c_str() : L"", "remote address")
                );

                return;
            }

            LOG_IF_FAILED(CreateNetworkConnection(args.RemoteAddress(), args.RemotePort(), conn));
        }

        if (conn)
        {
            LOG_IF_FAILED(conn->ProcessIncomingMessage(reader, firstCommandHeaderWord));

            // Release as soon as the session is over. A remote reconnects from a new ephemeral
            // port, so holding the old entry would consume a connection slot and two threads
            // until the idle reaper eventually noticed. Safe to remove the entry we are
            // executing on: the local shared_ptr keeps the object alive until this returns.
            if (conn->IsSessionFinished())
            {
                TraceLoggingWrite(
                    MidiNetworkMidiTransportTelemetryProvider::Provider(),
                    MIDI_TRACE_EVENT_INFO,
                    TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
                    TraceLoggingLevel(WINEVENT_LEVEL_INFO),
                    TraceLoggingPointer(this, "this"),
                    TraceLoggingWideString(L"Session ended. Releasing the connection.", MIDI_TRACE_EVENT_MESSAGE_FIELD),
                    TraceLoggingWideString(args.RemoteAddress() != nullptr ? args.RemoteAddress().CanonicalName().c_str() : L"", "remote address")
                );

                auto released = TransportState::Current().DetachNetworkConnection(args.RemoteAddress(), args.RemotePort());

                if (released != nullptr)
                {
                    auto endpointManager = TransportState::Current().GetEndpointManager();

                    if (endpointManager != nullptr)
                    {
                        LOG_IF_FAILED(endpointManager->QueueConnectionShutdown(released));
                    }
                }
            }
        }
        else
        {
            TraceLoggingWrite(
                MidiNetworkMidiTransportTelemetryProvider::Provider(),
                MIDI_TRACE_EVENT_ERROR,
                TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
                TraceLoggingLevel(WINEVENT_LEVEL_ERROR),
                TraceLoggingPointer(this, "this"),
                TraceLoggingWideString(L"Message received from remote client, but no connection could be created", MIDI_TRACE_EVENT_MESSAGE_FIELD)
            );
        }
    }
    CATCH_LOG();

    TraceLoggingWrite(
        MidiNetworkMidiTransportTelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_INFO,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this"),
        TraceLoggingWideString(L"Exit", MIDI_TRACE_EVENT_MESSAGE_FIELD)
    );

}


HRESULT 
MidiNetworkHost::Shutdown()
{
    TraceLoggingWrite(
        MidiNetworkMidiTransportTelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_INFO,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this"),
        TraceLoggingWideString(L"Enter", MIDI_TRACE_EVENT_MESSAGE_FIELD)
    );

    LOG_IF_FAILED(Stop());

    //while (m_connections.size() > 0)
    //{
    //    auto conn = m_connections.begin();
    //    LOG_IF_FAILED(conn->second->Shutdown());

    //    m_connections.erase(conn);
    //}

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

