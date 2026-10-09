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
    MidiNetworkHostDefinition const& hostDefinition
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
    // machine, and the registration uses the DNS host name in .local instead. Logged because
    // it is otherwise invisible and changes which name remote peers resolve.
    if (hostDefinition.HostName.empty())
    {
        TraceLoggingWrite(
            MidiNetworkMidiTransportTelemetryProvider::Provider(),
            MIDI_TRACE_EVENT_WARNING,
            TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
            TraceLoggingLevel(WINEVENT_LEVEL_WARNING),
            TraceLoggingPointer(this, "this"),
            TraceLoggingWideString(L"No .local host name was resolved for this machine. Advertising with the DNS host name in .local instead.", MIDI_TRACE_EVENT_MESSAGE_FIELD),
            TraceLoggingWideString(hostDefinition.ServiceInstanceName.c_str(), "service instance name")
        );
    }

    RETURN_HR_IF(E_INVALIDARG, hostDefinition.UmpEndpointName.empty());
    RETURN_HR_IF(E_INVALIDARG, hostDefinition.UmpEndpointName.size() > MIDI_MAX_UMP_ENDPOINT_NAME_BYTE_COUNT);

    RETURN_HR_IF(E_INVALIDARG, hostDefinition.ProductInstanceId.empty());
    RETURN_HR_IF(E_INVALIDARG, hostDefinition.ProductInstanceId.size() > MIDI_MAX_UMP_PRODUCT_INSTANCE_ID_BYTE_COUNT);

    m_started = false;

    m_createUmpEndpointsOnly = !hostDefinition.CreateMidi1Ports;
    m_fallbackMidi1PortCount = hostDefinition.FallbackMidi1PortCount;
    m_sendSpeedLimit = hostDefinition.SendSpeedLimit;
    m_reduceSendSpeedAutomatically = hostDefinition.ReduceSendSpeedAutomatically;

    m_hostEndpointName = hostDefinition.UmpEndpointName;
    m_hostProductInstanceId = hostDefinition.ProductInstanceId;

    if (!hostDefinition.UseAutomaticPortAllocation)
    {
        RETURN_HR_IF(E_INVALIDARG, hostDefinition.Port.empty());
    }

    m_entryIdentifier = hostDefinition.EntryIdentifier;

    {
        auto lock = m_remoteClientListsLock.lock();
        m_hostDefinition = hostDefinition;
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
    winrt::Windows::Networking::HostName const& localHostName,
    MidiNetworkCommandByeReason const reason,
    std::wstring const& message)
{
    auto socket = GetSocket();

    RETURN_HR_IF_NULL(S_FALSE, socket);
    RETURN_HR_IF_NULL(S_FALSE, remoteHostName);

    try
    {
        MidiNetworkDataWriter writer;

        RETURN_IF_FAILED(writer.Initialize(MidiNetworkConnection::GetReplyOutputStream(socket, localHostName, remoteHostName, remotePort)));
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
    // could recognize or a list could match, so it is refused rather than approved.
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

_Use_decl_annotations_
HRESULT
MidiNetworkHost::ForgetRemoteClient(MidiNetworkRemoteClientIdentity const& identity)
{
    RETURN_HR_IF(E_INVALIDARG, !identity.IsValid());

    auto key = identity.Key();

    auto lock = m_remoteClientListsLock.lock();

    std::erase(m_hostDefinition.AllowedClientKeys, key);
    std::erase(m_hostDefinition.DeniedClientKeys, key);

    return S_OK;
}

_Use_decl_annotations_
void
MidiNetworkHost::SetRemoteClientSettings(std::vector<MidiNetworkRemoteClientSettings> const& settings)
{
    auto lock = m_remoteClientListsLock.lock();

    m_hostDefinition.RemoteClientSettings = settings;
}

_Use_decl_annotations_
MidiNetworkRemoteClientSendSpeed
MidiNetworkHost::GetSendSpeedForRemoteClient(MidiNetworkRemoteClientIdentity const& identity) noexcept
{
    // the definition only has the host's own speed as it was created, so it comes from here
    MidiNetworkRemoteClientSendSpeed speed{ m_sendSpeedLimit, m_reduceSendSpeedAutomatically, false };

    try
    {
        auto lock = m_remoteClientListsLock.lock();

        if (auto const settings = m_hostDefinition.FindRemoteClientSettings(identity); settings != nullptr)
        {
            speed = { settings->SendSpeedLimit, settings->ReduceSendSpeedAutomatically, true };
        }
    }
    CATCH_LOG();

    return speed;
}

_Use_decl_annotations_
HRESULT
MidiNetworkHost::CreateNetworkConnection(
    HostName const& remoteHostName, 
    winrt::hstring const& remotePort,
    HostName const& localHostName,
    std::shared_ptr<MidiNetworkConnection>& connection)
{
    // Declared HRESULT, so it must not throw: callers use RETURN_IF_FAILED and an
    // escaping WinRT exception would unwind past them into a worker thread.
    try
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

        auto conn = std::make_shared<MidiNetworkHostConnection>();
        RETURN_IF_NULL_ALLOC(conn);

        RETURN_IF_FAILED(conn->Initialize(
            m_entryIdentifier,
            m_parentDeviceInstanceId,
            socket,
            remoteHostName,
            remotePort,
            localHostName,
            m_hostEndpointName,
            m_hostProductInstanceId,
            TransportState::Current().TransportSettings.RetransmitBufferMaxCommandPacketCount,
            TransportState::Current().TransportSettings.ForwardErrorCorrectionMaxCommandPacketCount,
            m_createUmpEndpointsOnly,
            m_fallbackMidi1PortCount
        ));

        conn->SetSendSpeedLimit(m_sendSpeedLimit, m_reduceSendSpeedAutomatically);

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
            TraceLoggingWideString(L"Exit", MIDI_TRACE_EVENT_MESSAGE_FIELD),
            TraceLoggingWideString(remoteHostName != nullptr ? remoteHostName.CanonicalName().c_str() : L"", "remote address"),
            TraceLoggingWideString(remotePort.c_str(), "remote port"),
            TraceLoggingWideString(localHostName != nullptr ? localHostName.CanonicalName().c_str() : L"", "local address")
        );

        return S_OK;
    }
    CATCH_RETURN()
}

bool
MidiNetworkHost::ServiceInstanceNameWasChanged()
{
    auto advertiser = GetAdvertiser();

    return advertiser != nullptr && advertiser->InstanceNameWasChanged();
}

winrt::hstring
MidiNetworkHost::ActualServiceInstanceName()
{
    return ActualServiceInstanceName(GetAdvertiser());
}

_Use_decl_annotations_
winrt::hstring
MidiNetworkHost::ActualServiceInstanceName(std::shared_ptr<MidiNetworkAdvertiser> const& advertiser)
{
    if (advertiser == nullptr) return m_hostDefinition.ServiceInstanceName;

    auto const actual = advertiser->ActualInstanceNameWithoutSuffix();

    return actual.empty() ? m_hostDefinition.ServiceInstanceName : actual;
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

    auto lifecycleLock = m_lifecycleLock.lock();

    // First step: stop advertising so no one is encouraged to bug us
    std::shared_ptr<MidiNetworkAdvertiser> advertiser{ nullptr };

    {
        auto lock = m_advertiserLock.lock();
        std::swap(advertiser, m_advertiser);
    }

    if (advertiser != nullptr)
    {
        // Before the goodbye. A repeat sent after it would put this host back in other devices'
        // lists for the record's 75 minute lifetime.
        if (auto endpointManager = TransportState::Current().GetEndpointManager())
        {
            endpointManager->OnHostRegistrationEnding(ActualServiceInstanceName(advertiser));
        }

        LOG_IF_FAILED(advertiser->Shutdown());
    }

    // Two phases. Every remote is told first, because a Bye held up behind another connection's
    // endpoint teardown is a Bye the remote may never see before its own timeout.
    auto connections = TransportState::Current().GetAllNetworkConnectionsForHost(m_entryIdentifier);

    for (auto& connection : connections)
    {
        LOG_IF_FAILED(connection->SendShutdownBye());
    }

    for (auto& connection : connections)
    {
        LOG_IF_FAILED(connection->Shutdown());
    }

    // now remove all those connections
    RETURN_IF_FAILED(TransportState::Current().RemoveAllNetworkConnectionsForHost(m_entryIdentifier));


    // unbind the port
    DatagramSocket socket{ nullptr };

    {
        auto lock = m_socketLock.lock();
        std::swap(socket, m_socket);
        m_activeNetworkAdapterId = winrt::guid{};
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

    // The parent device is deliberately left alone. It is created once per host and lives for the
    // lifetime of the transport, the same as the one shared by client endpoints. Deactivating it
    // here made a restarted host unusable: the instance id survives deactivation, so Start could
    // not activate it again, and every endpoint the host went on to create was parented to a
    // device which no longer existed. The connection shutdowns above have already queued the
    // removal of the child endpoints.

    m_started = false;
    m_networkAdapterFallbackUsed = false;

    // A stopped host does not wait for its adapter. ReconcileNetworkAdapter sets this again when
    // the adapter going away is why it stopped.
    SetWaitingForNetworkAdapter(false);

    return S_OK;
}


HRESULT
MidiNetworkHost::Start()
try
{
    TraceLoggingWrite(
        MidiNetworkMidiTransportTelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_INFO,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this"),
        TraceLoggingWideString(L"Enter", MIDI_TRACE_EVENT_MESSAGE_FIELD)
    );

    auto lifecycleLock = m_lifecycleLock.lock();

    // Starting a running host again used to put a second socket and a second DNS-SD registration
    // underneath it, and the first socket stayed bound.
    if (m_started)
    {
        TraceLoggingWrite(
            MidiNetworkMidiTransportTelemetryProvider::Provider(),
            MIDI_TRACE_EVENT_INFO,
            TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
            TraceLoggingLevel(WINEVENT_LEVEL_INFO),
            TraceLoggingPointer(this, "this"),
            TraceLoggingWideString(L"Host is already running. Nothing to start.", MIDI_TRACE_EVENT_MESSAGE_FIELD)
        );

        return S_OK;
    }

    // Before anything is created, because a host which has to wait for its adapter creates nothing
    winrt::guid networkAdapterId{};
    uint32_t networkInterfaceIndex{ 0 };
    bool networkAdapterFallbackUsed{ false };

    if (!ChooseNetworkAdapter(networkAdapterId, networkInterfaceIndex, networkAdapterFallbackUsed))
    {
        return HRESULT_FROM_WIN32(ERROR_DEV_NOT_EXIST);
    }

    auto endpointManager = TransportState::Current().GetEndpointManager();
    RETURN_HR_IF_NULL(E_UNEXPECTED, endpointManager);

    // Created once and kept for the lifetime of the transport, so a restart reuses it rather than
    // asking for it again. Asking again cannot succeed, and the id handed back when it fails is
    // the one that was requested rather than the one PnP assigned, which is prefixed. Endpoints
    // built on the unprefixed id name a parent that does not exist, and their activation waits
    // for a completion that never comes.
    if (m_parentDeviceInstanceId.empty())
    {
        std::wstring parentDeviceInstanceId{};

        RETURN_IF_FAILED(endpointManager->CreateParentDeviceForHost(
            m_hostDefinition.UmpEndpointName,
            m_hostDefinition.ServiceInstanceName,
            parentDeviceInstanceId
        ));

        RETURN_HR_IF(E_UNEXPECTED, parentDeviceInstanceId.empty());

        m_parentDeviceInstanceId = parentDeviceInstanceId;
    }

    DatagramSocket socket;
    socket.Control().DontFragment(true);
    socket.Control().QualityOfService(SocketQualityOfService::LowLatency);

    // Every remote client shares this one socket, so their bursts land in the same buffer.
    socket.Control().InboundBufferSizeInBytes(MIDI_NETWORK_SOCKET_RECEIVE_BUFFER_BYTES);

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

    {
        auto lock = m_socketLock.lock();
        m_socket = socket;
        m_activeNetworkAdapterId = networkAdapterId;
    }

    // a failure from here on must not leave a socket bound, or a handler registered, behind it
    auto unbindOnFailure = wil::scope_exit([&]()
        {
            {
                auto lock = m_socketLock.lock();
                m_socket = nullptr;
                m_activeNetworkAdapterId = winrt::guid{};
            }

            try
            {
                socket.MessageReceived(m_messageReceivedEventToken);
                socket.Close();
            }
            CATCH_LOG();
        });

    uint16_t boundPort{ 0 };

    // Bound on every adapter even when the host is limited to one. A socket bound to one
    // adapter's address cannot reply from it to an address the platform has to resolve, and the
    // receive path ignores whatever arrives on the others.
    RETURN_IF_FAILED(BindSocket(socket, boundPort));

    if (m_hostDefinition.Advertise)
    {
        RETURN_IF_FAILED(StartAdvertising(boundPort, networkAdapterId, networkInterfaceIndex));
    }

    unbindOnFailure.release();

    m_networkAdapterFallbackUsed = networkAdapterFallbackUsed;
    m_started = true;

    SetWaitingForNetworkAdapter(false);

    TraceLoggingWrite(
        MidiNetworkMidiTransportTelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_INFO,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this"),
        TraceLoggingWideString(L"Exit", MIDI_TRACE_EVENT_MESSAGE_FIELD),
        TraceLoggingUInt16(boundPort, "bound port"),
        TraceLoggingGuid(networkAdapterId, "network adapter"),
        TraceLoggingBool(networkAdapterFallbackUsed, "network adapter fallback used")
    );

    return S_OK;
}
CATCH_RETURN()

_Use_decl_annotations_
HRESULT
MidiNetworkHost::BindSocket(DatagramSocket const& socket, uint16_t& boundPort)
{
    boundPort = 0;
    m_portFallbackUsed = false;

    try
    {
        socket.BindServiceNameAsync(winrt::to_hstring(m_hostDefinition.Port)).get();

        boundPort = static_cast<uint16_t>(std::stoi(winrt::to_string(socket.Information().LocalPort())));

        return S_OK;
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
            TraceLoggingWideString(L"Unable to bind host socket to the requested port.", MIDI_TRACE_EVENT_MESSAGE_FIELD),
            TraceLoggingWideString(m_hostDefinition.Port.c_str(), "port"),
            TraceLoggingHResult(hr, MIDI_TRACE_EVENT_HRESULT_FIELD)
        );

        // A configured port can be taken by something else which started first, and after a
        // reboot that is entirely outside the user's control. Falling back keeps MIDI working;
        // the host reports that it did so, so the settings app can offer a new port.
        if (!m_hostDefinition.UseAutomaticPortAllocation && m_hostDefinition.AllowPortFallback)
        {
            try
            {
                socket.BindServiceNameAsync(L"").get();

                boundPort = static_cast<uint16_t>(std::stoi(winrt::to_string(socket.Information().LocalPort())));

                m_portFallbackUsed = true;

                TraceLoggingWrite(
                    MidiNetworkMidiTransportTelemetryProvider::Provider(),
                    MIDI_TRACE_EVENT_WARNING,
                    TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
                    TraceLoggingLevel(WINEVENT_LEVEL_WARNING),
                    TraceLoggingPointer(this, "this"),
                    TraceLoggingWideString(L"Configured port was unavailable. Host started on an automatically allocated port.", MIDI_TRACE_EVENT_MESSAGE_FIELD),
                    TraceLoggingWideString(m_hostDefinition.Port.c_str(), "configured port"),
                    TraceLoggingUInt16(boundPort, "bound port")
                );

                return S_OK;
            }
            CATCH_LOG();
        }

        TraceLoggingWrite(
            MidiNetworkMidiTransportTelemetryProvider::Provider(),
            MIDI_TRACE_EVENT_ERROR,
            TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
            TraceLoggingLevel(WINEVENT_LEVEL_ERROR),
            TraceLoggingPointer(this, "this"),
            TraceLoggingWideString(L"Host not started.", MIDI_TRACE_EVENT_MESSAGE_FIELD),
            TraceLoggingWideString(m_hostDefinition.Port.c_str(), "port")
        );

        RETURN_IF_FAILED(hr);
    }

    return E_UNEXPECTED;
}

_Use_decl_annotations_
HRESULT
MidiNetworkHost::StartAdvertising(
    uint16_t const boundPort,
    winrt::guid const& networkAdapterId,
    uint32_t const networkInterfaceIndex)
{
    auto advertiser = std::make_shared<MidiNetworkAdvertiser>();

    RETURN_IF_FAILED(advertiser->Initialize());

    auto const advertiseResult = advertiser->Advertise(
        m_hostDefinition.ServiceInstanceName,
        m_hostDefinition.HostName,
        boundPort,
        m_hostDefinition.UmpEndpointName,
        m_hostDefinition.ProductInstanceId,
        networkInterfaceIndex
    );

    RETURN_IF_FAILED(advertiseResult);

    {
        auto lock = m_advertiserLock.lock();
        m_advertiser = advertiser;
    }

    // Not repeated while the DNS client is still working on it, because it may yet settle on
    // another label
    if (advertiseResult != S_OK)
    {
        return S_OK;
    }

    // The DNS client announces a new registration only once and marks it wrongly, so the
    // transport repeats the announcement. The reasons are in MidiNetworkAdvertiser.cpp.
    if (auto endpointManager = TransportState::Current().GetEndpointManager())
    {
        try
        {
            endpointManager->OnHostRegistered(ActualServiceInstanceName(advertiser), networkAdapterId);
        }
        CATCH_LOG();
    }

    return S_OK;
}

_Use_decl_annotations_
bool
MidiNetworkHost::ChooseNetworkAdapter(
    winrt::guid& adapterId,
    uint32_t& interfaceIndex,
    bool& fallbackUsed)
{
    adapterId = winrt::guid{};
    interfaceIndex = 0;
    fallbackUsed = false;

    auto const definition = GetDefinition();

    if (!definition.IsLimitedToNetworkAdapter())
    {
        return true;
    }

    ::WindowsMidiServicesInternal::MidiNetworkAdapterInfo found{};

    if (::WindowsMidiServicesInternal::TryFindUsableMidiNetworkAdapter(
            ::WindowsMidiServicesInternal::GetMidiNetworkAdapters(),
            definition.NetworkAdapterId,
            std::wstring{ definition.NetworkAdapterPhysicalAddress },
            found))
    {
        adapterId = found.Id;
        interfaceIndex = found.InterfaceIndex();

        return true;
    }

    if (definition.AllowNetworkAdapterFallback)
    {
        fallbackUsed = true;

        TraceLoggingWrite(
            MidiNetworkMidiTransportTelemetryProvider::Provider(),
            MIDI_TRACE_EVENT_WARNING,
            TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
            TraceLoggingLevel(WINEVENT_LEVEL_WARNING),
            TraceLoggingPointer(this, "this"),
            TraceLoggingWideString(L"The host's network adapter is missing. Running on every adapter until it is back.", MIDI_TRACE_EVENT_MESSAGE_FIELD),
            TraceLoggingGuid(definition.NetworkAdapterId, "network adapter"),
            TraceLoggingWideString(definition.NetworkAdapterName.c_str(), "network adapter name")
        );

        return true;
    }

    TraceLoggingWrite(
        MidiNetworkMidiTransportTelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_WARNING,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_WARNING),
        TraceLoggingPointer(this, "this"),
        TraceLoggingWideString(L"The host's network adapter is missing, and it may not use another. Waiting for the adapter.", MIDI_TRACE_EVENT_MESSAGE_FIELD),
        TraceLoggingGuid(definition.NetworkAdapterId, "network adapter"),
        TraceLoggingWideString(definition.NetworkAdapterName.c_str(), "network adapter name")
    );

    SetWaitingForNetworkAdapter(true);

    return false;
}

_Use_decl_annotations_
void
MidiNetworkHost::SetWaitingForNetworkAdapter(bool const waiting) noexcept
{
    if (m_waitingForNetworkAdapter.exchange(waiting) != waiting)
    {
        TransportState::Current().NotificationSignal().SignalHostNetworkAdapterChanged();
    }
}

_Use_decl_annotations_
void
MidiNetworkHost::SetNetworkAdapter(
    winrt::guid const& id,
    winrt::hstring const& name,
    winrt::hstring const& physicalAddress,
    bool const allowFallback)
{
    auto lock = m_remoteClientListsLock.lock();

    m_hostDefinition.NetworkAdapterId = id;
    m_hostDefinition.NetworkAdapterName = name;
    m_hostDefinition.NetworkAdapterPhysicalAddress = physicalAddress;
    m_hostDefinition.AllowNetworkAdapterFallback = allowFallback;
}

_Use_decl_annotations_
HRESULT
MidiNetworkHost::ReconcileNetworkAdapter(std::vector<::WindowsMidiServicesInternal::MidiNetworkAdapterInfo> const& adapters)
try
{
    auto lifecycleLock = m_lifecycleLock.lock();

    // One the customer stopped stays stopped, and so does one which failed for another reason
    if (!m_started && !m_waitingForNetworkAdapter)
    {
        return S_OK;
    }

    auto const definition = GetDefinition();

    // Where the host should be now. A null id is every adapter.
    winrt::guid wanted{};
    bool wait{ false };

    if (definition.IsLimitedToNetworkAdapter())
    {
        ::WindowsMidiServicesInternal::MidiNetworkAdapterInfo found{};

        if (::WindowsMidiServicesInternal::TryFindUsableMidiNetworkAdapter(
                adapters,
                definition.NetworkAdapterId,
                std::wstring{ definition.NetworkAdapterPhysicalAddress },
                found))
        {
            wanted = found.Id;
        }
        else
        {
            wait = !definition.AllowNetworkAdapterFallback;
        }
    }

    if (!m_started)
    {
        // still waiting
        if (wait) return S_OK;

        TraceLoggingWrite(
            MidiNetworkMidiTransportTelemetryProvider::Provider(),
            MIDI_TRACE_EVENT_INFO,
            TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
            TraceLoggingLevel(WINEVENT_LEVEL_INFO),
            TraceLoggingPointer(this, "this"),
            TraceLoggingWideString(L"The host's network adapter is back. Starting the host.", MIDI_TRACE_EVENT_MESSAGE_FIELD),
            TraceLoggingGuid(wanted, "network adapter")
        );

        return Start();
    }

    winrt::guid active{};

    {
        auto lock = m_socketLock.lock();
        active = m_activeNetworkAdapterId;
    }

    if (!wait && wanted == active)
    {
        return S_OK;
    }

    TraceLoggingWrite(
        MidiNetworkMidiTransportTelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_INFO,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this"),
        TraceLoggingWideString(L"The host's network adapter changed. Restarting the host where it should be now.", MIDI_TRACE_EVENT_MESSAGE_FIELD),
        TraceLoggingGuid(active, "running on"),
        TraceLoggingGuid(wanted, "moving to"),
        TraceLoggingBool(wait, "waiting")
    );

    // Every connection goes with it. They came in on an adapter the host is leaving, or would
    // not be allowed on the one it is moving to.
    RETURN_IF_FAILED(Stop());

    if (wait)
    {
        SetWaitingForNetworkAdapter(true);

        return S_OK;
    }

    return Start();
}
CATCH_RETURN()

_Use_decl_annotations_
bool
MidiNetworkHost::ArrivedOnActiveNetworkAdapter(DatagramSocketMessageReceivedEventArgs const& args)
{
    winrt::guid active{};

    {
        auto lock = m_socketLock.lock();
        active = m_activeNetworkAdapterId;
    }

    if (active == winrt::guid{})
    {
        return true;
    }

    try
    {
        // Measured: the local address of a received datagram knows its adapter, for IPv4 and for
        // link-local IPv6 alike
        auto const local = args.LocalAddress();
        auto const info = local != nullptr ? local.IPInformation() : nullptr;
        auto const adapter = info != nullptr ? info.NetworkAdapter() : nullptr;

        return adapter != nullptr && adapter.NetworkAdapterId() == active;
    }
    catch (...)
    {
        // A host limited to one adapter does not answer what it cannot place
        return false;
    }
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

        // Read the first command header here so we can decide whether this remote gets a
        // connection at all before we allocate one for it.
        uint32_t firstCommandHeaderWord{ 0 };

        auto prologue = ReadNetworkPacketPrologue(reader, firstCommandHeaderWord);

        if (prologue == MidiNetworkPacketPrologueResult::TooSmall)
        {
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

        if (prologue != MidiNetworkPacketPrologueResult::Ok)
        {
            return;
        }

        MidiNetworkCommandPacketHeader firstCommandHeader;
        firstCommandHeader.HeaderWord = firstCommandHeaderWord;

        auto conn = TransportState::Current().GetNetworkConnection(args.RemoteAddress(), args.RemotePort());

        if (conn == nullptr)
        {
            conn = AdmitNewRemote(args, firstCommandHeader);
        }
        else if (IsSessionOpeningCommand(firstCommandHeader.HeaderData.CommandCode) &&
            conn->ConfigIdentifier() == m_entryIdentifier &&
            ArrivedOnActiveNetworkAdapter(args))
        {
            // An IPv6 host has several addresses and can lose the one a remote was using
            if (auto socket = GetSocket(); socket != nullptr)
            {
                LOG_IF_FAILED(conn->FollowReplySource(socket, args.LocalAddress()));
            }
        }

        if (conn != nullptr)
        {
            LOG_IF_FAILED(conn->ProcessIncomingMessage(reader, firstCommandHeaderWord));

            ReleaseConnectionIfSessionFinished(conn, args);
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

_Use_decl_annotations_
std::shared_ptr<MidiNetworkConnection>
MidiNetworkHost::AdmitNewRemote(
    DatagramSocketMessageReceivedEventArgs const& args,
    MidiNetworkCommandPacketHeader const& firstCommandHeader)
{
    // A host limited to one adapter ignores what arrives on any other, without a reply, as though
    // it were not there. The socket is bound on every adapter, so this is what limits it.
    if (!ArrivedOnActiveNetworkAdapter(args))
    {
        TraceLoggingWrite(
            MidiNetworkMidiTransportTelemetryProvider::Provider(),
            MIDI_TRACE_EVENT_VERBOSE,
            TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
            TraceLoggingLevel(WINEVENT_LEVEL_VERBOSE),
            TraceLoggingPointer(this, "this"),
            TraceLoggingWideString(L"Ignored a datagram which arrived on a network adapter this host is not limited to.", MIDI_TRACE_EVENT_MESSAGE_FIELD),
            TraceLoggingWideString(args.LocalAddress() != nullptr ? args.LocalAddress().CanonicalName().c_str() : L"", "local address"),
            TraceLoggingWideString(args.RemoteAddress() != nullptr ? args.RemoteAddress().CanonicalName().c_str() : L"", "remote address")
        );

        return nullptr;
    }

    // Spec 6.4: a client with no session must open with an invitation. Anything else gets at
    // most a rate-limited refusal, never a connection object or a thread.
    if (!IsSessionOpeningCommand(firstCommandHeader.HeaderData.CommandCode))
    {
        bool refused{ false };

        if (WarrantsSessionNotEstablishedBye(firstCommandHeader.HeaderData.CommandCode) &&
            args.RemoteAddress() != nullptr)
        {
            // Rate limited because an unsolicited reply to an unverified source address is a
            // reflection vector. See MidiNetworkRateLimiter.h.
            auto key = MidiNetworkReplyRateLimiter::MakeRemoteKey(
                std::wstring{ args.RemoteAddress().CanonicalName() },
                std::wstring{ args.RemotePort() });

            if (m_refusalRateLimiter.ShouldSend(key))
            {
                LOG_IF_FAILED(SendUnconnectedBye(
                    args.RemoteAddress(),
                    args.RemotePort(),
                    args.LocalAddress(),
                    MidiNetworkCommandByeReason::CommandByeReasonCommon_SessionNotEstablished,
                    internal::ResourceGetWString(IDS_MESSAGE_NO_SESSION_ESTABLISHED)));

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

        return nullptr;
    }

    // Reclaim connections abandoned by earlier sessions before we add another. Remotes reconnect
    // from a new ephemeral port, so this is where growth would otherwise happen.
    LOG_IF_FAILED(TransportState::Current().ReapIdleNetworkConnections(m_entryIdentifier));

    if (TransportState::Current().CountNetworkConnectionsForConfigIdentifier(m_entryIdentifier) >= TransportState::Current().TransportSettings.MaxHostConnections)
    {
        // The spec has a reason code for precisely this. Staying silent leaves the client unable
        // to tell a full host from a dead one.
        //
        // The limiter can still suppress this refusal, in which case the invitation goes
        // unanswered and we are outside 6.4. That is the accepted trade documented on
        // MidiNetworkReplyRateLimiter: an unconditional reply is an amplification vector.
        if (args.RemoteAddress() != nullptr)
        {
            auto key = MidiNetworkReplyRateLimiter::MakeRemoteKey(
                std::wstring{ args.RemoteAddress().CanonicalName() },
                std::wstring{ args.RemotePort() });

            if (m_refusalRateLimiter.ShouldSend(key))
            {
                LOG_IF_FAILED(SendUnconnectedBye(
                    args.RemoteAddress(),
                    args.RemotePort(),
                    args.LocalAddress(),
                    MidiNetworkCommandByeReason::CommandByeReasonHostToClient_TooManyOpenSessions,
                    internal::ResourceGetWString(IDS_MESSAGE_MAX_SESSIONS_REACHED)));
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

        return nullptr;
    }

    std::shared_ptr<MidiNetworkConnection> connection{ nullptr };

    auto hr = CreateNetworkConnection(args.RemoteAddress(), args.RemotePort(), args.LocalAddress(), connection);

    if (FAILED(hr) || connection == nullptr)
    {
        TraceLoggingWrite(
            MidiNetworkMidiTransportTelemetryProvider::Provider(),
            MIDI_TRACE_EVENT_ERROR,
            TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
            TraceLoggingLevel(WINEVENT_LEVEL_ERROR),
            TraceLoggingPointer(this, "this"),
            TraceLoggingWideString(L"Message received from remote client, but no connection could be created", MIDI_TRACE_EVENT_MESSAGE_FIELD),
            TraceLoggingHResult(hr, MIDI_TRACE_EVENT_HRESULT_FIELD)
        );

        return nullptr;
    }

    return connection;
}

_Use_decl_annotations_
void
MidiNetworkHost::ReleaseConnectionIfSessionFinished(
    std::shared_ptr<MidiNetworkConnection> const& connection,
    DatagramSocketMessageReceivedEventArgs const& args)
{
    // Safe to remove the entry we are executing on: the caller's shared_ptr keeps the object
    // alive until the receive callback returns.
    if (!connection->IsSessionFinished())
    {
        return;
    }

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

