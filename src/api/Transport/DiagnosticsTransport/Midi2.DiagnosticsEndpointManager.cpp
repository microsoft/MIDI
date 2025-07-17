// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================


#include "pch.h"
//#include "midi2.DiagnosticsTransport.h"

using namespace wil;
using namespace Microsoft::WRL;
using namespace Microsoft::WRL::Wrappers;

#define MAX_DEVICE_ID_LEN 200 // size in chars

GUID TransportLayerGUID = TRANSPORT_LAYER_GUID;


_Use_decl_annotations_
HRESULT
CMidi2DiagnosticsEndpointManager::Initialize(
    IMidiDeviceManager* midiDeviceManager,
    IMidiEndpointProtocolManager* /*midiEndpointProtocolManager*/

)
{
    TraceLoggingWrite(
        MidiDiagnosticsTransportTelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_INFO,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this")
    );

    RETURN_HR_IF(E_INVALIDARG, nullptr == midiDeviceManager);

    RETURN_IF_FAILED(midiDeviceManager->QueryInterface(__uuidof(IMidiDeviceManager), (void**)&m_MidiDeviceManager));

    m_ContainerId = m_TransportTransportId;           // we use the transport ID as the container ID for convenience

    RETURN_IF_FAILED(CreateParentDevice());

    RETURN_IF_FAILED(CreateLoopbackEndpoint(DEFAULT_LOOPBACK_BIDI_A_ID, LOOPBACK_BIDI_A_UNIQUE_ID, DEFAULT_LOOPBACK_BIDI_A_NAME, MidiFlow::MidiFlowBidirectional));
    RETURN_IF_FAILED(CreateLoopbackEndpoint(DEFAULT_LOOPBACK_BIDI_B_ID, LOOPBACK_BIDI_B_UNIQUE_ID, DEFAULT_LOOPBACK_BIDI_B_NAME, MidiFlow::MidiFlowBidirectional));

    RETURN_IF_FAILED(CreatePingEndpoint(DEFAULT_PING_BIDI_ID, PING_BIDI_UNIQUE_ID, DEFAULT_PING_BIDI_NAME, MidiFlow::MidiFlowBidirectional));


    return S_OK;
}


HRESULT
CMidi2DiagnosticsEndpointManager::CreateParentDevice()
{
    TraceLoggingWrite(
        MidiDiagnosticsTransportTelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_INFO,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this")
    );

    // the parent device parameters are set by the transport (this)
    std::wstring parentDeviceName{ TRANSPORT_PARENT_DEVICE_NAME };
    std::wstring parentDeviceId{ TRANSPORT_PARENT_ID };

    SW_DEVICE_CREATE_INFO createInfo = {};
    createInfo.cbSize = sizeof(createInfo);
    createInfo.pszInstanceId = parentDeviceId.c_str();
    createInfo.CapabilityFlags = SWDeviceCapabilitiesNone;
    createInfo.pszDeviceDescription = parentDeviceName.c_str();
    createInfo.pContainerId = &m_ContainerId;

    //m_ParentDevice = std::make_unique<MidiEndpointParentDeviceInfo>();
    //RETURN_IF_NULL_ALLOC(m_ParentDevice);

    wil::unique_cotaskmem_string newDeviceId;

    RETURN_IF_FAILED(m_MidiDeviceManager->ActivateVirtualParentDevice(
        0,
        nullptr,
        &createInfo,
        &newDeviceId
        ));

    m_parentDeviceId = std::wstring(newDeviceId.get());

    return S_OK;
}


// This creates the standard loopback interfaces. These are always
// present on the system and have known IDs. Additional ones cannot
// be created by the API. Virtual devices have far more flexibility
// for that use-case

_Use_decl_annotations_
HRESULT 
CMidi2DiagnosticsEndpointManager::CreateLoopbackEndpoint(
    std::wstring const instanceId,
    std::wstring const uniqueId,
    std::wstring const name,
    MidiFlow const flow
)
{
    TraceLoggingWrite(
        MidiDiagnosticsTransportTelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_INFO,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this")
    );

    std::wstring transportCode(TRANSPORT_CODE);

    //DEVPROP_BOOLEAN devPropTrue = DEVPROP_TRUE;
    //DEVPROP_BOOLEAN devPropFalse = DEVPROP_FALSE;

    std::wstring endpointName = name;
    std::wstring endpointDescription = L"Diagnostics loopback endpoint. For testing and development purposes.";

    std::vector<DEVPROPERTY> interfaceDeviceProperties{};

    //bool requiresMetadataHandler = true;
    bool multiClient = true;

    // do not generate incoming (from device) timestamps automatically.
    // for the loopback endpoints, we expect a zero timestamp to come back through as zero
    bool generateIncomingTimestamps = false;

    // no user or in-protocol data in this case
    std::wstring friendlyName = internal::CalculateEndpointDevicePrimaryName(endpointName, L"", L"");


    SW_DEVICE_CREATE_INFO createInfo = {};
    createInfo.cbSize = sizeof(createInfo);

    createInfo.pszInstanceId = instanceId.c_str();
    createInfo.CapabilityFlags = SWDeviceCapabilitiesNone;
    createInfo.pszDeviceDescription = name.c_str();

    wil::unique_cotaskmem_string newDeviceInterfaceId;

    MIDIENDPOINTCOMMONPROPERTIES commonProperties{};
    commonProperties.TransportId = m_TransportTransportId;
    commonProperties.EndpointDeviceType = MidiEndpointDeviceType_DiagnosticLoopback;
    commonProperties.FriendlyName = friendlyName.c_str();
    commonProperties.TransportCode = transportCode.c_str();
    commonProperties.EndpointName = endpointName.c_str();
    commonProperties.EndpointDescription = endpointDescription.c_str();
    commonProperties.CustomEndpointName = nullptr;
    commonProperties.CustomEndpointDescription = nullptr;
    commonProperties.UniqueIdentifier = uniqueId.c_str();
    commonProperties.SupportedDataFormats = MidiDataFormats::MidiDataFormats_UMP;
    commonProperties.NativeDataFormat = MidiDataFormats::MidiDataFormats_UMP;
    commonProperties.ManufacturerName = TRANSPORT_MANUFACTURER;

    UINT32 capabilities {0};
    capabilities |= MidiEndpointCapabilities_SupportsMidi1Protocol;
    capabilities |= MidiEndpointCapabilities_SupportsMidi2Protocol;
    if (multiClient)
    {
        capabilities |= MidiEndpointCapabilities_SupportsMultiClient;
    }
    if (generateIncomingTimestamps)
    {
        capabilities |= MidiEndpointCapabilities_GenerateIncomingTimestamps;
    }
    commonProperties.Capabilities = (MidiEndpointCapabilities) capabilities;

    RETURN_IF_FAILED(m_MidiDeviceManager->ActivateEndpoint(
        (PCWSTR)m_parentDeviceId.c_str(),                       // parent instance Id
        true,                                                   // UMP-only
        flow,                                                   // MIDI flow
        &commonProperties,
        (ULONG)interfaceDeviceProperties.size(),
        (ULONG)0,
        interfaceDeviceProperties.data(),
        nullptr,
        &createInfo,
        &newDeviceInterfaceId));

    // TODO: Invoke the protocol negotiator to now capture updated endpoint info.
    

    // todo: store the interface id and use it for matches later instead of the current partial string match

    return S_OK;
}

_Use_decl_annotations_
HRESULT
CMidi2DiagnosticsEndpointManager::CreatePingEndpoint(
    std::wstring const instanceId,
    std::wstring const uniqueId,
    std::wstring const name, 
    MidiFlow const flow
)
{
    TraceLoggingWrite(
        MidiDiagnosticsTransportTelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_INFO,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this")
    );

    //put all of the devproperties we want into arrays and pass into ActivateEndpoint:

    std::wstring transportCode(TRANSPORT_CODE);

    //DEVPROP_BOOLEAN devPropTrue = DEVPROP_TRUE;
    //DEVPROP_BOOLEAN devPropFalse = DEVPROP_FALSE;

    std::wstring endpointName = name;
    std::wstring endpointDescription = L"Internal UMP Ping endpoint. Do not send messages to this endpoint.";

    std::vector<DEVPROPERTY> interfaceDeviceProperties{};

    bool multiClient = true;
    bool generateIncomingTimestamps = true;

    // no user or in-protocol data in this case
    std::wstring friendlyName = internal::CalculateEndpointDevicePrimaryName(endpointName, L"", L"");

    SW_DEVICE_CREATE_INFO createInfo = {};
    createInfo.cbSize = sizeof(createInfo);

    createInfo.pszInstanceId = instanceId.c_str();
    createInfo.CapabilityFlags = SWDeviceCapabilitiesNone;
    createInfo.pszDeviceDescription = name.c_str();

    wil::unique_cotaskmem_string newDeviceInterfaceId;

    MIDIENDPOINTCOMMONPROPERTIES commonProperties{};
    commonProperties.TransportId = m_TransportTransportId;
    commonProperties.EndpointDeviceType = MidiEndpointDeviceType_DiagnosticPing;
    commonProperties.FriendlyName = friendlyName.c_str();
    commonProperties.TransportCode = transportCode.c_str();
    commonProperties.EndpointName = endpointName.c_str();
    commonProperties.EndpointDescription = endpointDescription.c_str();
    commonProperties.CustomEndpointName = nullptr;
    commonProperties.CustomEndpointDescription = nullptr;
    commonProperties.UniqueIdentifier = uniqueId.c_str();
    commonProperties.SupportedDataFormats = MidiDataFormats::MidiDataFormats_UMP;
    commonProperties.NativeDataFormat = MidiDataFormats::MidiDataFormats_UMP;
    commonProperties.ManufacturerName = TRANSPORT_MANUFACTURER;

    UINT32 capabilities {0};

    if (multiClient)
    {
        capabilities |= MidiEndpointCapabilities_SupportsMultiClient;
    }

    if (generateIncomingTimestamps)
    {
        capabilities |= MidiEndpointCapabilities_GenerateIncomingTimestamps;
    }

    commonProperties.Capabilities = (MidiEndpointCapabilities) capabilities;

    RETURN_IF_FAILED(m_MidiDeviceManager->ActivateEndpoint(
        m_parentDeviceId.c_str(),                               // parent instance Id
        true,                                                   // UMP-only
        flow,                                                   // MIDI flow
        &commonProperties,
        (ULONG)interfaceDeviceProperties.size(),
        (ULONG)0,
        interfaceDeviceProperties.data(),
        nullptr,
        &createInfo,
        &newDeviceInterfaceId));

    return S_OK;
}


HRESULT
CMidi2DiagnosticsEndpointManager::Shutdown()
{
    TraceLoggingWrite(
        MidiDiagnosticsTransportTelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_INFO,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this")
    );

    m_MidiDeviceManager.reset();

    return S_OK;
}
