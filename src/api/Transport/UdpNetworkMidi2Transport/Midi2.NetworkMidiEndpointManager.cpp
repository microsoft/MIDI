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
    IMidiDeviceManagerInterface* midiDeviceManager,
    IMidiEndpointProtocolManagerInterface* midiEndpointProtocolManager
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

    RETURN_IF_FAILED(midiDeviceManager->QueryInterface(__uuidof(IMidiDeviceManagerInterface), (void**)&m_midiDeviceManager));
    RETURN_IF_FAILED(midiEndpointProtocolManager->QueryInterface(__uuidof(IMidiEndpointProtocolManagerInterface), (void**)&m_midiProtocolManager));

    m_transportId = TRANSPORT_LAYER_GUID;   // this is needed so MidiSrv can instantiate the correct transport
    m_containerId = m_transportId;                           // we use the transport ID as the container ID for convenience

    RETURN_IF_FAILED(CreateParentDevice());

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
    m_backgroundEndpointCreatorThreadWakeup.SetEvent();

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

    std::jthread workerThread(std::bind_front(&CMidi2NetworkMidiEndpointManager::EndpointCreatorWorker, this));

    m_backgroundEndpointCreatorThread = std::move(workerThread);
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
    //auto coInit = wil::CoInitializeEx(COINIT_MULTITHREADED);

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

            if (!definition->Enabled)
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

                // Remove pending entry

                definition->Created = true;

                // this ensures the host doesn't disappear
                TransportState::Current().AddHost(host);
            }
        }

        // TODO: run through client definition entries. These aren't actual clients
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

            // any requiring a match, see if we have a match in our deviceinformation collection
            // - any that are IP/port direct, try to connect, but don't keep trying the same ones if they fail

            // check for id first, as this is fastest
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

                auto client = std::make_shared<MidiNetworkClient>();
                LOG_IF_NULL_ALLOC(client);

                auto initHr = client->Initialize(*clientDefinition, advertised->second);
                LOG_IF_FAILED(initHr);

                if (SUCCEEDED(initHr))
                {
                    TransportState::Current().AddClient(client);

                    // todo: get the hostname and port, and then start the client

                    //deviceId = advertised.Id();
                    //deviceName = advertised.Name();

                    //host.HostName = internal::GetDeviceInfoProperty<winrt::hstring>(entry.Properties(), L"System.Devices.Dnssd.HostName", L"");
                    //host.FullName = internal::GetDeviceInfoProperty<winrt::hstring>(entry.Properties(), L"System.Devices.Dnssd.FullName", L"");
                    //host.ServiceInstanceName = internal::GetDeviceInfoProperty<winrt::hstring>(entry.Properties(), L"System.Devices.Dnssd.InstanceName", L"");
                    //host.Port = internal::GetDeviceInfoProperty<uint16_t>(entry.Properties(), L"System.Devices.Dnssd.PortNumber", 0);

                    winrt::hstring hostNameString{};
                    uint16_t port{ 0 };

                    const winrt::hstring hostNamePropertyKey = L"System.Devices.Dnssd.HostName";
                    //const winrt::hstring hostNamePropertyKey = L"System.Devices.Dnssd.FullName";
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
                            hostNameString = array.at(0);
                        }
                    }
                    // next we get the host name, but this relies on DNS being set up properly,
                    // which is often not the case on a network with just some devices and a laptop
                    else if (advertised->second.Properties().HasKey(hostNamePropertyKey))
                    {
                        auto prop = advertised->second.Properties().Lookup(hostNamePropertyKey);

                        if (prop)
                        {
                            hostNameString = winrt::unbox_value<winrt::hstring>(prop);
                        }
                    }

                    if (advertised->second.Properties().HasKey(hostPortPropertyKey))
                    {
                        auto prop = advertised->second.Properties().Lookup(hostPortPropertyKey);

                        if (prop)
                        {
                            port = winrt::unbox_value<uint16_t>(prop);
                        }
                    }

                    // the == 0 is hacky, but for midi, anything under 1024 is likely bogus
                    if (!hostNameString.empty() && port != 0)
                    {

                        HostName hostName(hostNameString);
                        winrt::hstring portNameString = winrt::to_hstring(port);

                        auto startHr = client->Start(hostName, portNameString);

                        LOG_IF_FAILED(startHr);

                        if (SUCCEEDED(startHr))
                        {
                            // Mark as created so we don't try to process it again
                            clientDefinition->Created = true;
                        }
                        else
                        {
                            TraceLoggingWrite(
                                MidiNetworkMidiTransportTelemetryProvider::Provider(),
                                MIDI_TRACE_EVENT_ERROR,
                                TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
                                TraceLoggingLevel(WINEVENT_LEVEL_ERROR),
                                TraceLoggingPointer(this, "this"),
                                TraceLoggingWideString(L"Failed to start client", MIDI_TRACE_EVENT_MESSAGE_FIELD),
                                TraceLoggingWideString(advertised->second.Id().c_str(), "id"),
                                TraceLoggingWideString(hostNameString.c_str(), "hostname string"),
                                TraceLoggingWideString(hostName.ToString().c_str(), "created hostname"),
                                TraceLoggingWideString(portNameString.c_str(), "port")
                            );
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
                            TraceLoggingWideString(L"Unable to resolve remote device", MIDI_TRACE_EVENT_MESSAGE_FIELD),
                            TraceLoggingWideString(advertised->second.Id().c_str(), "id")
                        );
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
                        TraceLoggingWideString(L"Failed to initialize client", MIDI_TRACE_EVENT_MESSAGE_FIELD),
                        TraceLoggingWideString(advertised->second.Id().c_str(), "id")
                    );

                }

            }

        }

        // TODO: wait for notification of new hosts online or new entries added via config
        m_backgroundEndpointCreatorThreadWakeup.wait(10000);
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
CMidi2NetworkMidiEndpointManager::CreateParentDevice()
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
    std::wstring parentDeviceName{ TRANSPORT_PARENT_DEVICE_NAME };
    std::wstring parentDeviceId{ internal::NormalizeDeviceInstanceIdWStringCopy(TRANSPORT_PARENT_ID) };

    SW_DEVICE_CREATE_INFO createInfo = {};
    createInfo.cbSize = sizeof(createInfo);
    createInfo.pszInstanceId = parentDeviceId.c_str();
    createInfo.CapabilityFlags = SWDeviceCapabilitiesNone;
    createInfo.pszDeviceDescription = parentDeviceName.c_str();
    createInfo.pContainerId = &m_containerId;

    LPWSTR newDeviceId;

    RETURN_IF_FAILED(m_midiDeviceManager->ActivateVirtualParentDevice(
        0,
        nullptr,
        &createInfo,
        (LPWSTR*)&newDeviceId
    ));

    m_parentDeviceId = internal::NormalizeDeviceInstanceIdWStringCopy(newDeviceId);

    TraceLoggingWrite(
        MidiNetworkMidiTransportTelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_INFO,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this"),
        TraceLoggingWideString(newDeviceId, "New parent device instance id")
    );

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


_Use_decl_annotations_
HRESULT
CMidi2NetworkMidiEndpointManager::CreateNewEndpoint(
    MidiNetworkConnectionRole thisServiceRole,
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




    UNREFERENCED_PARAMETER(hostName);
    UNREFERENCED_PARAMETER(networkPort);



    std::wstring transportCode(TRANSPORT_CODE);

    //DEVPROP_BOOLEAN devPropTrue = DEVPROP_TRUE;
    //   DEVPROP_BOOLEAN devPropFalse = DEVPROP_FALSE;

//    std::wstring endpointDescription = definition->EndpointDescription;

    std::vector<DEVPROPERTY> interfaceDeviceProperties{};

    // no user or in-protocol data in this case
    std::wstring friendlyName = internal::CalculateEndpointDevicePrimaryName(endpointName, L"", L"");


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

    // TODO: We can have multiple devices with the same unique ID, so need 
    // to change this logic to check for that and create a new one, maybe
    // with the host name (if a string is available) and port?
    std::wstring instanceId = internal::NormalizeDeviceInstanceIdWStringCopy(
        instancePrefix +
        remoteEndpointProductInstanceId);

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


    // TODO: Add custom properties for the network information

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

    RETURN_IF_FAILED(m_midiDeviceManager->ActivateEndpoint(
        (PCWSTR)m_parentDeviceId.c_str(),                       // parent instance Id
        umpOnly,                                                // UMP-only. When set to false, WinMM MIDI 1.0 ports are created
        MidiFlow::MidiFlowBidirectional,                        // MIDI Flow
        &commonProperties,
        (ULONG)0, //interfaceDeviceProperties.size(),
        (ULONG)0,
        nullptr, //interfaceDeviceProperties.data(),
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
    createdNewDeviceInstanceId = instanceId;
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
