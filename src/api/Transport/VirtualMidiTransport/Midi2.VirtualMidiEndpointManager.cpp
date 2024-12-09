// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================


#include "pch.h"
#include "midi2.VirtualMidiTransport.h"

using namespace wil;
using namespace Microsoft::WRL;
using namespace Microsoft::WRL::Wrappers;

#define MAX_DEVICE_ID_LEN 200 // size in chars

GUID TransportLayerGUID = __uuidof(Midi2VirtualMidiTransport);


_Use_decl_annotations_
HRESULT
CMidi2VirtualMidiEndpointManager::Initialize(
    IMidiDeviceManagerInterface* midiDeviceManager, 
    IMidiEndpointProtocolManagerInterface* midiEndpointProtocolManager
)
{
    TraceLoggingWrite(
        MidiVirtualMidiTransportTelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_INFO,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this")
    );

    RETURN_HR_IF(E_INVALIDARG, nullptr == midiDeviceManager);
    RETURN_HR_IF(E_INVALIDARG, nullptr == midiEndpointProtocolManager);

    RETURN_IF_FAILED(midiDeviceManager->QueryInterface(__uuidof(IMidiDeviceManagerInterface), (void**)&m_MidiDeviceManager));
    RETURN_IF_FAILED(midiEndpointProtocolManager->QueryInterface(__uuidof(IMidiEndpointProtocolManagerInterface), (void**)&m_MidiProtocolManager));


    m_ContainerId = TRANSPORT_LAYER_GUID;           // we use the transport ID as the container ID for convenience

    RETURN_IF_FAILED(CreateParentDevice());

    return S_OK;
}



_Use_decl_annotations_
HRESULT
CMidi2VirtualMidiEndpointManager::DeleteClientEndpoint(std::wstring clientShortInstanceId)
{
    TraceLoggingWrite(
        MidiVirtualMidiTransportTelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_INFO,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this"),
        TraceLoggingWideString(clientShortInstanceId.c_str(), "clientShortInstanceId")
    );

    // get the instance id for the device

    if (m_MidiDeviceManager != nullptr)
    {
        //auto instanceId = GetSwdPropertyInstanceId(clientEndpointInterfaceId);
        auto instanceId = internal::NormalizeDeviceInstanceIdWStringCopy(clientShortInstanceId);

        if (!instanceId.empty())
        {
            RETURN_IF_FAILED(m_MidiDeviceManager->RemoveEndpoint(instanceId.c_str()));
        }
        else
        {
            TraceLoggingWrite(
                MidiVirtualMidiTransportTelemetryProvider::Provider(),
                MIDI_TRACE_EVENT_ERROR,
                TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
                TraceLoggingLevel(WINEVENT_LEVEL_ERROR),
                TraceLoggingPointer(this, "this"),
                TraceLoggingWideString(L"could not find instanceId property for client", MIDI_TRACE_EVENT_MESSAGE_FIELD)
            );

            RETURN_IF_FAILED(E_INVALIDARG);
        }
    }
    else
    {
        // null device manager
        RETURN_IF_FAILED(E_POINTER);
    }

    return S_OK;

}

_Use_decl_annotations_
HRESULT
CMidi2VirtualMidiEndpointManager::DeleteDeviceEndpoint(std::wstring deviceShortInstanceId)
{
    TraceLoggingWrite(
        MidiVirtualMidiTransportTelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_INFO,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this"),
        TraceLoggingWideString(deviceShortInstanceId.c_str(), "deviceShortInstanceId")
    );

    // get the instance id for the device

    if (m_MidiDeviceManager != nullptr)
    {
        //auto instanceId = GetSwdPropertyInstanceId(clientEndpointInterfaceId);
        auto instanceId = internal::NormalizeDeviceInstanceIdWStringCopy(deviceShortInstanceId);

        if (!instanceId.empty())
        {
            RETURN_IF_FAILED(m_MidiDeviceManager->RemoveEndpoint(instanceId.c_str()));
        }
        else
        {
            TraceLoggingWrite(
                MidiVirtualMidiTransportTelemetryProvider::Provider(),
                MIDI_TRACE_EVENT_ERROR,
                TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
                TraceLoggingLevel(WINEVENT_LEVEL_ERROR),
                TraceLoggingPointer(this, "this"),
                TraceLoggingWideString(L"could not find instanceId property for device", MIDI_TRACE_EVENT_MESSAGE_FIELD)
            );

            RETURN_IF_FAILED(E_INVALIDARG);
        }
    }
    else
    {
        // null device manager
        RETURN_IF_FAILED(E_POINTER);
    }

    return S_OK;
}



HRESULT
CMidi2VirtualMidiEndpointManager::CreateParentDevice()
{
    TraceLoggingWrite(
        MidiVirtualMidiTransportTelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_INFO,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this")
    );

    // the parent device parameters are set by the transport (this)
    std::wstring parentDeviceName{ TRANSPORT_PARENT_DEVICE_NAME };
    std::wstring parentDeviceId{ internal::NormalizeDeviceInstanceIdWStringCopy(TRANSPORT_PARENT_ID) };

    SW_DEVICE_CREATE_INFO createInfo = {};
    createInfo.cbSize = sizeof(createInfo);
    createInfo.pszInstanceId = parentDeviceId.c_str();
    createInfo.CapabilityFlags = SWDeviceCapabilitiesNone;
    createInfo.pszDeviceDescription = parentDeviceName.c_str();
    createInfo.pContainerId = &m_ContainerId;

    wil::unique_cotaskmem_string newDeviceId;

    RETURN_IF_FAILED(m_MidiDeviceManager->ActivateVirtualParentDevice(
        0,
        nullptr,
        &createInfo,
        &newDeviceId));

    m_parentDeviceId = internal::NormalizeDeviceInstanceIdWStringCopy(newDeviceId.get());


    TraceLoggingWrite(
        MidiVirtualMidiTransportTelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_INFO,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this"),
        TraceLoggingWideString(newDeviceId.get(), "New parent device instance id")
    );

    return S_OK;
}



_Use_decl_annotations_
HRESULT
CMidi2VirtualMidiEndpointManager::NegotiateAndRequestMetadata(std::wstring endpointId)
{
    TraceLoggingWrite(
        MidiVirtualMidiTransportTelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_INFO,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this"),
        TraceLoggingWideString(endpointId.c_str(), "endpoint id")
    );

    ENDPOINTPROTOCOLNEGOTIATIONPARAMS negotiationParams{ };

    negotiationParams.PreferredMidiProtocol = MIDI_PROP_CONFIGURED_PROTOCOL_MIDI2;
    negotiationParams.PreferToSendJitterReductionTimestampsToEndpoint = false;
    negotiationParams.PreferToReceiveJitterReductionTimestampsFromEndpoint = false;

    RETURN_IF_FAILED(m_MidiProtocolManager->DiscoverAndNegotiate(
        TransportLayerGUID,
        endpointId.c_str(),
        negotiationParams
    ));


    return S_OK;
}

_Use_decl_annotations_
HRESULT 
CMidi2VirtualMidiEndpointManager::CreateClientVisibleEndpoint(
    MidiVirtualDeviceEndpointEntry& entry
)
{
    TraceLoggingWrite(
        MidiVirtualMidiTransportTelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_INFO,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this"),
        TraceLoggingWideString(L"Enter", MIDI_TRACE_EVENT_MESSAGE_FIELD)
    );

    //put all of the devproperties we want into arrays and pass into ActivateEndpoint:

    std::wstring transportCode{ TRANSPORT_CODE };

    //DEVPROP_BOOLEAN devPropTrue = DEVPROP_TRUE;
    //   DEVPROP_BOOLEAN devPropFalse = DEVPROP_FALSE;

    std::wstring endpointName = entry.BaseEndpointName;
    std::wstring endpointDescription = entry.Description;

    std::vector<DEVPROPERTY> interfaceDeviceProperties{};

    // no user or in-protocol data in this case
    std::wstring friendlyName = internal::CalculateEndpointDevicePrimaryName(endpointName, L"", L"");

    // this is needed for the loopback endpoints to have a relationship with each other
    interfaceDeviceProperties.push_back(DEVPROPERTY{ {PKEY_MIDI_VirtualMidiEndpointAssociator, DEVPROP_STORE_SYSTEM, nullptr},
        DEVPROP_TYPE_STRING, (ULONG)(sizeof(wchar_t) * (entry.VirtualEndpointAssociationId.length() + 1)), (PVOID)entry.VirtualEndpointAssociationId.c_str() });

    SW_DEVICE_CREATE_INFO createInfo = {};
    createInfo.cbSize = sizeof(createInfo);

    std::wstring instanceId = internal::NormalizeDeviceInstanceIdWStringCopy(MIDI_VIRT_INSTANCE_ID_CLIENT_PREFIX + entry.ShortUniqueId);

    createInfo.pszInstanceId = instanceId.c_str();
    createInfo.CapabilityFlags = SWDeviceCapabilitiesNone;
    createInfo.pszDeviceDescription = endpointName.c_str();

    wil::unique_cotaskmem_string newDeviceInterfaceId;

    MIDIENDPOINTCOMMONPROPERTIES commonProperties{};
    commonProperties.TransportId = TRANSPORT_LAYER_GUID;
    commonProperties.EndpointDeviceType = MidiEndpointDeviceType_Normal;
    commonProperties.FriendlyName = friendlyName.c_str();
    commonProperties.TransportCode = transportCode.c_str();
    commonProperties.EndpointName = endpointName.c_str();
    commonProperties.EndpointDescription = endpointDescription.c_str();
    commonProperties.CustomEndpointName = nullptr;
    commonProperties.CustomEndpointDescription = nullptr;
    commonProperties.UniqueIdentifier = entry.ShortUniqueId.c_str();
    commonProperties.ManufacturerName = nullptr;
    commonProperties.SupportedDataFormats = MidiDataFormats::MidiDataFormats_UMP;
    commonProperties.NativeDataFormat = MidiDataFormats::MidiDataFormats_UMP;

    UINT32 capabilities {0};
    capabilities |= MidiEndpointCapabilities_SupportsMidi1Protocol;
    capabilities |= MidiEndpointCapabilities_SupportsMidi2Protocol;
    capabilities |= MidiEndpointCapabilities_SupportsMultiClient;
    capabilities |= MidiEndpointCapabilities_GenerateIncomingTimestamps;
    commonProperties.Capabilities = (MidiEndpointCapabilities) capabilities;

    RETURN_IF_FAILED(m_MidiDeviceManager->ActivateEndpoint(
        (PCWSTR)m_parentDeviceId.c_str(),                       // parent instance Id
        entry.UMPOnly,                                          // UMP-only. When set to false, WinMM-visible ports are created for older apps
        MidiFlow::MidiFlowBidirectional,                        // MIDI Flow
        &commonProperties,
        (ULONG)interfaceDeviceProperties.size(),
        (ULONG)0,
        interfaceDeviceProperties.data(),
        nullptr,
        &createInfo,
        &newDeviceInterfaceId));


     // we need this for removal later
    entry.CreatedShortClientInstanceId = instanceId;
    entry.CreatedClientEndpointId = internal::NormalizeEndpointInterfaceIdWStringCopy(newDeviceInterfaceId.get());
    entry.MidiClientBiDi = nullptr;

    // time to do protocol negotiation, request endpoint metadata, function blocks, etc.

    LOG_IF_FAILED(NegotiateAndRequestMetadata(newDeviceInterfaceId.get()));


    TraceLoggingWrite(
        MidiVirtualMidiTransportTelemetryProvider::Provider(),
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
CMidi2VirtualMidiEndpointManager::CreateDeviceSideEndpoint(
    MidiVirtualDeviceEndpointEntry &entry
)
{
    TraceLoggingWrite(
        MidiVirtualMidiTransportTelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_INFO,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this")
    );

    //put all of the devproperties we want into arrays and pass into ActivateEndpoint:

    std::wstring transportCode{ TRANSPORT_CODE };

    //DEVPROP_BOOLEAN devPropTrue = DEVPROP_TRUE;
    //DEVPROP_BOOLEAN devPropFalse = DEVPROP_FALSE;

    // TODO: These should come from localized resources
    std::wstring endpointName = internal::TrimmedWStringCopy(entry.BaseEndpointName + L" (Virtual MIDI Device)");
    std::wstring endpointDescription = internal::TrimmedWStringCopy(entry.Description + L" (This endpoint for use only by the device host application.)");

    std::vector<DEVPROPERTY> interfaceDeviceProperties{};

    // no user or in-protocol data in this case
    std::wstring friendlyName = internal::CalculateEndpointDevicePrimaryName(endpointName, L"", L"");


    interfaceDeviceProperties.push_back(DEVPROPERTY{ { PKEY_MIDI_VirtualMidiEndpointAssociator, DEVPROP_STORE_SYSTEM, nullptr},
        DEVPROP_TYPE_STRING, (ULONG)(sizeof(wchar_t) * (entry.VirtualEndpointAssociationId.length() + 1)), (PVOID)entry.VirtualEndpointAssociationId.c_str() });


    SW_DEVICE_CREATE_INFO createInfo = {};
    createInfo.cbSize = sizeof(createInfo);


    std::wstring instanceId = internal::NormalizeDeviceInstanceIdWStringCopy(MIDI_VIRT_INSTANCE_ID_DEVICE_PREFIX + entry.ShortUniqueId);

    createInfo.pszInstanceId = instanceId.c_str();
    createInfo.CapabilityFlags = SWDeviceCapabilitiesNone;
    createInfo.pszDeviceDescription = endpointName.c_str();

    wil::unique_cotaskmem_string newDeviceInterfaceId;

    MIDIENDPOINTCOMMONPROPERTIES commonProperties{};
    commonProperties.TransportId = TRANSPORT_LAYER_GUID;
    commonProperties.EndpointDeviceType = MidiEndpointDeviceType_VirtualDeviceResponder;
    commonProperties.FriendlyName = friendlyName.c_str();
    commonProperties.TransportCode = transportCode.c_str();
    commonProperties.EndpointName = endpointName.c_str();
    commonProperties.EndpointDescription = endpointDescription.c_str();
    commonProperties.CustomEndpointName = nullptr;
    commonProperties.CustomEndpointDescription = nullptr;
    commonProperties.UniqueIdentifier = entry.ShortUniqueId.c_str();
    commonProperties.SupportedDataFormats = MidiDataFormats::MidiDataFormats_UMP;
    commonProperties.NativeDataFormat = MidiDataFormats::MidiDataFormats_UMP;

    UINT32 capabilities {0};
    capabilities |= MidiEndpointCapabilities_SupportsMidi1Protocol;
    capabilities |= MidiEndpointCapabilities_SupportsMidi2Protocol;
    capabilities |= MidiEndpointCapabilities_GenerateIncomingTimestamps;
    commonProperties.Capabilities = (MidiEndpointCapabilities) capabilities;

    RETURN_IF_FAILED(m_MidiDeviceManager->ActivateEndpoint(
        (PCWSTR)m_parentDeviceId.c_str(),                       // parent instance Id
        true,                                                   // UMP-only is always true for the device-side
        MidiFlow::MidiFlowBidirectional,                        // MIDI Flow
        &commonProperties,
        (ULONG)interfaceDeviceProperties.size(),
        (ULONG)0,
        interfaceDeviceProperties.data(),
        nullptr,
        &createInfo,
        &newDeviceInterfaceId));


    // we need this for device removal later
    entry.CreatedShortDeviceInstanceId = instanceId;
    entry.CreatedDeviceEndpointId = internal::NormalizeEndpointInterfaceIdWStringCopy(newDeviceInterfaceId.get());
    entry.CreatedClientEndpointId = L"";
    entry.CreatedShortClientInstanceId = L"";
    entry.MidiDeviceBiDi = nullptr;
    entry.MidiClientBiDi = nullptr;

    RETURN_IF_FAILED(TransportState::Current().GetEndpointTable()->AddCreatedEndpointDevice(entry));

    TraceLoggingWrite(
        MidiVirtualMidiTransportTelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_INFO,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this"),
        TraceLoggingWideString(L"Created device-side endpoint", MIDI_TRACE_EVENT_MESSAGE_FIELD)
    );

    return S_OK;

}


HRESULT
CMidi2VirtualMidiEndpointManager::Shutdown()
{
    TraceLoggingWrite(
        MidiVirtualMidiTransportTelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_INFO,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this")
    );

    // destroy and release all the devices we have created

    LOG_IF_FAILED(TransportState::Current().GetEndpointTable()->Shutdown());
    TransportState::Current().Shutdown();

    m_MidiDeviceManager.reset();
    m_MidiProtocolManager.reset();

    return S_OK;
}

