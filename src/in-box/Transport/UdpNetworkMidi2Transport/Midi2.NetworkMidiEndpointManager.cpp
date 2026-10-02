// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://github.com/microsoft/MIDI/
// ============================================================================


#include "pch.h"
#include "midi2.NetworkMidiTransport.h"
#include "Feature_Servicing_MIDI2PortNamingRework.h"
#include "Feature_Servicing_MIDI2SchedulerV2.h"

using namespace wil;
using namespace Microsoft::WRL;
using namespace Microsoft::WRL::Wrappers;

#define MAX_DEVICE_ID_LEN 200 // size in chars


namespace
{
    uint8_t ClampFallbackMidi1PortCount(_In_ uint8_t const value) noexcept
    {
        return std::clamp(
            value,
            (uint8_t)MIDI_NETWORK_MIDI_FALLBACK_MIDI1_PORT_COUNT_MINIMUM,
            (uint8_t)MIDI_NETWORK_MIDI_FALLBACK_MIDI1_PORT_COUNT_MAXIMUM);
    }

    // A group terminal block descriptor cannot hold a longer string, and the name originates with
    // the remote. Truncating keeps the ports, where refusing the write would leave the endpoint
    // with none at all.
    std::wstring BoundBlockName(_In_ std::wstring const& name) noexcept
    {
        if (name.length() <= MAX_DESCRIPTOR_STRING_LENGTH)
        {
            return name;
        }

        auto bounded = name.substr(0, MAX_DESCRIPTOR_STRING_LENGTH);

        // never leave a lead surrogate without its trail
        if (!bounded.empty() && IS_HIGH_SURROGATE(bounded.back()))
        {
            bounded.pop_back();
        }

        return bounded;
    }
}


_Use_decl_annotations_
HRESULT
CMidi2NetworkMidiEndpointManager::Initialize(
    IMidiDeviceManager* midiDeviceManager,
    IMidiEndpointProtocolManager* midiEndpointProtocolManager
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

    RETURN_HR_IF(E_INVALIDARG, nullptr == midiDeviceManager);

    RETURN_IF_FAILED(midiDeviceManager->QueryInterface(__uuidof(IMidiDeviceManager), (void**)&m_midiDeviceManager));
    RETURN_IF_FAILED(midiEndpointProtocolManager->QueryInterface(__uuidof(IMidiEndpointProtocolManager), (void**)&m_midiProtocolManager));

    m_transportId = TRANSPORT_LAYER_GUID;   // this is needed so MidiSrv can instantiate the correct transport
    m_containerId = m_transportId;                           // we use the transport ID as the container ID for convenience

    RETURN_IF_FAILED(CreateParentDeviceForClients());

    m_initialized = true;

    // Before anything can start a host. The repeats work around the DNS client, as described in
    // MidiNetworkAdvertiser.cpp. A failure costs only the repeats, so the hosts still start.
    LOG_IF_FAILED(m_dnssdAnnouncer.Start(
        std::wstring{ DNS_PTR_SERVICE_TYPE },
        [this](size_t const hostCount, size_t const packetCount, ::WindowsMidiServicesInternal::MidiDnssdAnnouncementResult const& result)
        {
            TraceLoggingWrite(
                MidiNetworkMidiTransportTelemetryProvider::Provider(),
                MIDI_TRACE_EVENT_INFO,
                TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
                TraceLoggingLevel(WINEVENT_LEVEL_INFO),
                TraceLoggingPointer(this, "this"),
                TraceLoggingWideString(L"Repeated the DNS-SD announcement of this PC's hosts", MIDI_TRACE_EVENT_MESSAGE_FIELD),
                TraceLoggingUInt64(hostCount, "hosts"),
                TraceLoggingUInt64(packetCount, "packets"),
                TraceLoggingUInt32(result.IPv4Interfaces, "IPv4 interfaces"),
                TraceLoggingUInt32(result.IPv6Interfaces, "IPv6 interfaces"),
                TraceLoggingInt32(result.LastError, "last error")
            );
        }));

    // start background thread that creates endpoints
    RETURN_IF_FAILED(StartBackgroundEndpointCreator());
    RETURN_IF_FAILED(StartBackgroundNegotiation());
    RETURN_IF_FAILED(StartBackgroundEndpointWorker());

    // A host limited to one adapter has to move when that adapter goes or comes back: Wi-Fi
    // turned off, a USB adapter unplugged, or simply an adapter which comes up after the service
    // has started. Without the notification the next scan still catches it, just later.
    if (!m_networkChangeMonitor.Start(
        [this]() { RequestNetworkAdapterReconcile(); },
        MIDI_NETWORK_ADAPTER_CHANGE_SETTLE_MILLISECONDS))
    {
        TraceLoggingWrite(
            MidiNetworkMidiTransportTelemetryProvider::Provider(),
            MIDI_TRACE_EVENT_WARNING,
            TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
            TraceLoggingLevel(WINEVENT_LEVEL_WARNING),
            TraceLoggingPointer(this, "this"),
            TraceLoggingWideString(L"Unable to watch for network adapter changes. Hosts limited to an adapter will follow it at each scan instead.", MIDI_TRACE_EVENT_MESSAGE_FIELD)
        );
    }

    // start the device watcher so we see new hosts come online
    RETURN_IF_FAILED(StartRemoteHostWatcher());

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
CMidi2NetworkMidiEndpointManager::StartRemoteHostWatcher()
{
    TraceLoggingWrite(
        MidiNetworkMidiTransportTelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_INFO,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this"),
        TraceLoggingWideString(L"Enter", MIDI_TRACE_EVENT_MESSAGE_FIELD)
    );

    // when new remote host is found, track it. We will need to have a complete
    // list of found hosts so we can handle both pre-configured reconnects as
    // well as connect requests that come later

    // Windows.Devices.Enumeration is deliberately not used here. Its DNS-SD watcher never
    // raises Removed, even for a correct TTL 0 goodbye, so this map only ever grew and the
    // service went on inviting devices which had left the network.
    // https://github.com/microsoft/MIDI/issues/1149 and /issues/1003.
    auto const hr = m_browser.Start(
        std::wstring{ DNS_PTR_SERVICE_TYPE },
        [this](::WindowsMidiServicesInternal::MidiDnssdService const& service) { OnAdvertisedHostAdded(service); },
        [this](::WindowsMidiServicesInternal::MidiDnssdService const& service, uint32_t const) { OnAdvertisedHostUpdated(service); },
        [this](std::wstring const& fullName, std::wstring const& deviceId) { OnAdvertisedHostRemoved(fullName, deviceId); });

    RETURN_IF_FAILED(hr);

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

// mDNS is an unbounded source: a remote can advertise arbitrary text of arbitrary length. Values
// which do not meet the specification are dropped rather than returned, so they can never reach a
// comparison, a device property, or an SWD id.
static bool TryGetAdvertisedProductInstanceId(
    _In_ ::WindowsMidiServicesInternal::MidiDnssdService const& service,
    _Out_ std::wstring& value)
{
    value = service.ProductInstanceId();

    if (value.empty())
    {
        return false;
    }

    if (internal::ExceedsUtf8ByteCount(value, MIDI_MAX_UMP_PRODUCT_INSTANCE_ID_BYTE_COUNT) ||
        !internal::ContainsOnlyPrintableAscii(value))
    {
        value.clear();

        return false;
    }

    return true;
}

static bool TryGetAdvertisedEndpointName(
    _In_ ::WindowsMidiServicesInternal::MidiDnssdService const& service,
    _Out_ std::wstring& value)
{
    value = service.UmpEndpointName();

    if (value.empty())
    {
        return false;
    }

    if (internal::ExceedsUtf8ByteCount(value, MIDI_MAX_UMP_ENDPOINT_NAME_BYTE_COUNT))
    {
        value.clear();

        return false;
    }

    return true;
}

// Matches a configured client entry against a discovered host. The mDNS device id is opaque and
// changes between machines, so the advertised Product Instance Id and UMP Endpoint Name are
// accepted too. All comparisons are case-insensitive.
static bool TryFindAdvertisedHost(
    _In_ std::map<std::wstring, ::WindowsMidiServicesInternal::MidiDnssdService> const& advertisedHosts,
    _In_ winrt::hstring const& matchId,
    _Out_ ::WindowsMidiServicesInternal::MidiDnssdService& found)
{
    found = { };

    if (matchId.empty())
    {
        return false;
    }

    auto wanted = internal::ToLowerTrimmedWStringCopy(std::wstring{ matchId });

    for (auto const& entry : advertisedHosts)
    {
        if (internal::ToLowerTrimmedWStringCopy(entry.first) == wanted)
        {
            found = entry.second;

            return true;
        }

        std::wstring value{ };

        if (TryGetAdvertisedProductInstanceId(entry.second, value) &&
            internal::ToLowerTrimmedWStringCopy(value) == wanted)
        {
            found = entry.second;

            return true;
        }

        if (TryGetAdvertisedEndpointName(entry.second, value) &&
            internal::ToLowerTrimmedWStringCopy(value) == wanted)
        {
            found = entry.second;

            return true;
        }
    }

    return false;
}

_Use_decl_annotations_
void
CMidi2NetworkMidiEndpointManager::OnAdvertisedHostAdded(::WindowsMidiServicesInternal::MidiDnssdService const& service)
{
    // TODO: Search our host entries to make sure the host is not *this* host

    std::wstring advertisedEndpointName{ };
    std::wstring advertisedProductInstanceId{ };

    TryGetAdvertisedEndpointName(service, advertisedEndpointName);
    TryGetAdvertisedProductInstanceId(service, advertisedProductInstanceId);

    TraceLoggingWrite(
        MidiNetworkMidiTransportTelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_INFO,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this"),
        TraceLoggingWideString(L"Discovered advertised host", MIDI_TRACE_EVENT_MESSAGE_FIELD),
        TraceLoggingWideString(service.DeviceId().c_str(), "id"),
        TraceLoggingWideString(advertisedEndpointName.c_str(), "UMP endpoint name"),
        TraceLoggingWideString(advertisedProductInstanceId.c_str(), "product instance id")
    );

    {
        auto lock = m_advertisedHostsLock.lock_exclusive();

        m_foundAdvertisedHosts.insert_or_assign(service.DeviceId(), service);
    }

    WakeupBackgroundEndpointCreatorThread();
}

// The address or port can change while a host stays put, so the stored copy is refreshed. A
// configured client which is already connected is left alone; the protocol handles that.
_Use_decl_annotations_
void
CMidi2NetworkMidiEndpointManager::OnAdvertisedHostUpdated(::WindowsMidiServicesInternal::MidiDnssdService const& service)
{
    auto lock = m_advertisedHostsLock.lock_exclusive();

    m_foundAdvertisedHosts.insert_or_assign(service.DeviceId(), service);
}

_Use_decl_annotations_
void
CMidi2NetworkMidiEndpointManager::OnAdvertisedHostRemoved(std::wstring const& fullName, std::wstring const& deviceId)
{
    TraceLoggingWrite(
        MidiNetworkMidiTransportTelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_INFO,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this"),
        TraceLoggingWideString(L"Advertised host went away", MIDI_TRACE_EVENT_MESSAGE_FIELD),
        TraceLoggingWideString(fullName.c_str(), "full name"),
        TraceLoggingWideString(deviceId.c_str(), "id")
    );

    auto lock = m_advertisedHostsLock.lock_exclusive();

    // we don't disconnect or anything here. That's handled in-protocol.
    m_foundAdvertisedHosts.erase(deviceId);
}

HRESULT
CMidi2NetworkMidiEndpointManager::WakeupBackgroundEndpointCreatorThread()
{
    TraceLoggingWrite(
        MidiNetworkMidiTransportTelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_INFO,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this"),
        TraceLoggingWideString(L"Enter", MIDI_TRACE_EVENT_MESSAGE_FIELD)
    );

    m_backgroundEndpointCreatorThreadWakeup.SetEvent();

    return S_OK;
}

HRESULT
CMidi2NetworkMidiEndpointManager::WakeupBackgroundNegotiationThread()
{
    m_backgroundNegotiationThreadWakeup.SetEvent();

    return S_OK;
}

HRESULT
CMidi2NetworkMidiEndpointManager::StartBackgroundNegotiation()
{
    m_backgroundNegotiationThread = std::jthread(std::bind_front(&CMidi2NetworkMidiEndpointManager::NegotiationWorker, this));

    return S_OK;
}

HRESULT
CMidi2NetworkMidiEndpointManager::StartBackgroundEndpointWorker()
{
    {
        auto lock = m_endpointWorkLock.lock();

        m_endpointWorkerAcceptingWork = true;
    }

    m_endpointWorkerThread = std::jthread(std::bind_front(&CMidi2NetworkMidiEndpointManager::EndpointWorker, this));

    return S_OK;
}

// Defined with endpoint creation, below
static std::wstring BuildEndpointDeviceInstanceId(
    _In_ std::wstring const& endpointName,
    _In_ std::wstring const& productInstanceId);

_Use_decl_annotations_
HRESULT
CMidi2NetworkMidiEndpointManager::QueueEndpointWork(EndpointWorkItem item)
try
{
    auto const kind = item.Kind;
    size_t queueDepth{ 0 };

    {
        auto lock = m_endpointWorkLock.lock();

        if (!m_endpointWorkerAcceptingWork)
        {
            return E_ABORT;
        }

        m_endpointWork.push_back(std::move(item));

        queueDepth = m_endpointWork.size();
    }

    m_endpointWorkerWakeup.SetEvent();

    TraceLoggingWrite(
        MidiNetworkMidiTransportTelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_INFO,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this"),
        TraceLoggingWideString(L"Queued endpoint work", MIDI_TRACE_EVENT_MESSAGE_FIELD),
        TraceLoggingInt32(static_cast<int32_t>(kind), "kind"),
        TraceLoggingUInt64(static_cast<uint64_t>(queueDepth), "queue depth")
    );

    return S_OK;
}
CATCH_RETURN()

_Use_decl_annotations_
HRESULT
CMidi2NetworkMidiEndpointManager::QueueHostEndpointCreation(
    std::shared_ptr<MidiNetworkHostConnection> connection,
    std::wstring const& clientUmpEndpointName,
    std::wstring const& clientProductInstanceId
)
try
{
    RETURN_HR_IF_NULL(E_INVALIDARG, connection);

    EndpointWorkItem item{ };
    item.Kind = EndpointWorkKind::CreateHostEndpoint;
    item.Connection = connection;
    item.RemoteEndpointName = clientUmpEndpointName;
    item.RemoteProductInstanceId = clientProductInstanceId;

    if (!clientUmpEndpointName.empty() && !clientProductInstanceId.empty())
    {
        item.DeviceInstanceId = BuildEndpointDeviceInstanceId(clientUmpEndpointName, clientProductInstanceId);
    }

    return QueueEndpointWork(std::move(item));
}
CATCH_RETURN()

_Use_decl_annotations_
HRESULT
CMidi2NetworkMidiEndpointManager::QueueClientEndpointCreation(
    std::shared_ptr<MidiNetworkClientConnection> connection,
    std::wstring const& remoteHostUmpEndpointName,
    std::wstring const& remoteHostProductInstanceId
)
try
{
    RETURN_HR_IF_NULL(E_INVALIDARG, connection);

    EndpointWorkItem item{ };
    item.Kind = EndpointWorkKind::CreateClientEndpoint;
    item.Connection = connection;
    item.RemoteEndpointName = remoteHostUmpEndpointName;
    item.RemoteProductInstanceId = remoteHostProductInstanceId;

    if (!remoteHostUmpEndpointName.empty() && !remoteHostProductInstanceId.empty())
    {
        item.DeviceInstanceId = BuildEndpointDeviceInstanceId(remoteHostUmpEndpointName, remoteHostProductInstanceId);
    }

    return QueueEndpointWork(std::move(item));
}
CATCH_RETURN()

_Use_decl_annotations_
HRESULT
CMidi2NetworkMidiEndpointManager::QueueConnectionShutdown(
    std::shared_ptr<MidiNetworkConnection> connection
)
try
{
    RETURN_HR_IF_NULL(E_INVALIDARG, connection);

    EndpointWorkItem item{ };
    item.Kind = EndpointWorkKind::ShutdownConnection;
    item.Connection = connection;

    if (SUCCEEDED(QueueEndpointWork(std::move(item))))
    {
        return S_OK;
    }

    // Nothing will take it from the queue. That only happens once the service is stopping.
    return connection->Shutdown();
}
CATCH_RETURN()

_Use_decl_annotations_
HRESULT
CMidi2NetworkMidiEndpointManager::RemoveEndpointForSession(
    std::wstring const& deviceInstanceId
)
try
{
    RETURN_HR_IF(E_INVALIDARG, deviceInstanceId.empty());

    if (::GetCurrentThreadId() != m_endpointWorkerThreadId)
    {
        EndpointWorkItem item{ };
        item.Kind = EndpointWorkKind::RemoveEndpoint;
        item.DeviceInstanceId = internal::NormalizeDeviceInstanceIdWStringCopy(deviceInstanceId);

        if (SUCCEEDED(QueueEndpointWork(std::move(item))))
        {
            return S_OK;
        }

        // Nothing will take it from the queue. That only happens once the service is stopping.
    }

    return DeleteEndpoint(deviceInstanceId);
}
CATCH_RETURN()

_Use_decl_annotations_
bool
CMidi2NetworkMidiEndpointManager::TryTakeNextEndpointWork(EndpointWorkItem& item)
{
    auto lock = m_endpointWorkLock.lock();

    if (m_endpointWork.empty())
    {
        return false;
    }

    auto next = std::find_if(
        m_endpointWork.begin(),
        m_endpointWork.end(),
        [](EndpointWorkItem const& queued)
        {
            return queued.Kind == EndpointWorkKind::CreateHostEndpoint ||
                queued.Kind == EndpointWorkKind::CreateClientEndpoint;
        });

    if (next == m_endpointWork.end())
    {
        next = m_endpointWork.begin();
    }
    else if (!next->DeviceInstanceId.empty())
    {
        auto const& creationInstanceId = next->DeviceInstanceId;

        // The same remote's previous endpoint, which has to be gone before this one can exist
        auto removal = std::find_if(
            m_endpointWork.begin(),
            m_endpointWork.end(),
            [&creationInstanceId](EndpointWorkItem const& queued)
            {
                return queued.Kind == EndpointWorkKind::RemoveEndpoint &&
                    queued.DeviceInstanceId == creationInstanceId;
            });

        if (removal != m_endpointWork.end())
        {
            next = removal;
        }
    }

    item = std::move(*next);
    m_endpointWork.erase(next);

    return true;
}

_Use_decl_annotations_
void
CMidi2NetworkMidiEndpointManager::RunEndpointWork(EndpointWorkItem const& item, bool const workerStopping)
{
    // One bad item must not take the rest of the queue with it, and a creation which throws still
    // owes the remote an answer.
    try
    {
        // An endpoint is visible to apps before its creation returns. Until a session claims it,
        // an app which opens it early is attached to its connection through this.
        auto const openCreationWindow = [&item]()
            {
                LOG_IF_FAILED(TransportState::Current().RegisterEndpointBeingCreated(item.DeviceInstanceId, item.Connection));
                item.Connection->BeginEndpointCreation();

                return wil::scope_exit([&item]()
                    {
                        item.Connection->EndEndpointCreation();
                        TransportState::Current().UnregisterEndpointBeingCreated(item.DeviceInstanceId, item.Connection.get());
                    });
            };

        switch (item.Kind)
        {
        case EndpointWorkKind::CreateHostEndpoint:
        {
            auto connection = std::static_pointer_cast<MidiNetworkHostConnection>(item.Connection);

            if (workerStopping)
            {
                LOG_IF_FAILED(connection->FailHostSessionEndpointCreation(E_ABORT));
                break;
            }

            // The remote said Bye while this sat in the queue. Creating the endpoint now would
            // be immediately undone, and that teardown competes with the rest of the queue.
            if (connection->IsHostEndpointCreationAbandoned())
            {
                connection->CancelPendingHostEndpointCreation();

                TraceLoggingWrite(
                    MidiNetworkMidiTransportTelemetryProvider::Provider(),
                    MIDI_TRACE_EVENT_INFO,
                    TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
                    TraceLoggingLevel(WINEVENT_LEVEL_INFO),
                    TraceLoggingPointer(this, "this"),
                    TraceLoggingWideString(L"Skipped host endpoint creation, remote already said Bye", MIDI_TRACE_EVENT_MESSAGE_FIELD)
                );

                break;
            }

            auto creationWindow = openCreationWindow();

            std::wstring newDeviceInstanceId{ };
            std::wstring newEndpointDeviceInterfaceId{ };

            auto const queuedMilliseconds = std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::steady_clock::now() - item.QueuedAt).count();

            auto const activationStarted = std::chrono::steady_clock::now();

            auto const hr = connection->CreateHostEndpointForPendingInvitation(
                item.RemoteEndpointName,
                item.RemoteProductInstanceId,
                newDeviceInstanceId,
                newEndpointDeviceInterfaceId);

            auto const activationMilliseconds = std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::steady_clock::now() - activationStarted).count();

            TraceLoggingWrite(
                MidiNetworkMidiTransportTelemetryProvider::Provider(),
                MIDI_TRACE_EVENT_INFO,
                TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
                TraceLoggingLevel(WINEVENT_LEVEL_INFO),
                TraceLoggingPointer(this, "this"),
                TraceLoggingWideString(L"Host endpoint creation completed", MIDI_TRACE_EVENT_MESSAGE_FIELD),
                TraceLoggingInt64(static_cast<int64_t>(queuedMilliseconds), "queued ms"),
                TraceLoggingInt64(static_cast<int64_t>(activationMilliseconds), "activation ms"),
                TraceLoggingHResult(hr, "hresult")
            );

            if (SUCCEEDED(hr))
            {
                LOG_IF_FAILED(connection->CompleteHostSessionAfterEndpointCreated(newDeviceInstanceId, newEndpointDeviceInterfaceId));
            }
            else
            {
                LOG_IF_FAILED(connection->FailHostSessionEndpointCreation(hr));
            }

            break;
        }

        case EndpointWorkKind::CreateClientEndpoint:
        {
            auto connection = std::static_pointer_cast<MidiNetworkClientConnection>(item.Connection);

            if (workerStopping)
            {
                LOG_IF_FAILED(connection->FailClientSessionEndpointCreation(E_ABORT));
                break;
            }

            // The host said Bye, or the session timed out, while this sat in the queue
            if (!connection->IsSessionActive())
            {
                TraceLoggingWrite(
                    MidiNetworkMidiTransportTelemetryProvider::Provider(),
                    MIDI_TRACE_EVENT_INFO,
                    TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
                    TraceLoggingLevel(WINEVENT_LEVEL_INFO),
                    TraceLoggingPointer(this, "this"),
                    TraceLoggingWideString(L"Skipped client endpoint creation, session already ended", MIDI_TRACE_EVENT_MESSAGE_FIELD)
                );

                break;
            }

            auto creationWindow = openCreationWindow();

            std::wstring newDeviceInstanceId{ };
            std::wstring newEndpointDeviceInterfaceId{ };

            auto const hr = connection->CreateClientEndpointForAcceptedInvitation(
                item.RemoteEndpointName,
                item.RemoteProductInstanceId,
                newDeviceInstanceId,
                newEndpointDeviceInterfaceId);

            if (SUCCEEDED(hr))
            {
                LOG_IF_FAILED(connection->CompleteClientSessionAfterEndpointCreated(newDeviceInstanceId, newEndpointDeviceInterfaceId));
            }
            else
            {
                LOG_IF_FAILED(connection->FailClientSessionEndpointCreation(hr));
            }

            break;
        }

        case EndpointWorkKind::RemoveEndpoint:
            LOG_IF_FAILED(DeleteEndpoint(item.DeviceInstanceId));
            break;

        case EndpointWorkKind::ShutdownConnection:
            LOG_IF_FAILED(item.Connection->Shutdown());
            break;
        }
    }
    catch (...)
    {
        auto const hr = wil::ResultFromCaughtException();

        TraceLoggingWrite(
            MidiNetworkMidiTransportTelemetryProvider::Provider(),
            MIDI_TRACE_EVENT_ERROR,
            TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
            TraceLoggingLevel(WINEVENT_LEVEL_ERROR),
            TraceLoggingPointer(this, "this"),
            TraceLoggingWideString(L"Exception running endpoint work. Continuing.", MIDI_TRACE_EVENT_MESSAGE_FIELD),
            TraceLoggingInt32(static_cast<int32_t>(item.Kind), "kind"),
            TraceLoggingHResult(hr, MIDI_TRACE_EVENT_HRESULT_FIELD)
        );

        // Guarded again, because the drain at exit has nothing outside it to catch an exception
        try
        {
            if (item.Kind == EndpointWorkKind::CreateHostEndpoint)
            {
                LOG_IF_FAILED(std::static_pointer_cast<MidiNetworkHostConnection>(item.Connection)->FailHostSessionEndpointCreation(hr));
            }
            else if (item.Kind == EndpointWorkKind::CreateClientEndpoint)
            {
                LOG_IF_FAILED(std::static_pointer_cast<MidiNetworkClientConnection>(item.Connection)->FailClientSessionEndpointCreation(hr));
            }
        }
        CATCH_LOG();
    }
}

_Use_decl_annotations_
HRESULT
CMidi2NetworkMidiEndpointManager::EndpointWorker(std::stop_token stopToken)
{
    // Endpoint activation and removal are COM calls into the service, and shutting a connection
    // down writes a Bye through the WinRT socket. Without an apartment they fail with
    // CO_E_NOTINITIALIZED and endpoints are never released.
    winrt::init_apartment();

    m_endpointWorkerThreadId = ::GetCurrentThreadId();

    auto nextLatencyRefresh = std::chrono::steady_clock::now();

    while (!stopToken.stop_requested())
    {
        try
        {
            // Before the queue is read, so work queued while it drains still ends the wait below
            m_endpointWorkerWakeup.ResetEvent();

            EndpointWorkItem item{ };

            while (!stopToken.stop_requested() && TryTakeNextEndpointWork(item))
            {
                RunEndpointWork(item, false);
            }

            DWORD waitMilliseconds{ INFINITE };

            if (Feature_Servicing_MIDI2SchedulerV2::IsEnabled())
            {
                auto const now = std::chrono::steady_clock::now();

                if (now >= nextLatencyRefresh)
                {
                    RefreshCalculatedLatencyProperties();

                    nextLatencyRefresh = now + std::chrono::milliseconds(MIDI_NETWORK_LATENCY_REFRESH_INTERVAL_MILLISECONDS);
                }

                auto const untilRefresh = std::chrono::duration_cast<std::chrono::milliseconds>(
                    nextLatencyRefresh - std::chrono::steady_clock::now()).count();

                waitMilliseconds = untilRefresh > 0 ? static_cast<DWORD>(untilRefresh) : 0;
            }

            if (!stopToken.stop_requested())
            {
                m_endpointWorkerWakeup.wait(waitMilliseconds);
            }
        }
        // One bad entry must not end the worker. An exception leaving this thread would
        // terminate the service, and returning would leave nothing creating endpoints again.
        catch (...)
        {
            TraceLoggingWrite(
                MidiNetworkMidiTransportTelemetryProvider::Provider(),
                MIDI_TRACE_EVENT_ERROR,
                TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
                TraceLoggingLevel(WINEVENT_LEVEL_ERROR),
                TraceLoggingPointer(this, "this"),
                TraceLoggingWideString(L"Exception in worker iteration. Continuing.", MIDI_TRACE_EVENT_MESSAGE_FIELD)
            );
        }
    }

    // Anything already queued is finished here. Removals and shutdowns still run, and each
    // creation is refused, because the remote was promised an answer.
    std::vector<EndpointWorkItem> remaining{ };

    {
        auto lock = m_endpointWorkLock.lock();

        m_endpointWorkerAcceptingWork = false;

        remaining.swap(m_endpointWork);
    }

    for (auto const& item : remaining)
    {
        RunEndpointWork(item, true);
    }

    m_endpointWorkerThreadId = 0;

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
CMidi2NetworkMidiEndpointManager::NegotiationWorker(std::stop_token stopToken)
{
    // DiscoverAndNegotiate is a COM call into the service
    winrt::init_apartment();

    while (!stopToken.stop_requested())
    {
        try
        {
            if (m_backgroundNegotiationThreadWakeup.is_signaled())
            {
                m_backgroundNegotiationThreadWakeup.ResetEvent();
            }

            std::vector<std::wstring> negotiations;

            {
                auto lock = m_pendingNegotiationsLock.lock();
                negotiations.swap(m_pendingNegotiations);
            }

            for (auto const& endpointDeviceInterfaceId : negotiations)
            {
                if (stopToken.stop_requested())
                {
                    break;
                }

                LOG_IF_FAILED(InitiateDiscoveryAndNegotiation(endpointDeviceInterfaceId));
            }

            if (!stopToken.stop_requested())
            {
                m_backgroundNegotiationThreadWakeup.wait();
            }
        }
        // One bad entry must not end the worker. An exception leaving this thread would
        // terminate the service, and returning would leave nothing creating endpoints again.
        catch (...)
        {
            TraceLoggingWrite(
                MidiNetworkMidiTransportTelemetryProvider::Provider(),
                MIDI_TRACE_EVENT_ERROR,
                TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
                TraceLoggingLevel(WINEVENT_LEVEL_ERROR),
                TraceLoggingPointer(this, "this"),
                TraceLoggingWideString(L"Exception in worker iteration. Continuing.", MIDI_TRACE_EVENT_MESSAGE_FIELD)
            );
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

    m_negotiationThreadExitedEvent.SetEvent();

    return S_OK;
}

// Ping round trip is the only measured latency figure the transport has. Half of it is the mean
// one-way delay to the remote, which is what the outbound scheduler needs to compensate for. The
// scheduler reads this at connection time, so refreshing it periodically is enough.
void
CMidi2NetworkMidiEndpointManager::RefreshCalculatedLatencyProperties()
{
    if (!Feature_Servicing_MIDI2SchedulerV2::IsEnabled())
    {
        return;
    }

    auto const writeLatency = [&](std::wstring const& endpointDeviceId, uint64_t const roundTripTicks)
        {
            if (endpointDeviceId.empty() || roundTripTicks == 0)
            {
                return;
            }

            uint64_t oneWayTicks = roundTripTicks / 2;

            {
                auto lock = m_lastWrittenLatencyTicksLock.lock();

                auto const existing = m_lastWrittenLatencyTicks.find(endpointDeviceId);

                // Device property writes are not free and this runs on a timer, so only write on a
                // change big enough to matter to a musician.
                if (existing != m_lastWrittenLatencyTicks.end())
                {
                    auto const previous = existing->second;
                    auto const difference = (oneWayTicks > previous) ? (oneWayTicks - previous) : (previous - oneWayTicks);

                    if (difference < m_latencyWriteThresholdTicks)
                    {
                        return;
                    }
                }
            }

            DEVPROPERTY props[] =
            {
                { { PKEY_MIDI_MidiOutCalculatedLatencyTicks, DEVPROP_STORE_SYSTEM, nullptr },
                  DEVPROP_TYPE_UINT64, static_cast<ULONG>(sizeof(uint64_t)), (PVOID)&oneWayTicks },
            };

            if (SUCCEEDED(m_midiDeviceManager->UpdateEndpointProperties(endpointDeviceId.c_str(), ARRAYSIZE(props), props)))
            {
                auto lock = m_lastWrittenLatencyTicksLock.lock();

                m_lastWrittenLatencyTicks[endpointDeviceId] = oneWayTicks;
            }
        };

    try
    {
        for (auto const& client : TransportState::Current().GetClients())
        {
            if (client == nullptr) continue;

            writeLatency(client->GetEndpointDeviceId(), client->PeekAverageLatencyTicks());
        }

        for (auto const& host : TransportState::Current().GetHosts())
        {
            if (host == nullptr) continue;

            for (auto const& connection : TransportState::Current().GetHostConnectionsForHost(host->EntryIdentifier()))
            {
                if (connection == nullptr) continue;

                writeLatency(connection->GetEndpointDeviceId(), connection->PeekAverageLatencyTicks());
            }
        }
    }
    CATCH_LOG();
}

HRESULT
CMidi2NetworkMidiEndpointManager::StartBackgroundEndpointCreator()
{
    TraceLoggingWrite(
        MidiNetworkMidiTransportTelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_INFO,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this"),
        TraceLoggingWideString(L"Enter", MIDI_TRACE_EVENT_MESSAGE_FIELD)
    );


    m_backgroundEndpointCreatorThread = std::jthread(std::bind_front(&CMidi2NetworkMidiEndpointManager::EndpointCreatorWorker, this));


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

// True when this machine has at least one usable IP address. On a laptop with wifi off and
// nothing plugged in there is nothing for a client to reach, and every configured client would
// otherwise pay a connect timeout each scan.
static bool IsNetworkAvailable()
{
    try
    {
        for (auto const& host : winrt::Windows::Networking::Connectivity::NetworkInformation::GetHostNames())
        {
            // machine and domain names carry no IP information
            if (host.IPInformation() == nullptr) continue;

            auto type = host.Type();

            if (type == HostNameType::Ipv4 || type == HostNameType::Ipv6)
            {
                return true;
            }
        }
    }
    CATCH_LOG();

    return false;
}

_Use_decl_annotations_
HRESULT
CMidi2NetworkMidiEndpointManager::StartNewClient(
    MidiNetworkClientDefinition const& clientDefinition, 
    winrt::hstring const& hostNameOrIPAddress, 
    uint16_t const hostPort)
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
            TraceLoggingWideString(L"Enter", MIDI_TRACE_EVENT_MESSAGE_FIELD),
            TraceLoggingWideString(hostNameOrIPAddress.c_str(), "host name or ip"),
            TraceLoggingUInt16(hostPort, "host port")
            );

        if (!IsNetworkAvailable())
        {
            TraceLoggingWrite(
                MidiNetworkMidiTransportTelemetryProvider::Provider(),
                MIDI_TRACE_EVENT_INFO,
                TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
                TraceLoggingLevel(WINEVENT_LEVEL_INFO),
                TraceLoggingPointer(this, "this"),
                TraceLoggingWideString(L"No network is available. Skipping this client until one is.", MIDI_TRACE_EVENT_MESSAGE_FIELD),
                TraceLoggingWideString(hostNameOrIPAddress.c_str(), "host name or ip")
            );

            // Not an error, and the definition is deliberately left uncreated so the next scan
            // retries it once the machine is back on a network.
            return S_FALSE;
        }

        // reserve() does not change size(), so the name has to be sized before it is written into
        // and resized to the returned length afterward. Otherwise every read of it sees an empty
        // string and the generated names degrade to "-midisrv".
        DWORD nameLen = MAX_COMPUTERNAME_LENGTH + 1;
        std::wstring machineName;
        machineName.resize(nameLen);

        std::wstring root;

        if (GetComputerName(machineName.data(), &nameLen))
        {
            machineName.resize(nameLen);

            root = internal::ToLowerTrimmedWStringCopy(machineName) + L"-midisrv";
        }
        else
        {
            root = L"windows-midisrv";
        }

        // The local identity is filled in on this copy only. The stored definition keeps what
        // was configured, so a changed computer name is picked up on the next connection.
        auto definition = clientDefinition;

        if (definition.LocalProductInstanceId.empty())
        {
            // shared with the hosts, so a remote sees one identity for this PC in either role
            definition.LocalProductInstanceId = TransportState::Current().GetEffectiveProductInstanceId();
        }

        if (definition.LocalEndpointName.empty())
        {
            definition.LocalEndpointName = root;
        }


        auto client = std::make_shared<MidiNetworkClient>();
        RETURN_IF_NULL_ALLOC(client);

        // A reconnect still has the previous client registered. Left in place it keeps its socket
        // and threads, and lookups by entry identifier find the dead one.
        auto previousClient = TransportState::Current().GetClient(definition.EntryIdentifier);

        if (previousClient != nullptr)
        {
            LOG_IF_FAILED(previousClient->Shutdown());
            LOG_IF_FAILED(TransportState::Current().RemoveLiveClient(definition.EntryIdentifier));
        }

        auto initHr = client->Initialize(definition);
        RETURN_IF_FAILED(initHr);

        // != 0 for the hostPort is hacky, but for MIDI, we shouldn't expect ports < 1024 anyway
        if (!hostNameOrIPAddress.empty() && hostPort != 0)
        {
            HostName hostName(hostNameOrIPAddress);
            winrt::hstring portNumberString = winrt::to_hstring(hostPort);

            auto startHr = client->Start(hostName, portNumberString);
            RETURN_IF_FAILED(startHr);

            // Building a client means an invitation, a reply and then endpoint creation, and the
            // entry can be removed while that is in flight. Registering it anyway leaves a client
            // no caller can see or disconnect, still holding its socket and its MIDI endpoint.
            if (!TransportState::Current().AddClientIfStillPending(client, definition.EntryIdentifier))
            {
                TraceLoggingWrite(
                    MidiNetworkMidiTransportTelemetryProvider::Provider(),
                    MIDI_TRACE_EVENT_INFO,
                    TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
                    TraceLoggingLevel(WINEVENT_LEVEL_INFO),
                    TraceLoggingPointer(this, "this"),
                    TraceLoggingWideString(L"Client entry was removed while the client was being created. Shutting it back down.", MIDI_TRACE_EVENT_MESSAGE_FIELD),
                    TraceLoggingGuid(definition.EntryIdentifier, "entry identifier")
                );

                LOG_IF_FAILED(client->Shutdown());

                return S_OK;
            }

            LOG_IF_FAILED(TransportState::Current().MarkClientDefinitionLive(definition.EntryIdentifier));

            return S_OK;
        }

        return E_FAIL;
    }
    CATCH_RETURN()
}


_Use_decl_annotations_
HRESULT
CMidi2NetworkMidiEndpointManager::EndpointCreatorWorker(std::stop_token stopToken)
{
    TraceLoggingWrite(
        MidiNetworkMidiTransportTelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_INFO,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this"),
        TraceLoggingWideString(L"Enter", MIDI_TRACE_EVENT_MESSAGE_FIELD)
    );

    winrt::init_apartment();

    // this is set up to run through one time before waiting for the wakeup
    // this way we can process anything added before the EndpointManager has been
    // initialized

    while (!stopToken.stop_requested())
    {
        try
        {
            TraceLoggingWrite(
                MidiNetworkMidiTransportTelemetryProvider::Provider(),
                MIDI_TRACE_EVENT_INFO,
                TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
                TraceLoggingLevel(WINEVENT_LEVEL_INFO),
                TraceLoggingPointer(this, "this"),
                TraceLoggingWideString(L"Background worker loop", MIDI_TRACE_EVENT_MESSAGE_FIELD)
            );

            if (m_backgroundEndpointCreatorThreadWakeup.is_signaled())
            {
                m_backgroundEndpointCreatorThreadWakeup.ResetEvent();
            }

            // Negotiation runs on its own thread. It calls into the service and can block there
            // behind a PnP notification, which used to stop this loop creating any client at all.
            StartPendingHosts();
            StartPendingClients();

            ReconcileHostNetworkAdapters();

            // wait for notification of new hosts online or new entries added via config
            // the most time we wait is the DirectConnectionScanInterval
            m_backgroundEndpointCreatorThreadWakeup.wait(TransportState::Current().TransportSettings.DirectConnectionScanInterval);
        }
        // One bad entry must not end the worker. An exception leaving this thread would
        // terminate the service, and returning would leave nothing creating endpoints again.
        catch (...)
        {
            TraceLoggingWrite(
                MidiNetworkMidiTransportTelemetryProvider::Provider(),
                MIDI_TRACE_EVENT_ERROR,
                TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
                TraceLoggingLevel(WINEVENT_LEVEL_ERROR),
                TraceLoggingPointer(this, "this"),
                TraceLoggingWideString(L"Exception in worker iteration. Continuing.", MIDI_TRACE_EVENT_MESSAGE_FIELD)
            );
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

void
CMidi2NetworkMidiEndpointManager::StartPendingHosts()
{
    for (auto const& definition : TransportState::Current().GetHostDefinitions())
    {
        if (definition.State != MidiNetworkEntryState::Pending || !definition.IsEnabled)
        {
            continue;
        }

        auto host = std::make_shared<MidiNetworkHost>();

        auto initializeResult = host->Initialize(definition);

        if (FAILED(initializeResult))
        {
            LOG_IF_FAILED(initializeResult);

            TraceLoggingWrite(
                MidiNetworkMidiTransportTelemetryProvider::Provider(),
                MIDI_TRACE_EVENT_ERROR,
                TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
                TraceLoggingLevel(WINEVENT_LEVEL_ERROR),
                TraceLoggingPointer(this, "this"),
                TraceLoggingWideString(L"Host definition rejected during initialization. Host not started.", MIDI_TRACE_EVENT_MESSAGE_FIELD),
                TraceLoggingGuid(definition.EntryIdentifier, "entry identifier"),
                TraceLoggingWideString(definition.UmpEndpointName.c_str(), "name"),
                TraceLoggingHResult(initializeResult, MIDI_TRACE_EVENT_HRESULT_FIELD)
            );

            // Initialize assigns the definition last, so a rejected host has an empty one.
            // Starting it anyway bound a socket and published a nameless host that no
            // caller could identify or remove.
            LOG_IF_FAILED(TransportState::Current().MarkHostDefinitionFailed(definition.EntryIdentifier));

            continue;
        }

        if (!host->HasStarted())
        {
            LOG_IF_FAILED(host->Start());
        }

        LOG_IF_FAILED(TransportState::Current().MarkHostDefinitionLive(definition.EntryIdentifier));

        // The definition can be removed while the host above is being built, so
        // registration is conditional on it still being there. Losing that race means
        // this host is unreachable and must not be left holding a socket and a service
        // instance name.
        if (!TransportState::Current().AddHostIfStillPending(host, definition.EntryIdentifier))
        {
            TraceLoggingWrite(
                MidiNetworkMidiTransportTelemetryProvider::Provider(),
                MIDI_TRACE_EVENT_INFO,
                TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
                TraceLoggingLevel(WINEVENT_LEVEL_INFO),
                TraceLoggingPointer(this, "this"),
                TraceLoggingWideString(L"Host entry was removed while the host was being created. Shutting it back down.", MIDI_TRACE_EVENT_MESSAGE_FIELD),
                TraceLoggingGuid(definition.EntryIdentifier, "entry identifier")
            );

            LOG_IF_FAILED(host->Shutdown());
        }
    }
}

void
CMidi2NetworkMidiEndpointManager::ReconcileHostNetworkAdapters()
{
    auto const requested = m_networkAdapterReconcileRequested.exchange(false);

    auto const hosts = TransportState::Current().GetHosts();

    // Every scan also looks again at a host which is waiting or has fallen back, in case a
    // change came and went without a notification. A host which is where it should be costs
    // nothing until something changes.
    bool const anyAway = std::any_of(hosts.begin(), hosts.end(), [](std::shared_ptr<MidiNetworkHost> const& host)
        {
            return host != nullptr && (host->IsWaitingForNetworkAdapter() || host->NetworkAdapterFallbackUsed());
        });

    if (!requested && !anyAway)
    {
        return;
    }

    auto const adapters = ::WindowsMidiServicesInternal::GetMidiNetworkAdapters();

    for (auto const& host : hosts)
    {
        if (host == nullptr) continue;

        LOG_IF_FAILED(host->ReconcileNetworkAdapter(adapters));
    }
}

void
CMidi2NetworkMidiEndpointManager::RequestNetworkAdapterReconcile() noexcept
{
    m_networkAdapterReconcileRequested = true;

    LOG_IF_FAILED(WakeupBackgroundEndpointCreatorThread());
}

// Client definitions aren't clients. They are what is needed to connect to a host once it can be
// reached, so each pass only connects the ones whose remote can be found.
void
CMidi2NetworkMidiEndpointManager::StartPendingClients()
{
    for (auto const& definition : TransportState::Current().GetClientDefinitions())
    {
        if (definition.State != MidiNetworkEntryState::Pending || !definition.Enabled)
        {
            continue;
        }

        winrt::hstring hostNameOrIPAddress{ };
        uint16_t port{ 0 };

        if (TryResolveClientTarget(definition, hostNameOrIPAddress, port))
        {
            LOG_IF_FAILED(StartNewClient(definition, hostNameOrIPAddress, port));
        }
    }
}

_Use_decl_annotations_
bool
CMidi2NetworkMidiEndpointManager::TryResolveClientTarget(
    MidiNetworkClientDefinition const& definition,
    winrt::hstring& hostNameOrIPAddress,
    uint16_t& port)
{
    hostNameOrIPAddress = winrt::hstring{ };
    port = 0;

    // --- connect via mDNS entry
    if (!definition.MatchId.empty() ||
        !definition.MatchProductInstanceId.empty() ||
        !definition.MatchUmpEndpointName.empty())
    {
        ::WindowsMidiServicesInternal::MidiDnssdService advertisedHost{ };

        // The device id first, then the device's own identity. A responder renames a
        // colliding DNS-SD instance label and a user or firmware update can change it, so
        // the id alone would silently stop matching a device which is still right there.
        bool found{ false };

        {
            auto lock = m_advertisedHostsLock.lock_shared();

            found = TryFindAdvertisedHost(m_foundAdvertisedHosts, definition.MatchId, advertisedHost);

            if (!found)
            {
                found = TryFindAdvertisedHost(m_foundAdvertisedHosts, definition.MatchProductInstanceId, advertisedHost);
            }

            if (!found)
            {
                found = TryFindAdvertisedHost(m_foundAdvertisedHosts, definition.MatchUmpEndpointName, advertisedHost);
            }
        }

        if (!found)
        {
            return false;
        }

        TraceLoggingWrite(
            MidiNetworkMidiTransportTelemetryProvider::Provider(),
            MIDI_TRACE_EVENT_INFO,
            TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
            TraceLoggingLevel(WINEVENT_LEVEL_INFO),
            TraceLoggingPointer(this, "this"),
            TraceLoggingWideString(L"Processing mdns entry", MIDI_TRACE_EVENT_MESSAGE_FIELD),
            TraceLoggingWideString(advertisedHost.DeviceId().c_str(), "id")
        );

        // IP address first, as that is the most reliable. The host name relies on
        // DNS being set up properly, which is often not the case on a network with
        // just some devices and a laptop.
        if (!advertisedHost.IPv4Addresses.empty())
        {
            // we only take the top one right now. We should take the others as well
            hostNameOrIPAddress = winrt::hstring{ advertisedHost.IPv4Addresses.front() };
        }
        else if (!advertisedHost.IPv6Addresses.empty())
        {
            // A routable address first. A link-local one, in fe80::/10, carries the %scope of the
            // adapter it was seen on, which is what makes it reachable at all.
            auto const routable = std::find_if(advertisedHost.IPv6Addresses.begin(), advertisedHost.IPv6Addresses.end(),
                [](std::wstring const& address)
                {
                    return !(address.size() > 3 && _wcsnicmp(address.c_str(), L"fe", 2) == 0 && wcschr(L"89abAB", address[2]) != nullptr);
                });

            hostNameOrIPAddress = winrt::hstring{ routable != advertisedHost.IPv6Addresses.end() ? *routable : advertisedHost.IPv6Addresses.front() };
        }
        else if (!advertisedHost.HostName.empty())
        {
            hostNameOrIPAddress = winrt::hstring{ advertisedHost.HostName };
        }

        port = advertisedHost.Port;

        return true;
    }

    // --- connect via direct host information / ip
    if (!definition.MatchDirectPort.empty())
    {
        // TODO: Check to make sure we've waited at least the minimum probe interval before checking these

        wchar_t* end{ nullptr };
        auto bigport = wcstoul(definition.MatchDirectPort.c_str(), &end, 10);

        // If port number is 0 or > int16.max then error out
        if (bigport == 0 || bigport > UINT16_MAX)
        {
            LOG_IF_FAILED(E_INVALIDARG);
            // TODO: report the error
            return false;
        }

        port = static_cast<uint16_t>(bigport);

        if (!definition.MatchDirectHostNameOrIPAddress.empty())
        {
            TraceLoggingWrite(
                MidiNetworkMidiTransportTelemetryProvider::Provider(),
                MIDI_TRACE_EVENT_INFO,
                TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
                TraceLoggingLevel(WINEVENT_LEVEL_INFO),
                TraceLoggingPointer(this, "this"),
                TraceLoggingWideString(L"Processing direct connection entry", MIDI_TRACE_EVENT_MESSAGE_FIELD),
                TraceLoggingWideString(definition.MatchDirectHostNameOrIPAddress.c_str(), "remote IP address"),
                TraceLoggingWideString(definition.MatchDirectPort.c_str(), "remote port")
            );

            hostNameOrIPAddress = definition.MatchDirectHostNameOrIPAddress;
        }

        // TODO: Check to see if the client is actually online

        return true;
    }

    return false;
}


HRESULT
CMidi2NetworkMidiEndpointManager::CreateParentDeviceForClients()
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

        RETURN_HR_IF_NULL(E_UNEXPECTED, m_midiDeviceManager);


        // the parent device parameters are set by the transport (this)
        std::wstring parentDeviceName{ TRANSPORT_CLIENT_PARENT_DEVICE_NAME };
        std::wstring parentDeviceInstanceId{ internal::NormalizeDeviceInstanceIdWStringCopy(TRANSPORT_CLIENT_PARENT_ID) };

        SW_DEVICE_CREATE_INFO createInfo = {};
        createInfo.cbSize = sizeof(createInfo);
        createInfo.pszInstanceId = parentDeviceInstanceId.c_str();
        createInfo.CapabilityFlags = SWDeviceCapabilitiesNone;
        createInfo.pszDeviceDescription = parentDeviceName.c_str();
        createInfo.pContainerId = &m_containerId;

        wil::unique_cotaskmem_string newParentDeviceId;

        RETURN_IF_FAILED(m_midiDeviceManager->ActivateVirtualParentDevice(
            0,
            nullptr,
            &createInfo,
            &newParentDeviceId
        ));

        m_clientParentDeviceInstanceId = newParentDeviceId.get();
        //m_clientParentDeviceInstanceId = parentDeviceInstanceId;


        TraceLoggingWrite(
            MidiNetworkMidiTransportTelemetryProvider::Provider(),
            MIDI_TRACE_EVENT_INFO,
            TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
            TraceLoggingLevel(WINEVENT_LEVEL_INFO),
            TraceLoggingPointer(this, "this"),
            TraceLoggingWideString(m_clientParentDeviceInstanceId.c_str(), "New parent device instance id")
        );

        return S_OK;
    }
    CATCH_RETURN()
}

_Use_decl_annotations_
HRESULT
CMidi2NetworkMidiEndpointManager::CreateParentDeviceForHost(
    winrt::hstring const& name,
    winrt::hstring const& serviceInstanceId,
    std::wstring& createdNewDeviceInstanceId
)
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

        RETURN_HR_IF_NULL(E_UNEXPECTED, m_midiDeviceManager);

        auto const parentKey = internal::NormalizeDeviceInstanceIdWStringCopy(std::wstring{ serviceInstanceId.c_str() });

        // Already made one for this name, so hand back what activation gave us then. Asking again
        // cannot succeed while the device is still there, and the id has to be the one activation
        // returned: an endpoint activated against any other form of it waits for a completion
        // which never arrives.
        {
            auto lock = m_hostParentDeviceIdsLock.lock_shared();

            if (auto const existing = m_hostParentDeviceIds.find(parentKey); existing != m_hostParentDeviceIds.end())
            {
                createdNewDeviceInstanceId = existing->second;

                return S_FALSE;
            }
        }

        // the parent device parameters are set by the transport (this)
        std::wstring parentDeviceId{ internal::NormalizeDeviceInstanceIdWStringCopy(TRANSPORT_HOST_PARENT_ID_PREFIX + std::wstring{ serviceInstanceId.c_str() }) };
        std::wstring parentName{ TRANSPORT_HOST_PARENT_NAME_PREFIX + name };

        wil::unique_cotaskmem_string newParentDeviceId;

        SW_DEVICE_CREATE_INFO createInfo = {};
        createInfo.cbSize = sizeof(createInfo);
        createInfo.pszInstanceId = parentDeviceId.c_str();
        createInfo.CapabilityFlags = SWDeviceCapabilitiesNone;
        createInfo.pszDeviceDescription = parentName.c_str();
        createInfo.pContainerId = &m_containerId;

        RETURN_IF_FAILED(m_midiDeviceManager->ActivateVirtualParentDevice(
            0,
            nullptr,
            &createInfo,
            &newParentDeviceId
        ));

        createdNewDeviceInstanceId = internal::NormalizeDeviceInstanceIdWStringCopy(newParentDeviceId.get());

        {
            auto lock = m_hostParentDeviceIdsLock.lock_exclusive();

            m_hostParentDeviceIds[parentKey] = createdNewDeviceInstanceId;
        }

        TraceLoggingWrite(
            MidiNetworkMidiTransportTelemetryProvider::Provider(),
            MIDI_TRACE_EVENT_INFO,
            TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
            TraceLoggingLevel(WINEVENT_LEVEL_INFO),
            TraceLoggingPointer(this, "this"),
            TraceLoggingWideString(createdNewDeviceInstanceId.c_str(), "New parent device instance id")
        );

        return S_OK;
    }
    CATCH_RETURN()
}


_Use_decl_annotations_
HRESULT
CMidi2NetworkMidiEndpointManager::DeleteEndpoint(
    std::wstring deviceInstanceId)
{
    TraceLoggingWrite(
        MidiNetworkMidiTransportTelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_INFO,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this"),
        TraceLoggingWideString(deviceInstanceId.c_str(), "deviceShortInstanceId")
    );

    RETURN_HR_IF_NULL(E_UNEXPECTED, m_midiDeviceManager);

    auto instanceId = internal::NormalizeDeviceInstanceIdWStringCopy(deviceInstanceId);

    if (!instanceId.empty())
    {
        RETURN_IF_FAILED(m_midiDeviceManager->RemoveEndpoint(instanceId.c_str()));

        std::vector<std::wstring> removedEndpointDeviceIds{ };

        {
            auto lock = m_createdEndpointsLock.lock();

            m_createdEndpoints.erase(
                std::remove_if(
                    m_createdEndpoints.begin(),
                    m_createdEndpoints.end(),
                    [&instanceId, &removedEndpointDeviceIds](auto const& record)
                    {
                        if (std::wstring{ record.DeviceInstanceId } != instanceId)
                        {
                            return false;
                        }

                        removedEndpointDeviceIds.push_back(std::wstring{ record.EndpointDeviceId });

                        return true;
                    }),
                m_createdEndpoints.end());
        }

        // The same remote gets the same endpoint id back when it reconnects, and the new
        // endpoint has no latency written yet.
        auto lock = m_lastWrittenLatencyTicksLock.lock();

        for (auto const& endpointDeviceId : removedEndpointDeviceIds)
        {
            m_lastWrittenLatencyTicks.erase(endpointDeviceId);
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
            TraceLoggingWideString(L"Empty instanceId property for endpoint", MIDI_TRACE_EVENT_MESSAGE_FIELD)
        );

        RETURN_IF_FAILED(E_INVALIDARG);
    }

    return S_OK;
}

_Use_decl_annotations_
HRESULT
CMidi2NetworkMidiEndpointManager::QueueDiscoveryAndNegotiation(
    std::wstring const& endpointDeviceInterfaceId
)
{
    if (endpointDeviceInterfaceId.empty())
    {
        return E_INVALIDARG;
    }

    {
        auto lock = m_pendingNegotiationsLock.lock();

        if (std::find(m_pendingNegotiations.begin(), m_pendingNegotiations.end(), endpointDeviceInterfaceId) == m_pendingNegotiations.end())
        {
            m_pendingNegotiations.push_back(endpointDeviceInterfaceId);
        }
    }

    LOG_IF_FAILED(WakeupBackgroundNegotiationThread());

    return S_OK;
}

_Use_decl_annotations_
HRESULT
CMidi2NetworkMidiEndpointManager::InitiateDiscoveryAndNegotiation(
    std::wstring const& endpointDeviceInterfaceId
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

    // Discovery and protocol negotiation

    ENDPOINTPROTOCOLNEGOTIATIONPARAMS negotiationParams{ };
    negotiationParams.PreferredMidiProtocol = MIDI_PROP_CONFIGURED_PROTOCOL_MIDI2;
    negotiationParams.PreferToSendJitterReductionTimestampsToEndpoint = false;
    negotiationParams.PreferToReceiveJitterReductionTimestampsFromEndpoint = false;


    RETURN_IF_FAILED(m_midiProtocolManager->DiscoverAndNegotiate(
        m_transportId,
        endpointDeviceInterfaceId.c_str(),
        negotiationParams
    ));

    return S_OK;
}

// endpoint for a remote client connected to this host
_Use_decl_annotations_
HRESULT
CMidi2NetworkMidiEndpointManager::CreateNewHostEndpointToRemoteClient(
    _In_ std::wstring const& configIdentifier,
    _In_ std::wstring const& parentHostDeviceInstanceId,
    _In_ std::wstring const& endpointName,
    _In_ std::wstring const& remoteEndpointProductInstanceId,
    _In_ winrt::Windows::Networking::HostName const& hostName,
    _In_ std::wstring const& networkPort,
    _In_ bool umpOnly,
    _In_ uint8_t const fallbackMidi1PortCount,
    _Out_ std::wstring& createdNewDeviceInstanceId,
    _Out_ std::wstring& createdNewEndpointDeviceInterfaceId
)
{
    RETURN_HR_IF(E_INVALIDARG, parentHostDeviceInstanceId.empty());

    return CreateNewEndpoint(
        MidiNetworkConnectionRole::ConnectionWindowsIsHost,
        configIdentifier,
        parentHostDeviceInstanceId,
        endpointName,
        remoteEndpointProductInstanceId,
        hostName,
        networkPort,
        umpOnly,
        fallbackMidi1PortCount,
        createdNewDeviceInstanceId,
        createdNewEndpointDeviceInterfaceId
    );

}

// endpoint for this client connected to a remote host
_Use_decl_annotations_
HRESULT
CMidi2NetworkMidiEndpointManager::CreateNewClientEndpointToRemoteHost(
    _In_ std::wstring const& configIdentifier,
    _In_ std::wstring const& endpointName,
    _In_ std::wstring const& remoteEndpointProductInstanceId,
    _In_ winrt::Windows::Networking::HostName const& hostName,
    _In_ std::wstring const& networkPort,
    _In_ bool umpOnly,
    _In_ uint8_t const fallbackMidi1PortCount,
    _Out_ std::wstring& createdNewDeviceInstanceId,
    _Out_ std::wstring& createdNewEndpointDeviceInterfaceId
)
{
    RETURN_HR_IF(E_INVALIDARG, m_clientParentDeviceInstanceId.empty());

    return CreateNewEndpoint(
        MidiNetworkConnectionRole::ConnectionWindowsIsClient,
        configIdentifier,
        m_clientParentDeviceInstanceId,
        endpointName,
        remoteEndpointProductInstanceId,
        hostName,
        networkPort,
        umpOnly,
        fallbackMidi1PortCount,
        createdNewDeviceInstanceId,
        createdNewEndpointDeviceInterfaceId
    );

}

// Stable across builds, processes and reboots. std::hash is implementation-defined and is
// explicitly not required to be stable, which makes it unusable for a value we persist as a
// device instance id.
static uint64_t StableHash64(_In_ std::wstring const& value)
{
    uint64_t hash{ 14695981039346656037ULL };   // FNV-1a 64 offset basis

    for (auto const& ch : value)
    {
        auto codeUnit = static_cast<uint16_t>(ch);

        hash ^= static_cast<uint64_t>(codeUnit & 0x00FF);
        hash *= 1099511628211ULL;

        hash ^= static_cast<uint64_t>((codeUnit >> 8) & 0x00FF);
        hash *= 1099511628211ULL;
    }

    return hash;
}

static std::wstring FormatHash64(_In_ uint64_t const value)
{
    wchar_t buffer[17]{ };

    swprintf_s(buffer, ARRAYSIZE(buffer), L"%016llX", value);

    return std::wstring{ buffer };
}

// Endpoint identity, per spec section 4.4: "Operating systems and devices may use the
// UMPEndpointName and ProductInstanceId to recall Device properties when reconnecting to
// devices." Neither field works alone. Product Instance Id is only "statistically unique" and a
// device with several Host instances is told to use the same one for all of them, while the UMP
// Endpoint Name is required to differ per Host instance but is only unique within a device.
//
// Deliberately excludes IP address, port and role. Clients "may use a new UDP port number for
// every Session" (spec 3.3), addresses move with DHCP, and a device connecting in both roles
// presents identical identity in both (spec section 12).
static std::wstring BuildEndpointDeviceInstanceId(
    _In_ std::wstring const& endpointName,
    _In_ std::wstring const& productInstanceId)
{
    auto identityKey = productInstanceId + L"|" + endpointName;

    // Device instance ids allow only -_ and ASCII alphanumerics, so the name is reduced to a
    // readable hint and the hash carries the actual identity.
    auto readableName = internal::RemoveInvalidSWDUniqueIdCharacters(endpointName);

    if (readableName.length() > MIDI_NETWORK_ENDPOINT_INSTANCE_ID_NAME_MAX_CHARS)
    {
        readableName = readableName.substr(0, MIDI_NETWORK_ENDPOINT_INSTANCE_ID_NAME_MAX_CHARS);
    }

    return internal::NormalizeDeviceInstanceIdWStringCopy(
        std::wstring{ MIDI_NETWORK_ENDPOINT_INSTANCE_ID_PREFIX } +
        readableName +
        L"_" +
        FormatHash64(StableHash64(identityKey)));
}

_Use_decl_annotations_
std::shared_ptr<WindowsMidiServicesPluginConfigurationLib::MidiEndpointCustomProperties>
CMidi2NetworkMidiEndpointManager::ResolveEndpointCustomization(
    MidiNetworkConnectionRole const thisServiceRole,
    std::wstring const& configIdentifier,
    WindowsMidiServicesPluginConfigurationLib::MidiEndpointMatchCriteria& matchCriteria,
    std::wstring& customName,
    std::wstring& customDescription)
{
    customName.clear();
    customDescription.clear();

    // Looked up by configuration entry id, because at connection time that is the only thing we
    // reliably know: a direct connection has not yet learned the remote's name or product
    // instance id.
    GUID parsed{};

    winrt::guid entryId = internal::TryParseGuidString(configIdentifier, parsed)
        ? winrt::guid{ parsed }
        : winrt::guid{};

    if (entryId != winrt::guid{})
    {
        // From the stored definitions, which configuration updates change. A host keeps the
        // copy it was started with, so reading the host's own meant a rename never reached
        // the remote clients which connected after it.
        if (thisServiceRole == MidiNetworkConnectionRole::ConnectionWindowsIsClient)
        {
            if (auto definition = TransportState::Current().GetClientDefinition(entryId); definition.has_value())
            {
                customName = definition->CustomEndpointName;
            }
        }
        else if (auto definition = TransportState::Current().GetHostDefinition(entryId); definition.has_value())
        {
            customName = definition->CustomEndpointName;
        }
    }

    auto configurationManager = TransportState::Current().GetConfigurationManager();

    if (configurationManager == nullptr)
    {
        return nullptr;
    }

    auto customProperties = configurationManager->CustomPropertiesCache()->GetProperties(matchCriteria);

    if (customProperties != nullptr)
    {
        // A customization matched by identity wins: it is the more specific answer, and it
        // is what a later rename writes.
        if (!customProperties->Name.empty())
        {
            customName = customProperties->Name;
        }

        if (!customProperties->Description.empty())
        {
            customDescription = customProperties->Description;
        }
    }

    return customProperties;
}

_Use_decl_annotations_
HRESULT
CMidi2NetworkMidiEndpointManager::CreateNewEndpoint(
    MidiNetworkConnectionRole thisServiceRole,
    std::wstring const& configIdentifier,
    std::wstring const& parentInstanceId,
    std::wstring const& endpointName,
    std::wstring const& remoteEndpointProductInstanceId,
    winrt::Windows::Networking::HostName const& hostName,
    std::wstring const& networkPort,
    bool umpOnly,
    uint8_t const fallbackMidi1PortCount,
    std::wstring& createdNewDeviceInstanceId,
    std::wstring& createdNewEndpointDeviceInterfaceId
)
// Declared HRESULT, so it must not throw: callers use RETURN_IF_FAILED and an escaping
// WinRT exception would unwind past them into a worker thread.
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

    RETURN_HR_IF(E_UNEXPECTED, !m_initialized);
    RETURN_HR_IF_NULL(E_UNEXPECTED, m_midiDeviceManager);

    // Both are required by the spec (sections 6.4 and 6.5) and together they are the endpoint's
    // identity. An implementation which omits either is out of spec, and accepting it would mean
    // inventing an identity that cannot be recalled on reconnect. Refused, loudly.
    if (endpointName.empty() || remoteEndpointProductInstanceId.empty())
    {
        TraceLoggingWrite(
            MidiNetworkMidiTransportTelemetryProvider::Provider(),
            MIDI_TRACE_EVENT_ERROR,
            TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
            TraceLoggingLevel(WINEVENT_LEVEL_ERROR),
            TraceLoggingPointer(this, "this"),
            TraceLoggingWideString(L"Remote endpoint did not supply both a UMP Endpoint Name and a Product Instance Id, which the specification requires. Refusing to create an endpoint for it.", MIDI_TRACE_EVENT_MESSAGE_FIELD),
            TraceLoggingBoolean(endpointName.empty(), "endpoint name missing"),
            TraceLoggingBoolean(remoteEndpointProductInstanceId.empty(), "product instance id missing"),
            TraceLoggingWideString(hostName != nullptr ? hostName.CanonicalName().c_str() : L"", "remote address"),
            TraceLoggingWideString(networkPort.c_str(), "remote port")
        );

        RETURN_IF_FAILED(E_INVALIDARG);
    }

    std::wstring transportCode(TRANSPORT_CODE);

    // A name the user chose for this connection, resolved before anything is activated so the
    // endpoint and its MIDI 1.0 ports are created under it rather than being renamed a moment
    // later. The customization is cached by the configuration manager whether or not the
    // endpoint existed when it arrived, which is what makes this work for a connection the user
    // names as they create it.
    // Computed here rather than at activation because a saved customization is matched on it.
    // It is derived from the remote's identity, so it is the same on every reconnect.
    std::wstring instanceId = BuildEndpointDeviceInstanceId(endpointName, remoteEndpointProductInstanceId);

    WindowsMidiServicesPluginConfigurationLib::MidiEndpointMatchCriteria matchCriteria{};
    matchCriteria.DeviceInstanceId = instanceId;
    matchCriteria.TransportSuppliedEndpointName = endpointName;
    matchCriteria.DeviceProductInstanceId = remoteEndpointProductInstanceId;
    matchCriteria.NetworkStaticIPAddress = hostName != nullptr ? hostName.CanonicalName() : L"";
    matchCriteria.NetworkPort = static_cast<uint16_t>(_wtoi(networkPort.c_str()));

    std::wstring customName{ };
    std::wstring customDescription{ };

    // Held for the rest of the function because the property values written below point into it,
    // and because only the name and description can be supplied while the node is being created.
    auto customProperties = ResolveEndpointCustomization(
        thisServiceRole,
        configIdentifier,
        matchCriteria,
        customName,
        customDescription);

    // The user's name is the one shown everywhere, including to apps which know nothing about
    // MIDI properties, so it becomes the device node name too and not just PKEY_MIDI_CustomEndpointName.
    std::wstring friendlyName = customName.empty() ? endpointName : customName;


    TraceLoggingWrite(
        MidiNetworkMidiTransportTelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_INFO,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this"),
        TraceLoggingWideString(L"Adding endpoint properties", MIDI_TRACE_EVENT_MESSAGE_FIELD),
        TraceLoggingWideString(friendlyName.c_str(), "friendlyName"),
        TraceLoggingWideString(transportCode.c_str(), "transport code"),
        TraceLoggingWideString(endpointName.c_str(), "endpointName")
    );

    // Device properties

    SW_DEVICE_CREATE_INFO createInfo = {};
    createInfo.cbSize = sizeof(createInfo);

    createInfo.pszInstanceId = instanceId.c_str();
    createInfo.CapabilityFlags = SWDeviceCapabilitiesNone;
    createInfo.pszDeviceDescription = friendlyName.c_str();

    wil::unique_cotaskmem_string newDeviceInterfaceId;

    TraceLoggingWrite(
        MidiNetworkMidiTransportTelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_INFO,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this"),
        TraceLoggingWideString(L"Activating endpoint", MIDI_TRACE_EVENT_MESSAGE_FIELD),
        TraceLoggingWideString(instanceId.c_str(), "instance id")
    );


    // Add custom properties for the network information

    std::vector<DEVPROPERTY> interfaceDevProperties;

    auto hostNameString = hostName.ToString();

    interfaceDevProperties.push_back({ {PKEY_MIDI_NetworkMidiLastRemoteHostName, DEVPROP_STORE_SYSTEM, nullptr},
        DEVPROP_TYPE_STRING, static_cast<ULONG>((hostNameString.size() + 1) * sizeof(WCHAR)), (PVOID)(hostNameString.c_str()) });

    interfaceDevProperties.push_back({ {PKEY_MIDI_NetworkMidiLastRemotePort, DEVPROP_STORE_SYSTEM, nullptr},
        DEVPROP_TYPE_STRING, static_cast<ULONG>((networkPort.size() + 1) * sizeof(WCHAR)), (PVOID)(networkPort.c_str()) });

    interfaceDevProperties.push_back({ {PKEY_MIDI_TransportEndpointConfigId, DEVPROP_STORE_SYSTEM, nullptr},
        DEVPROP_TYPE_STRING, static_cast<ULONG>((configIdentifier.size() + 1) * sizeof(WCHAR)), (PVOID)(configIdentifier.c_str()) });

    // The instance id no longer encodes the role, so it is published as a property instead.
    uint32_t connectionRole = thisServiceRole == MidiNetworkConnectionRole::ConnectionWindowsIsHost ?
        MIDI_NETWORK_CONNECTION_ROLE_WINDOWS_IS_HOST : MIDI_NETWORK_CONNECTION_ROLE_WINDOWS_IS_CLIENT;

    interfaceDevProperties.push_back({ {PKEY_MIDI_NetworkMidiConnectionRole, DEVPROP_STORE_SYSTEM, nullptr},
        DEVPROP_TYPE_UINT32, static_cast<ULONG>(sizeof(uint32_t)), (PVOID)&connectionRole });


    // A Network MIDI 2.0 endpoint declares function blocks, never group terminal blocks, and the
    // service builds MIDI 1.0 ports from whichever it finds. A remote which never completes
    // endpoint discovery declares neither, so the service has nothing to work from and the
    // endpoint ends up with no MIDI 1.0 ports at all. Publishing a group terminal block up front
    // gives it something. Function blocks take precedence over this the moment they arrive, so a
    // remote which does describe itself is unaffected.
    //
    // This has to outlive ActivateEndpoint, because the property below points into it. The name
    // table holds its own buffer for the same reason.
    std::vector<std::byte> groupTerminalBlockData{};
    WindowsMidiServicesNamingLib::MidiEndpointNameTable nameTable{};

    if (!umpOnly)
    {
        LOG_IF_FAILED(BuildFallbackMidi1PortProperties(
            friendlyName,
            fallbackMidi1PortCount,
            groupTerminalBlockData,
            nameTable,
            interfaceDevProperties));
    }


    std::wstring endpointDescription{ L"Network MIDI 2.0 endpoint "};

    switch (thisServiceRole)
    {
    case MidiNetworkConnectionRole::ConnectionWindowsIsHost:
        endpointDescription += L"(This PC is the Network Host)";
        break;
    case MidiNetworkConnectionRole::ConnectionWindowsIsClient:
        endpointDescription += L"(This PC is a Network Client)";
        break;
    }

    // The id is kept intact everywhere it is shown or matched, because it is meaningful on the
    // remote device. Only the SWD copy is stripped, since device identifiers allow far fewer
    // characters than the specification permits here.
    auto swdUniqueIdentifier = internal::RemoveInvalidSWDUniqueIdCharacters(remoteEndpointProductInstanceId);

    MIDIENDPOINTCOMMONPROPERTIES commonProperties{};
    commonProperties.TransportId = TRANSPORT_LAYER_GUID;
    commonProperties.EndpointDeviceType = MidiEndpointDeviceType::MidiEndpointDeviceType_Normal;
    commonProperties.FriendlyName = friendlyName.c_str();
    commonProperties.TransportCode = transportCode.c_str();
    commonProperties.EndpointName = endpointName.c_str();
    commonProperties.EndpointDescription = endpointDescription.c_str();
    commonProperties.CustomEndpointName = customName.empty() ? nullptr : customName.c_str();
    commonProperties.CustomEndpointDescription = customDescription.empty() ? nullptr : customDescription.c_str();

    commonProperties.UniqueIdentifier = swdUniqueIdentifier.c_str();
    commonProperties.SupportedDataFormats = MidiDataFormats::MidiDataFormats_UMP;
    commonProperties.NativeDataFormat = MidiDataFormats::MidiDataFormats_UMP;

    UINT32 capabilities{ 0 };
    capabilities |= MidiEndpointCapabilities_SupportsMidi1Protocol;
    capabilities |= MidiEndpointCapabilities_SupportsMidi2Protocol;
    capabilities |= MidiEndpointCapabilities_SupportsMultiClient;
    capabilities |= MidiEndpointCapabilities_GenerateIncomingTimestamps;
    commonProperties.Capabilities = (MidiEndpointCapabilities)capabilities;

    // this is here only because it kept getting optimized away during debugging
    std::wstring parent = parentInstanceId;

    auto activateHR = m_midiDeviceManager->ActivateEndpoint(
        (PCWSTR)parent.c_str(),                                 // parent instance Id
        umpOnly,                                                // UMP-only. When set to false, WinMM MIDI 1.0 ports are created
        MidiFlow::MidiFlowBidirectional,                        // MIDI Flow
        &commonProperties,
        (ULONG)interfaceDevProperties.size(),
        (ULONG)0,
        interfaceDevProperties.data(),
        nullptr,
        &createInfo,
        &newDeviceInterfaceId);

    RETURN_IF_FAILED(activateHR);

    // S_FALSE means this instance id is already active, so nothing was created and no interface
    // id was returned. Now that identity is role-free, this is how a device which is already
    // connected in the other role shows up. Treated as a failure here so the caller can decline
    // the session rather than proceed with an endpoint it does not have.
    if (activateHR == S_FALSE || newDeviceInterfaceId.get() == nullptr)
    {
        TraceLoggingWrite(
            MidiNetworkMidiTransportTelemetryProvider::Provider(),
            MIDI_TRACE_EVENT_WARNING,
            TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
            TraceLoggingLevel(WINEVENT_LEVEL_WARNING),
            TraceLoggingPointer(this, "this"),
            TraceLoggingWideString(L"This device already has an active endpoint, so a second session for it was not created.", MIDI_TRACE_EVENT_MESSAGE_FIELD),
            TraceLoggingWideString(instanceId.c_str(), "instance id"),
            TraceLoggingWideString(endpointName.c_str(), "endpoint name"),
            TraceLoggingWideString(remoteEndpointProductInstanceId.c_str(), "product instance id"),
            TraceLoggingWideString(hostName != nullptr ? hostName.CanonicalName().c_str() : L"", "remote address")
        );

        RETURN_IF_FAILED(HRESULT_FROM_WIN32(ERROR_DEVICE_ALREADY_ATTACHED));
    }

    TraceLoggingWrite(
        MidiNetworkMidiTransportTelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_INFO,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this"),
        TraceLoggingWideString(L"Endpoint activated", MIDI_TRACE_EVENT_MESSAGE_FIELD),
        TraceLoggingWideString(remoteEndpointProductInstanceId.c_str(), "product instance id"),
        TraceLoggingWideString(newDeviceInterfaceId.get(), "new device interface id")
    );


    // we need this for removal later
    //createdNewDeviceInstanceId = internal::NormalizeDeviceInstanceIdWStringCopy(L"SWD\\MIDISRV\\" + instanceId);
    createdNewDeviceInstanceId = internal::NormalizeDeviceInstanceIdWStringCopy(instanceId);
    createdNewEndpointDeviceInterfaceId = internal::NormalizeEndpointInterfaceIdWStringCopy(newDeviceInterfaceId.get());

    {
        auto lock = m_createdEndpointsLock.lock();

        m_createdEndpoints.push_back(CreatedEndpointRecord{
            winrt::hstring{ createdNewEndpointDeviceInterfaceId },
            winrt::hstring{ createdNewDeviceInstanceId },
            winrt::hstring{ endpointName },
            winrt::hstring{ remoteEndpointProductInstanceId },
            umpOnly ? (uint8_t)0 : ClampFallbackMidi1PortCount(fallbackMidi1PortCount) });
    }

    // Everything the node could not be created with. A network endpoint is never live when its
    // customization arrives, so without this an image or a port naming choice would be cached
    // and then silently dropped.
    if (customProperties != nullptr)
    {
        std::vector<DEVPROPERTY> customDevProperties{};

        if (customProperties->WriteAllProperties(customDevProperties) && customDevProperties.size() > 0)
        {
            LOG_IF_FAILED(m_midiDeviceManager->UpdateEndpointProperties(
                createdNewEndpointDeviceInterfaceId.c_str(),
                static_cast<ULONG>(customDevProperties.size()),
                customDevProperties.data()));
        }
    }

    TraceLoggingWrite(
        MidiNetworkMidiTransportTelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_INFO,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this"),
        TraceLoggingWideString(L"Done", MIDI_TRACE_EVENT_MESSAGE_FIELD)
    );

    return S_OK;
}
CATCH_RETURN()


_Use_decl_annotations_
HRESULT
CMidi2NetworkMidiEndpointManager::BuildFallbackMidi1PortProperties(
    std::wstring const& portName,
    uint8_t const fallbackMidi1PortCount,
    std::vector<std::byte>& groupTerminalBlockData,
    WindowsMidiServicesNamingLib::MidiEndpointNameTable& nameTable,
    std::vector<DEVPROPERTY>& properties)
try
{
    // A Network MIDI 2.0 endpoint declares function blocks, never group terminal blocks, and the
    // service builds MIDI 1.0 ports from whichever it finds. A remote which never completes
    // endpoint discovery declares neither, so the service has nothing to work from and the
    // endpoint ends up with no MIDI 1.0 ports at all. Publishing a group terminal block gives it
    // something. Function blocks take precedence the moment they arrive, so a remote which does
    // describe itself is unaffected.
    internal::GroupTerminalBlockInternal block{};

    block.Number = 1;
    block.Direction = MIDI_GROUP_TERMINAL_BLOCK_BIDIRECTIONAL;
    block.FirstGroupIndex = 0;
    block.GroupCount = ClampFallbackMidi1PortCount(fallbackMidi1PortCount);
    block.Protocol = 0x11;      // 0x11 = MIDI 2.0

    // The remote chooses this name, and a name too long for a block descriptor would otherwise
    // make the write fail and leave the endpoint with no MIDI 1.0 ports at all.
    block.Name = BoundBlockName(portName);

    std::vector<internal::GroupTerminalBlockInternal> blocks{ block };

    groupTerminalBlockData.clear();

    RETURN_HR_IF(E_FAIL, !internal::WriteGroupTerminalBlocksToPropertyDataPointer(blocks, groupTerminalBlockData));

    properties.push_back({ {PKEY_MIDI_GroupTerminalBlocks, DEVPROP_STORE_SYSTEM, nullptr},
        DEVPROP_TYPE_BINARY, static_cast<ULONG>(groupTerminalBlockData.size()), (PVOID)groupTerminalBlockData.data() });

    // The names the service gives the ports come from this table, not from the block.
    //
    // Every group is named, not just the ones the block above spans. A function block may later
    // land on any group, and naming all sixteen is also what puts the group number into the port
    // name: the names only differ by group, so the naming library numbers them. A network port is
    // addressed by its group and there is no older name to stay compatible with, so that number
    // belongs there even when the endpoint currently has a single port. Narrowing this to the
    // spanned groups would silently drop the number from that case.
    auto namingBlocks = blocks;
    namingBlocks.front().GroupCount = MIDI_NETWORK_MIDI_GROUP_COUNT;

    // The endpoint name is passed as the parent as well as being the block name. It is what the
    // legacy WinMM form puts in its parentheses, and leaving it out gives "MIDIIN2 ()" for every
    // group past the first. The naming library strips the block name when it repeats the parent,
    // so the new style name does not end up doubled.
    RETURN_IF_FAILED(nameTable.PopulateAllEntriesForNativeUmpDevice(block.Name, namingBlocks));

    if (Feature_Servicing_MIDI2PortNamingRework::IsEnabled())
    {
        nameTable.SetPortNamesHaveLegacyEquivalent(false);

        // these ports are new style, so they need the same numbering as everything else
        LOG_IF_FAILED(nameTable.RebuildNewStyleNames(std::wstring{ block.Name }, false));
    }

    RETURN_IF_FAILED(nameTable.WriteProperties(properties));

    return S_OK;
}
catch (...)
{
    RETURN_CAUGHT_EXCEPTION();
}


_Use_decl_annotations_
HRESULT
CMidi2NetworkMidiEndpointManager::RefreshMidi1PortsForEndpoint(
    std::wstring const& endpointDeviceInterfaceId,
    uint8_t const fallbackMidi1PortCount,
    std::optional<std::wstring> const& portNameOverride)
try
{
    TraceLoggingWrite(
        MidiNetworkMidiTransportTelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_INFO,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this"),
        TraceLoggingWideString(L"Enter", MIDI_TRACE_EVENT_MESSAGE_FIELD),
        TraceLoggingWideString(endpointDeviceInterfaceId.c_str(), MIDI_TRACE_EVENT_DEVICE_SWD_ID_FIELD)
    );

    RETURN_HR_IF(E_INVALIDARG, endpointDeviceInterfaceId.empty());
    RETURN_HR_IF_NULL(E_UNEXPECTED, m_midiDeviceManager);

    // What this transport built the endpoint from. Taken from the record rather than read back
    // out of the group terminal block property, because that means trusting a length-prefixed
    // blob to walk correctly. A count of zero means the endpoint was built UMP-only and has no
    // MIDI 1.0 ports to rebuild.
    std::wstring recordName{ };
    uint8_t recordCount{ 0 };

    {
        auto lock = m_createdEndpointsLock.lock();

        auto const record = std::find_if(
            m_createdEndpoints.begin(),
            m_createdEndpoints.end(),
            [&endpointDeviceInterfaceId](CreatedEndpointRecord const& entry)
            {
                return internal::NormalizeEndpointInterfaceIdWStringCopy(std::wstring{ entry.EndpointDeviceId }) ==
                    internal::NormalizeEndpointInterfaceIdWStringCopy(endpointDeviceInterfaceId);
            });

        RETURN_HR_IF(S_FALSE, record == m_createdEndpoints.end());

        recordName = record->TransportSuppliedEndpointName;
        recordCount = record->FallbackMidi1PortCount;
    }

    RETURN_HR_IF(S_FALSE, recordCount == 0);

    // Without an override the current effective name has to be resolved. With one, the caller has
    // just written a name the device does not report back yet, and an empty one means the
    // customization was withdrawn, so the remote's own name applies again.
    std::wstring portName{ portNameOverride.value_or(std::wstring{ }) };

    if (!portNameOverride.has_value())
    {
        try
        {
            auto additionalProperties = winrt::single_threaded_vector<winrt::hstring>();
            additionalProperties.Append(STRING_PKEY_MIDI_CustomEndpointName);

            auto deviceInfo = winrt::Windows::Devices::Enumeration::DeviceInformation::CreateFromIdAsync(
                winrt::hstring{ endpointDeviceInterfaceId },
                additionalProperties,
                winrt::Windows::Devices::Enumeration::DeviceInformationKind::DeviceInterface).get();

            if (deviceInfo != nullptr)
            {
                // A custom name is applied at enumeration and never reaches the device interface
                // name, which keeps reporting whatever the remote supplied. Reading the name here
                // instead of the property would quietly undo a rename.
                auto customName = deviceInfo.Properties().TryLookup(STRING_PKEY_MIDI_CustomEndpointName);

                if (customName != nullptr)
                {
                    portName = winrt::unbox_value_or<winrt::hstring>(customName, L"");
                }
            }
        }
        CATCH_LOG();
    }

    if (portName.empty())
    {
        portName = recordName;
    }

    RETURN_HR_IF(S_FALSE, portName.empty());

    // Zero is a rename asking to keep the width it already has
    auto const effectiveCount = fallbackMidi1PortCount == 0
        ? recordCount
        : ClampFallbackMidi1PortCount(fallbackMidi1PortCount);

    std::vector<std::byte> groupTerminalBlockData{ };
    WindowsMidiServicesNamingLib::MidiEndpointNameTable nameTable{ };
    std::vector<DEVPROPERTY> properties{ };

    RETURN_IF_FAILED(BuildFallbackMidi1PortProperties(
        portName,
        effectiveCount,
        groupTerminalBlockData,
        nameTable,
        properties));

    RETURN_IF_FAILED(m_midiDeviceManager->UpdateEndpointProperties(
        endpointDeviceInterfaceId.c_str(),
        static_cast<ULONG>(properties.size()),
        properties.data()));

    TraceLoggingWrite(
        MidiNetworkMidiTransportTelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_INFO,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this"),
        TraceLoggingWideString(L"Rebuilt the fallback MIDI 1.0 port properties", MIDI_TRACE_EVENT_MESSAGE_FIELD),
        TraceLoggingWideString(endpointDeviceInterfaceId.c_str(), MIDI_TRACE_EVENT_DEVICE_SWD_ID_FIELD),
        TraceLoggingUInt8(effectiveCount, "fallback port count")
    );

    return S_OK;
}
catch (...)
{
    RETURN_CAUGHT_EXCEPTION();
}


_Use_decl_annotations_
winrt::hstring
CMidi2NetworkMidiEndpointManager::FindMatchingInstantiatedEndpoint(
    WindowsMidiServicesPluginConfigurationLib::MidiEndpointMatchCriteria& criteria)
{
    criteria.Normalize();

    auto lock = m_createdEndpointsLock.lock();

    for (auto const& record : m_createdEndpoints)
    {
        WindowsMidiServicesPluginConfigurationLib::MidiEndpointMatchCriteria available{};

        available.EndpointDeviceId = record.EndpointDeviceId;
        available.DeviceInstanceId = record.DeviceInstanceId;
        available.TransportSuppliedEndpointName = record.TransportSuppliedEndpointName;
        available.DeviceProductInstanceId = record.ProductInstanceId;

        if (available.Matches(criteria))
        {
            return available.EndpointDeviceId;
        }
    }

    return L"";
}


_Use_decl_annotations_
void
CMidi2NetworkMidiEndpointManager::OnHostRegistered(std::wstring_view const serviceInstanceLabel, winrt::guid const& networkAdapterId)
{
    m_dnssdAnnouncer.AddRegistration(serviceInstanceLabel, networkAdapterId);
}

_Use_decl_annotations_
void
CMidi2NetworkMidiEndpointManager::OnHostRegistrationEnding(std::wstring_view const serviceInstanceLabel) noexcept
{
    m_dnssdAnnouncer.RemoveRegistration(serviceInstanceLabel);
}


HRESULT
CMidi2NetworkMidiEndpointManager::Shutdown()
{
    TraceLoggingWrite(
        MidiNetworkMidiTransportTelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_INFO,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this"),
        TraceLoggingWideString(L"Enter", MIDI_TRACE_EVENT_MESSAGE_FIELD)
    );

    m_browser.Stop();

    // Before the creator thread stops, so a change arriving now cannot wake a thread that is gone
    m_networkChangeMonitor.Stop();

    // Before the hosts below withdraw their registrations, so no repeat can follow a goodbye
    m_dnssdAnnouncer.Stop();

    {
        auto lock = m_advertisedHostsLock.lock_exclusive();

        m_foundAdvertisedHosts.clear();
    }

    m_backgroundEndpointCreatorThread.request_stop();
    m_backgroundEndpointCreatorThreadWakeup.SetEvent();

    if (m_backgroundEndpointCreatorThread.joinable() && m_backgroundEndpointCreatorThread.get_id() != std::this_thread::get_id())
    {
        m_backgroundEndpointCreatorThread.join();
    }

    // Joined after the creator, which can release a connection on its last pass. The worker's
    // exit finishes whatever is still queued.
    m_endpointWorkerThread.request_stop();
    m_endpointWorkerWakeup.SetEvent();

    if (m_endpointWorkerThread.joinable() && m_endpointWorkerThread.get_id() != std::this_thread::get_id())
    {
        m_endpointWorkerThread.join();
    }

    // Deliberately not joined. A negotiation blocked inside the service cannot be canceled from
    // here, and waiting on it is what previously left the service unable to stop at all. Wait a
    // short time for a clean exit, and abandon the thread if it is stuck.
    m_backgroundNegotiationThread.request_stop();
    m_backgroundNegotiationThreadWakeup.SetEvent();

    if (m_backgroundNegotiationThread.joinable() && m_backgroundNegotiationThread.get_id() != std::this_thread::get_id())
    {
        if (m_negotiationThreadExitedEvent.wait(MIDI_NETWORK_NEGOTIATION_THREAD_EXIT_TIMEOUT_MILLISECONDS))
        {
            m_backgroundNegotiationThread.join();
        }
        else
        {
            TraceLoggingWrite(
                MidiNetworkMidiTransportTelemetryProvider::Provider(),
                MIDI_TRACE_EVENT_WARNING,
                TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
                TraceLoggingLevel(WINEVENT_LEVEL_WARNING),
                TraceLoggingPointer(this, "this"),
                TraceLoggingWideString(L"Negotiation thread is blocked in the service. Abandoning it so shutdown can continue.", MIDI_TRACE_EVENT_MESSAGE_FIELD)
            );

            // the jthread destructor would join, which is exactly what must not happen here
            m_backgroundNegotiationThread.detach();
        }
    }

    m_initialized = false;

    // Hosts and clients own sockets and worker threads. Without this they survive until static
    // destruction, which would join those threads under the loader lock.
    LOG_IF_FAILED(TransportState::Current().ShutdownHostsClientsAndConnections());

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
