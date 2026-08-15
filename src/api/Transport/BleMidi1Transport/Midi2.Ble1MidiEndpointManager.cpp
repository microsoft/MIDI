// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://github.com/microsoft/MIDI/
// ============================================================================


#include "pch.h"
#include "midi2.Ble1MidiTransport.h"

using namespace wil;
using namespace Microsoft::WRL;
using namespace Microsoft::WRL::Wrappers;

#define MAX_DEVICE_ID_LEN 200 // size in chars


_Use_decl_annotations_
HRESULT
CMidi2Ble1MidiEndpointManager::Initialize(
    IMidiDeviceManager* midiDeviceManager,
    IMidiEndpointProtocolManager* midiEndpointProtocolManager
)
{
    TraceLoggingWrite(
        MidiBle1MidiTransportTelemetryProvider::Provider(),
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

    RETURN_IF_FAILED(CreateParentDevice());

    m_initialized = true;

    // start background thread that creates endpoints
    RETURN_IF_FAILED(StartBackgroundEndpointCreator());

    // start the device watcher so we see new hosts come online
    RETURN_IF_FAILED(StartRemoteHostWatcher());

    TraceLoggingWrite(
        MidiBle1MidiTransportTelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_INFO,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this"),
        TraceLoggingWideString(L"Exit", MIDI_TRACE_EVENT_MESSAGE_FIELD)
    );

    return S_OK;
}


HRESULT
CMidi2Ble1MidiEndpointManager::StartRemoteHostWatcher()
{
    TraceLoggingWrite(
        MidiBle1MidiTransportTelemetryProvider::Provider(),
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
    m_deviceWatcherAddedToken = m_deviceWatcher.Added({ this, &CMidi2Ble1MidiEndpointManager::OnDeviceWatcherAdded });
    m_deviceWatcherUpdatedToken = m_deviceWatcher.Updated({ this, &CMidi2Ble1MidiEndpointManager::OnDeviceWatcherUpdated });
    m_deviceWatcherRemovedToken = m_deviceWatcher.Removed({ this, &CMidi2Ble1MidiEndpointManager::OnDeviceWatcherRemoved });
    m_deviceWatcherStoppedToken = m_deviceWatcher.Stopped({ this, &CMidi2Ble1MidiEndpointManager::OnDeviceWatcherStopped });

    // start the watcher
    m_deviceWatcher.Start();

    TraceLoggingWrite(
        MidiBle1MidiTransportTelemetryProvider::Provider(),
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
CMidi2Ble1MidiEndpointManager::OnDeviceWatcherAdded(enumeration::DeviceWatcher const&, enumeration::DeviceInformation const& args)
{
    TraceLoggingWrite(
        MidiBle1MidiTransportTelemetryProvider::Provider(),
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
        MidiBle1MidiTransportTelemetryProvider::Provider(),
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
CMidi2Ble1MidiEndpointManager::OnDeviceWatcherUpdated(enumeration::DeviceWatcher const&, enumeration::DeviceInformationUpdate const& /*args*/)
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
CMidi2Ble1MidiEndpointManager::OnDeviceWatcherRemoved(enumeration::DeviceWatcher const&, enumeration::DeviceInformationUpdate const& args)
{
    TraceLoggingWrite(
        MidiBle1MidiTransportTelemetryProvider::Provider(),
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
        MidiBle1MidiTransportTelemetryProvider::Provider(),
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
CMidi2Ble1MidiEndpointManager::OnDeviceWatcherStopped(enumeration::DeviceWatcher const&, foundation::IInspectable const&)
{
    TraceLoggingWrite(
        MidiBle1MidiTransportTelemetryProvider::Provider(),
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
CMidi2Ble1MidiEndpointManager::WakeupBackgroundEndpointCreatorThread()
{
    TraceLoggingWrite(
        MidiBle1MidiTransportTelemetryProvider::Provider(),
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
CMidi2Ble1MidiEndpointManager::StartBackgroundEndpointCreator()
{
    TraceLoggingWrite(
        MidiBle1MidiTransportTelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_INFO,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this"),
        TraceLoggingWideString(L"Enter", MIDI_TRACE_EVENT_MESSAGE_FIELD)
    );


    m_backgroundEndpointCreatorThread = std::jthread(std::bind_front(&CMidi2Ble1MidiEndpointManager::EndpointCreatorWorker, this));
    m_backgroundEndpointCreatorThread.detach();


    TraceLoggingWrite(
        MidiBle1MidiTransportTelemetryProvider::Provider(),
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
CMidi2Ble1MidiEndpointManager::EndpointCreatorWorker(std::stop_token stopToken)
{
    TraceLoggingWrite(
        MidiBle1MidiTransportTelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_INFO,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this"),
        TraceLoggingWideString(L"Enter", MIDI_TRACE_EVENT_MESSAGE_FIELD)
    );

    UNREFERENCED_PARAMETER(stopToken);

    // the first time this starts up, we delay for a bit. This is a hack
    // but right now, the service is doing way too much immediately. Having
    // devices connect immediately just adds to the contention. This needs
    // to be removed before this is production-ready
//    Sleep(3000);

    winrt::init_apartment();


    return S_OK;
}


HRESULT
CMidi2Ble1MidiEndpointManager::CreateParentDevice()
{
    TraceLoggingWrite(
        MidiBle1MidiTransportTelemetryProvider::Provider(),
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
        MidiBle1MidiTransportTelemetryProvider::Provider(),
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
CMidi2Ble1MidiEndpointManager::DeleteEndpoint(
    std::wstring deviceInstanceId)
{
    TraceLoggingWrite(
        MidiBle1MidiTransportTelemetryProvider::Provider(),
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
            MidiBle1MidiTransportTelemetryProvider::Provider(),
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
CMidi2Ble1MidiEndpointManager::InitiateDiscoveryAndNegotiation(
    std::wstring const& endpointDeviceInterfaceId
)
{
    TraceLoggingWrite(
        MidiBle1MidiTransportTelemetryProvider::Provider(),
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



HRESULT
CMidi2Ble1MidiEndpointManager::Shutdown()
{
    TraceLoggingWrite(
        MidiBle1MidiTransportTelemetryProvider::Provider(),
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
        MidiBle1MidiTransportTelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_INFO,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this"),
        TraceLoggingWideString(L"Exit", MIDI_TRACE_EVENT_MESSAGE_FIELD)
    );


    return S_OK;
}
