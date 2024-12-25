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

    // TODO: Lock the transport state hosts

    m_initialized = true;

    for (auto const& host : TransportState::Current().GetHosts())
    {
        if (!host->HasStarted())
        {
            RETURN_IF_FAILED(host->Start());
        }
    }

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
    std::wstring const& endpointName,
    std::wstring const& remoteEndpointProductInstanceId,
    winrt::Windows::Networking::HostName const& hostName,
    std::wstring const& networkPort,
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

    // TODO: We can have multiple devices with the same unique ID, so need 
    // to change this logic to check for that and create a new one, maybe
    // with the host name (if a string is available) and port?
    std::wstring instanceId = internal::NormalizeDeviceInstanceIdWStringCopy(
        std::wstring{ MIDI_NETWORK_ENDPOINT_INSTANCE_ID_PREFIX } + 
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



    MIDIENDPOINTCOMMONPROPERTIES commonProperties{};
    commonProperties.TransportId = TRANSPORT_LAYER_GUID;
    commonProperties.EndpointDeviceType = MidiEndpointDeviceType::MidiEndpointDeviceType_Normal;
    commonProperties.FriendlyName = friendlyName.c_str();
    commonProperties.TransportCode = transportCode.c_str();
    commonProperties.EndpointName = endpointName.c_str();
    commonProperties.EndpointDescription = nullptr;
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
        false,                                                  // UMP-only. When set to false, WinMM MIDI 1.0 ports are created
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



    return S_OK;
}
