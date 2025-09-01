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
    m_backgroundEndpointCreatorThread.detach();


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

    DWORD nameLen = MAX_COMPUTERNAME_LENGTH + 1;
    std::wstring machineName;
    machineName.reserve(nameLen + 1);
    memset(machineName.data(), 0, MAX_COMPUTERNAME_LENGTH + 1);

    std::wstring root;

    if (GetComputerName(machineName.data(), &nameLen))
    {
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
                // TODO: right now, this is case-sensitive. It needs clean-up.
                if (auto advertised = m_foundAdvertisedHosts.find(clientDefinition->MatchId); advertised != m_foundAdvertisedHosts.end())
                {
                    TraceLoggingWrite(
                        MidiNetworkMidiTransportTelemetryProvider::Provider(),
                        MIDI_TRACE_EVENT_INFO,
                        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
                        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
                        TraceLoggingPointer(this, "this"),
                        TraceLoggingWideString(L"Processing mdns entry", MIDI_TRACE_EVENT_MESSAGE_FIELD),
                        TraceLoggingWideString(advertised->second.Id().c_str(), "id")
                    );

                    winrt::hstring hostNameOrIPAddress{};
                    uint16_t port{ 0 };

                    const winrt::hstring hostNamePropertyKey = L"System.Devices.Dnssd.HostName";
                    const winrt::hstring hostPortPropertyKey = L"System.Devices.Dnssd.PortNumber";
                    const winrt::hstring ipAddressPropertyKey = L"System.Devices.IpAddress";

                    // we use IP address first, as that is the most reliable
                    if (advertised->second.Properties().HasKey(ipAddressPropertyKey))
                    {
                        auto prop = advertised->second.Properties().Lookup(ipAddressPropertyKey).as<foundation::IReferenceArray<winrt::hstring>>();
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
                    else if (hostNameOrIPAddress.empty() && advertised->second.Properties().HasKey(hostNamePropertyKey))
                    {
                        auto prop = advertised->second.Properties().Lookup(hostNamePropertyKey);

                        if (prop)
                        {
                            hostNameOrIPAddress = winrt::unbox_value<winrt::hstring>(prop);
                        }
                    }

                    // we always need the port
                    if (advertised->second.Properties().HasKey(hostPortPropertyKey))
                    {
                        auto prop = advertised->second.Properties().Lookup(hostPortPropertyKey);

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

    RETURN_HR_IF_MSG(E_INVALIDARG, endpointName.empty(), "Empty endpoint name");
    RETURN_HR_IF_MSG(E_INVALIDARG, remoteEndpointProductInstanceId.empty(), "Empty product instance id");

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


    std::wstring instancePrefix{ };
    if (thisServiceRole == MidiNetworkConnectionRole::ConnectionWindowsIsHost)
    {
        // we are the host
        instancePrefix = std::wstring{ MIDI_NETWORK_ENDPOINT_INSTANCE_ID_HOST_PREFIX };
    }
    else if(thisServiceRole == MidiNetworkConnectionRole::ConnectionWindowsIsClient)
    {
        // we are the client
        instancePrefix = std::wstring{ MIDI_NETWORK_ENDPOINT_INSTANCE_ID_CLIENT_PREFIX };
    }
    else
    {
        RETURN_IF_FAILED(E_FAIL);
    }


    std::hash<std::wstring> hasher;
    std::wstring hash;
    auto idToHash = hostName.CanonicalName() + remoteEndpointProductInstanceId + networkPort;
    hash = std::to_wstring(hasher(idToHash.c_str()));

    std::wstring instanceId = internal::NormalizeDeviceInstanceIdWStringCopy(
        instancePrefix +
        hash);

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

    RETURN_IF_FAILED(m_midiDeviceManager->ActivateEndpoint(
        (PCWSTR)parent.c_str(),                                 // parent instance Id
        umpOnly,                                                // UMP-only. When set to false, WinMM MIDI 1.0 ports are created
        MidiFlow::MidiFlowBidirectional,                        // MIDI Flow
        &commonProperties,
        (ULONG)interfaceDevProperties.size(),
        (ULONG)0,
        interfaceDevProperties.data(),
        nullptr,
        &createInfo,
        &newDeviceInterfaceId));


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

    m_initialized = false;

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
