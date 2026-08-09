// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://github.com/microsoft/MIDI/
// ============================================================================


#include "pch.h"
#include "midi2.NetworkMidiTransport.h"

using namespace wil;
using namespace Microsoft::WRL;
using namespace Microsoft::WRL::Wrappers;

#define MAX_DEVICE_ID_LEN 200 // size in chars


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

    // start background thread that creates endpoints
    RETURN_IF_FAILED(StartBackgroundEndpointCreator());

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

    // ProtocolId is mDNS: {4526e8c1-8aac-4153-9b16-55e86ada0e54}
    // ServiceName is per MIDI spec: _midi2._udp"
    // Domain is per MIDI spec: local

    winrt::hstring query = 
        L"System.Devices.AepService.ProtocolId:={4526e8c1-8aac-4153-9b16-55e86ada0e54} AND " \
        L"System.Devices.Dnssd.ServiceName:=\"_midi2._udp\" AND " \
        L"System.Devices.Dnssd.Domain:=\"local\"";

    auto props = winrt::single_threaded_vector<winrt::hstring>();

    // https://learn.microsoft.com/en-us/windows/win32/properties/props-system-devices-dnssd-domain
    props.Append(L"System.Devices.AepService.ProtocolId");  // guid
    props.Append(L"System.Devices.Dnssd.HostName");         // string
    props.Append(L"System.Devices.Dnssd.FullName");         // string
    props.Append(L"System.Devices.Dnssd.ServiceName");      // string
    props.Append(L"System.Devices.Dnssd.Domain");           // string
    props.Append(L"System.Devices.Dnssd.InstanceName");     // string
    props.Append(L"System.Devices.IpAddress");              // multivalue string
    props.Append(L"System.Devices.Dnssd.PortNumber");       // uint16_t
    props.Append(L"System.Devices.Dnssd.TextAttributes");   // multivalue string

    m_deviceWatcher = enumeration::DeviceInformation::CreateWatcher(
        query, 
        props, 
        enumeration::DeviceInformationKind::AssociationEndpointService);

    // add event handlers
    m_deviceWatcherAddedToken = m_deviceWatcher.Added({ this, &CMidi2NetworkMidiEndpointManager::OnDeviceWatcherAdded });
    m_deviceWatcherUpdatedToken = m_deviceWatcher.Updated({ this, &CMidi2NetworkMidiEndpointManager::OnDeviceWatcherUpdated });
    m_deviceWatcherRemovedToken = m_deviceWatcher.Removed({ this, &CMidi2NetworkMidiEndpointManager::OnDeviceWatcherRemoved });
    m_deviceWatcherStoppedToken = m_deviceWatcher.Stopped({ this, &CMidi2NetworkMidiEndpointManager::OnDeviceWatcherStopped });

    // start the watcher
    m_deviceWatcher.Start();

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

// Reads one key from the DNS-SD TXT record. Spec section 4.4 defines UMPEndpointName and
// ProductInstanceId. RFC 6763 makes TXT keys case-insensitive, so the comparison is too.
static bool TryGetDnssdTextAttribute(
    _In_ enumeration::DeviceInformation const& device,
    _In_ std::wstring const& key,
    _Out_ std::wstring& value)
{
    value.clear();

    const winrt::hstring textAttributesPropertyKey = L"System.Devices.Dnssd.TextAttributes";

    if (!device.Properties().HasKey(textAttributesPropertyKey))
    {
        return false;
    }

    auto prop = device.Properties().Lookup(textAttributesPropertyKey);

    if (!prop)
    {
        return false;
    }

    try
    {
        auto attributes = prop.as<foundation::IReferenceArray<winrt::hstring>>();

        winrt::com_array<winrt::hstring> entries;
        attributes.GetStringArray(entries);

        auto lowerKey = internal::ToLowerTrimmedWStringCopy(key);

        for (auto const& entry : entries)
        {
            std::wstring text{ entry };

            auto separator = text.find(L'=');

            if (separator == std::wstring::npos)
            {
                continue;
            }

            if (internal::ToLowerTrimmedWStringCopy(text.substr(0, separator)) == lowerKey)
            {
                value = text.substr(separator + 1);

                return true;
            }
        }
    }
    catch (...)
    {
        LOG_CAUGHT_EXCEPTION();
    }

    return false;
}

// Matches a configured client entry against a discovered host. The mDNS device id is opaque and
// changes between machines, so the advertised Product Instance Id and UMP Endpoint Name are
// accepted too. All comparisons are case-insensitive.
static bool TryFindAdvertisedHost(
    _In_ std::map<winrt::hstring, enumeration::DeviceInformation> const& advertisedHosts,
    _In_ winrt::hstring const& matchId,
    _Out_ enumeration::DeviceInformation& found)
{
    found = nullptr;

    if (matchId.empty())
    {
        return false;
    }

    auto wanted = internal::ToLowerTrimmedWStringCopy(std::wstring{ matchId });

    for (auto const& entry : advertisedHosts)
    {
        if (internal::ToLowerTrimmedWStringCopy(std::wstring{ entry.first }) == wanted)
        {
            found = entry.second;

            return true;
        }

        std::wstring value{ };

        if (TryGetDnssdTextAttribute(entry.second, L"ProductInstanceId", value) &&
            internal::ToLowerTrimmedWStringCopy(value) == wanted)
        {
            found = entry.second;

            return true;
        }

        if (TryGetDnssdTextAttribute(entry.second, L"UMPEndpointName", value) &&
            internal::ToLowerTrimmedWStringCopy(value) == wanted)
        {
            found = entry.second;

            return true;
        }
    }

    return false;
}

_Use_decl_annotations_
HRESULT
CMidi2NetworkMidiEndpointManager::OnDeviceWatcherAdded(enumeration::DeviceWatcher const&, enumeration::DeviceInformation const& args)
{
    TraceLoggingWrite(
        MidiNetworkMidiTransportTelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_INFO,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this"),
        TraceLoggingWideString(L"Enter", MIDI_TRACE_EVENT_MESSAGE_FIELD),
        TraceLoggingWideString(args.Id().c_str(), "id"),
        TraceLoggingWideString(args.Name().c_str(), "name")
    );

    // TODO: Search our host entries to make sure the host is not *this* host

    std::wstring advertisedEndpointName{ };
    std::wstring advertisedProductInstanceId{ };

    TryGetDnssdTextAttribute(args, L"UMPEndpointName", advertisedEndpointName);
    TryGetDnssdTextAttribute(args, L"ProductInstanceId", advertisedProductInstanceId);

    TraceLoggingWrite(
        MidiNetworkMidiTransportTelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_INFO,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this"),
        TraceLoggingWideString(L"Discovered advertised host", MIDI_TRACE_EVENT_MESSAGE_FIELD),
        TraceLoggingWideString(advertisedEndpointName.c_str(), "UMP endpoint name"),
        TraceLoggingWideString(advertisedProductInstanceId.c_str(), "product instance id")
    );

    m_foundAdvertisedHosts.insert_or_assign(args.Id(), args);

    // signal the background thread to check the collection?
    //m_backgroundEndpointCreatorThreadWakeup.SetEvent();
    WakeupBackgroundEndpointCreatorThread();

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
CMidi2NetworkMidiEndpointManager::OnDeviceWatcherUpdated(enumeration::DeviceWatcher const&, enumeration::DeviceInformationUpdate const& /*args*/)
{
    //TraceLoggingWrite(
    //    MidiNetworkMidiTransportTelemetryProvider::Provider(),
    //    MIDI_TRACE_EVENT_INFO,
    //    TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
    //    TraceLoggingLevel(WINEVENT_LEVEL_INFO),
    //    TraceLoggingPointer(this, "this"),
    //    TraceLoggingWideString(L"Enter", MIDI_TRACE_EVENT_MESSAGE_FIELD),
    //    TraceLoggingWideString(args.Id().c_str(), "id")
    //);

    // nothing to do here. We don't care about updates. This gets really spammy because
    // the endpoint updates maybe with each mdns ad broadcast or something

    return S_OK;
}

_Use_decl_annotations_
HRESULT
CMidi2NetworkMidiEndpointManager::OnDeviceWatcherRemoved(enumeration::DeviceWatcher const&, enumeration::DeviceInformationUpdate const& args)
{
    TraceLoggingWrite(
        MidiNetworkMidiTransportTelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_INFO,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this"),
        TraceLoggingWideString(L"Enter", MIDI_TRACE_EVENT_MESSAGE_FIELD),
        TraceLoggingWideString(args.Id().c_str(), "id")
    );

    if (m_foundAdvertisedHosts.find(args.Id()) != m_foundAdvertisedHosts.end())
    {
        m_foundAdvertisedHosts.erase(args.Id());

        // we don't disconnect or anything here. That's handled in-protocol.
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
CMidi2NetworkMidiEndpointManager::OnDeviceWatcherStopped(enumeration::DeviceWatcher const&, foundation::IInspectable const&)
{
    TraceLoggingWrite(
        MidiNetworkMidiTransportTelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_INFO,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this"),
        TraceLoggingWideString(L"Enter", MIDI_TRACE_EVENT_MESSAGE_FIELD)
    );

    // nothing to do here
    return S_OK;
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
    std::shared_ptr<MidiNetworkClientDefinition> clientDefinition, 
    winrt::hstring const& hostNameOrIPAddress, 
    uint16_t const hostPort)
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

    // TODO: Need a lock in here to make sure two passes of the creation
    // loop aren't both trying to create the same client

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

    if (clientDefinition->LocalProductInstanceId.empty())
    {
        clientDefinition->LocalProductInstanceId = L"unspecified-" + root;
    }

    if (clientDefinition->LocalEndpointName.empty())
    {
        clientDefinition->LocalEndpointName = root;
    }


    auto client = std::make_shared<MidiNetworkClient>();
    RETURN_IF_NULL_ALLOC(client);

    auto initHr = client->Initialize(*clientDefinition);
    RETURN_IF_FAILED(initHr);

    // != 0 for the hostPort is hacky, but for MIDI, we shouldn't expect ports < 1024 anyway
    if (!hostNameOrIPAddress.empty() && hostPort != 0)
    {
        HostName hostName(hostNameOrIPAddress);
        winrt::hstring portNumberString = winrt::to_hstring(hostPort);

        auto startHr = client->Start(hostName, portNumberString);
        RETURN_IF_FAILED(startHr);

        // Add the client and mark as created so we don't try to process it again
        TransportState::Current().AddClient(client);
        clientDefinition->Created = true;

        return S_OK;
    }

    return E_FAIL;
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


    // the first time this starts up, we delay for a bit. This is a hack
    // but right now, the service is doing way too much immediately. Having
    // devices connect immediately just adds to the contention. This needs
    // to be removed before this is production-ready
//    Sleep(3000);

    winrt::init_apartment();

    // this is set up to run through one time before waiting for the wakeup
    // this way we can process anything added before the EndpointManager has been
    // initialized

    while (!stopToken.stop_requested())
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

        // Negotiations queued from the socket receive callback. Drained into a local copy so
        // the lock is not held across a call into the service.
        std::vector<std::wstring> negotiations;

        {
            auto lock = m_pendingNegotiationsLock.lock();
            negotiations.swap(m_pendingNegotiations);
        }

        for (auto const& endpointDeviceInterfaceId : negotiations)
        {
            LOG_IF_FAILED(InitiateDiscoveryAndNegotiation(endpointDeviceInterfaceId));
        }

        // Connections released by the receive path. Shutdown joins their worker threads, so it
        // happens here rather than stalling the socket callback.
        std::vector<std::shared_ptr<MidiNetworkConnection>> shutdowns;

        {
            auto lock = m_pendingConnectionShutdownsLock.lock();
            shutdowns.swap(m_pendingConnectionShutdowns);
        }

        for (auto const& connection : shutdowns)
        {
            LOG_IF_FAILED(connection->Shutdown());
        }

        // run through host entries

        for (auto& definition : TransportState::Current().GetPendingHostDefinitions())
        {
            if (definition->Created)
            {
                continue;
            }

            if (!definition->IsEnabled)
            {
                continue;
            }

            auto host = std::make_shared<MidiNetworkHost>();
            LOG_IF_NULL_ALLOC(host);

            if (host != nullptr)
            {
                LOG_IF_FAILED(host->Initialize(*definition));

                if (!host->HasStarted())
                {
                    LOG_IF_FAILED(host->Start());
                }

                definition->Created = true;

                // this ensures the host doesn't disappear
                TransportState::Current().AddHost(host);
            }
        }

        // Run through client definition entries. These aren't actual clients
        // but are instead just parameters needed to create connections to hosts when
        // they come online.

        for (auto const& clientDefinition : TransportState::Current().GetPendingClientDefinitions())
        {
            if (clientDefinition->Created)
            {
                continue;
            }

            if (!clientDefinition->Enabled)
            {
                continue;
            }

            // --- connect via mDNS entry
            if (!clientDefinition->MatchId.empty())
            {
                enumeration::DeviceInformation advertisedHost{ nullptr };

                if (TryFindAdvertisedHost(m_foundAdvertisedHosts, clientDefinition->MatchId, advertisedHost))
                {
                    TraceLoggingWrite(
                        MidiNetworkMidiTransportTelemetryProvider::Provider(),
                        MIDI_TRACE_EVENT_INFO,
                        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
                        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
                        TraceLoggingPointer(this, "this"),
                        TraceLoggingWideString(L"Processing mdns entry", MIDI_TRACE_EVENT_MESSAGE_FIELD),
                        TraceLoggingWideString(advertisedHost.Id().c_str(), "id")
                    );

                    winrt::hstring hostNameOrIPAddress{};
                    uint16_t port{ 0 };

                    const winrt::hstring hostNamePropertyKey = L"System.Devices.Dnssd.HostName";
                    const winrt::hstring hostPortPropertyKey = L"System.Devices.Dnssd.PortNumber";
                    const winrt::hstring ipAddressPropertyKey = L"System.Devices.IpAddress";

                    // we use IP address first, as that is the most reliable
                    if (advertisedHost.Properties().HasKey(ipAddressPropertyKey))
                    {
                        auto prop = advertisedHost.Properties().Lookup(ipAddressPropertyKey).as<foundation::IReferenceArray<winrt::hstring>>();
                        winrt::com_array<winrt::hstring> array;
                        prop.GetStringArray(array);

                        // we only take the top one right now. We should take the others as well
                        if (array.size() > 0)
                        {
                            hostNameOrIPAddress = array.at(0);
                        }
                    }
                    // next we get the host name if necessary, but this relies on DNS being set up properly,
                    // which is often not the case on a network with just some devices and a laptop
                    else if (hostNameOrIPAddress.empty() && advertisedHost.Properties().HasKey(hostNamePropertyKey))
                    {
                        auto prop = advertisedHost.Properties().Lookup(hostNamePropertyKey);

                        if (prop)
                        {
                            hostNameOrIPAddress = winrt::unbox_value<winrt::hstring>(prop);
                        }
                    }

                    // we always need the port
                    if (advertisedHost.Properties().HasKey(hostPortPropertyKey))
                    {
                        auto prop = advertisedHost.Properties().Lookup(hostPortPropertyKey);

                        if (prop)
                        {
                            port = winrt::unbox_value<uint16_t>(prop);
                        }
                    }

                    LOG_IF_FAILED(StartNewClient(clientDefinition, hostNameOrIPAddress, port));
                }
            }

            // --- connect via direct host information / ip
            else if (!clientDefinition->MatchDirectPort.empty())
            {
                // TODO: Check to make sure we've waited at least the minimum probe interval before checking these

                uint16_t port{ 0 };
                wchar_t* end;
                auto bigport = wcstoul(clientDefinition->MatchDirectPort.c_str(), &end, 10);

                // If port number is 0 or > int16.max then error out
                if (bigport == 0 || bigport > UINT16_MAX)
                {
                    LOG_IF_FAILED(E_INVALIDARG);
                    // TODO: report the error
                    continue;
                }

                port = static_cast<uint16_t>(bigport);


                // we have the required port number, so let's check for either host name or IP address
                
                winrt::hstring hostNameOrIPAddress{ };

                // by IP address
                if (!clientDefinition->MatchDirectHostNameOrIPAddress.empty())
                {
                    TraceLoggingWrite(
                        MidiNetworkMidiTransportTelemetryProvider::Provider(),
                        MIDI_TRACE_EVENT_INFO,
                        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
                        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
                        TraceLoggingPointer(this, "this"),
                        TraceLoggingWideString(L"Processing direct connection entry", MIDI_TRACE_EVENT_MESSAGE_FIELD),
                        TraceLoggingWideString(clientDefinition->MatchDirectHostNameOrIPAddress.c_str(), "remote IP address"),
                        TraceLoggingWideString(clientDefinition->MatchDirectPort.c_str(), "remote port")
                    );

                    hostNameOrIPAddress = clientDefinition->MatchDirectHostNameOrIPAddress;

                }
                // by host name
                //else if (!clientDefinition->MatchDirectHostName.empty())
                //{
                //    TraceLoggingWrite(
                //        MidiNetworkMidiTransportTelemetryProvider::Provider(),
                //        MIDI_TRACE_EVENT_INFO,
                //        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
                //        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
                //        TraceLoggingPointer(this, "this"),
                //        TraceLoggingWideString(L"Processing direct connection entry", MIDI_TRACE_EVENT_MESSAGE_FIELD),
                //        TraceLoggingWideString(clientDefinition->MatchDirectHostName.c_str(), "remote host name"),
                //        TraceLoggingWideString(clientDefinition->MatchDirectPort.c_str(), "remote port")
                //    );

                //    hostNameOrIPAddress = clientDefinition->MatchDirectHostName;
                //}

                // TODO: Check to see if the client is actually online

                LOG_IF_FAILED(StartNewClient(clientDefinition, hostNameOrIPAddress, port));
            }
        }

        // wait for notification of new hosts online or new entries added via config
        // the most time we wait is the DirectConnectionScanInterval
        m_backgroundEndpointCreatorThreadWakeup.wait(TransportState::Current().TransportSettings.DirectConnectionScanInterval);
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
CMidi2NetworkMidiEndpointManager::CreateParentDeviceForClients()
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

_Use_decl_annotations_
HRESULT
CMidi2NetworkMidiEndpointManager::CreateParentDeviceForHost(
    winrt::hstring const& name,
    winrt::hstring const& serviceInstanceId,
    std::wstring& createdNewDeviceInstanceId
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

    RETURN_HR_IF_NULL(E_UNEXPECTED, m_midiDeviceManager);

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

    // NOTE: This will fail if the parent device already exists. Since there's no function to
    // remove the virtual parent currently in the MIDI Device Manager, this will fail the second
    // time it is called (so after a Stop and then start)
    auto activateHr = m_midiDeviceManager->ActivateVirtualParentDevice(
        0,
        nullptr,
        &createInfo,
        &newParentDeviceId
    );
    RETURN_IF_FAILED(activateHr);


    createdNewDeviceInstanceId = newParentDeviceId.get();
    //createdNewDeviceInstanceId = parentDeviceId;

    TraceLoggingWrite(
        MidiNetworkMidiTransportTelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_INFO,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this"),
        TraceLoggingWideString(parentDeviceId.c_str(), "New parent device instance id")
    );

    return S_OK;
}

_Use_decl_annotations_
HRESULT
CMidi2NetworkMidiEndpointManager::DeleteParentHostDevice(
    std::wstring const& deviceInstanceId)
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

    auto instanceId = deviceInstanceId;

    if (!instanceId.empty())
    {
        // this will remove all child endpoints
        // NOTE: There's no device manager function to remove the parent, yet.
        RETURN_IF_FAILED(m_midiDeviceManager->RemoveEndpoint(instanceId.c_str()));
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
CMidi2NetworkMidiEndpointManager::QueueConnectionShutdown(
    std::shared_ptr<MidiNetworkConnection> connection
)
{
    RETURN_HR_IF_NULL(E_INVALIDARG, connection);

    {
        auto lock = m_pendingConnectionShutdownsLock.lock();
        m_pendingConnectionShutdowns.push_back(connection);
    }

    LOG_IF_FAILED(WakeupBackgroundEndpointCreatorThread());

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

    LOG_IF_FAILED(WakeupBackgroundEndpointCreatorThread());

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
    std::wstring& createdNewDeviceInstanceId,
    std::wstring& createdNewEndpointDeviceInterfaceId
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

    //DEVPROP_BOOLEAN devPropTrue = DEVPROP_TRUE;
    //   DEVPROP_BOOLEAN devPropFalse = DEVPROP_FALSE;

//    std::wstring endpointDescription = definition->EndpointDescription;

    // no user or in-protocol data in this case
    std::wstring friendlyName = endpointName;


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


    std::wstring instanceId = BuildEndpointDeviceInstanceId(endpointName, remoteEndpointProductInstanceId);

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

    MIDIENDPOINTCOMMONPROPERTIES commonProperties{};
    commonProperties.TransportId = TRANSPORT_LAYER_GUID;
    commonProperties.EndpointDeviceType = MidiEndpointDeviceType::MidiEndpointDeviceType_Normal;
    commonProperties.FriendlyName = friendlyName.c_str();
    commonProperties.TransportCode = transportCode.c_str();
    commonProperties.EndpointName = endpointName.c_str();
    commonProperties.EndpointDescription = endpointDescription.c_str();
    commonProperties.CustomEndpointName = nullptr;
    commonProperties.CustomEndpointDescription = nullptr;
    commonProperties.UniqueIdentifier = remoteEndpointProductInstanceId.c_str();
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

    if (m_deviceWatcher)
    {
        if (m_deviceWatcherStoppedToken)
        {
            m_deviceWatcher.Stopped(m_deviceWatcherStoppedToken);
        }

        if (m_deviceWatcherAddedToken)
        {
            m_deviceWatcher.Added(m_deviceWatcherAddedToken);
        }

        if (m_deviceWatcherRemovedToken)
        {
            m_deviceWatcher.Removed(m_deviceWatcherRemovedToken);
        }

        if (m_deviceWatcherUpdatedToken)
        {
            m_deviceWatcher.Updated(m_deviceWatcherUpdatedToken);
        }

        m_deviceWatcher.Stop();
    }

    m_foundAdvertisedHosts.clear();

    m_backgroundEndpointCreatorThread.request_stop();
    m_backgroundEndpointCreatorThreadWakeup.SetEvent();

    if (m_backgroundEndpointCreatorThread.joinable() && m_backgroundEndpointCreatorThread.get_id() != std::this_thread::get_id())
    {
        m_backgroundEndpointCreatorThread.join();
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
