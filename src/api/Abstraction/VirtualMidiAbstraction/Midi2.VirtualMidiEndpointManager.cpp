// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================


#include "pch.h"
#include "midi2.VirtualMidiAbstraction.h"

using namespace wil;
using namespace Microsoft::WRL;
using namespace Microsoft::WRL::Wrappers;

#define MAX_DEVICE_ID_LEN 200 // size in chars

GUID AbstractionLayerGUID = __uuidof(Midi2VirtualMidiAbstraction);


_Use_decl_annotations_
HRESULT
CMidi2VirtualMidiEndpointManager::Initialize(
    IUnknown* MidiDeviceManager, 
    IUnknown* MidiEndpointProtocolManager
)
{
    TraceLoggingWrite(
        MidiVirtualMidiAbstractionTelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_INFO,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this")
    );

    RETURN_HR_IF(E_INVALIDARG, nullptr == MidiDeviceManager);
    RETURN_HR_IF(E_INVALIDARG, nullptr == MidiEndpointProtocolManager);

    RETURN_IF_FAILED(MidiDeviceManager->QueryInterface(__uuidof(IMidiDeviceManagerInterface), (void**)&m_MidiDeviceManager));
    RETURN_IF_FAILED(MidiEndpointProtocolManager->QueryInterface(__uuidof(IMidiEndpointProtocolManagerInterface), (void**)&m_MidiProtocolManager));


    m_ContainerId = ABSTRACTION_LAYER_GUID;           // we use the transport ID as the container ID for convenience

    RETURN_IF_FAILED(CreateParentDevice());

    return S_OK;
}



_Use_decl_annotations_
HRESULT
CMidi2VirtualMidiEndpointManager::DeleteClientEndpoint(std::wstring clientShortInstanceId)
{
    TraceLoggingWrite(
        MidiVirtualMidiAbstractionTelemetryProvider::Provider(),
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
                MidiVirtualMidiAbstractionTelemetryProvider::Provider(),
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
        MidiVirtualMidiAbstractionTelemetryProvider::Provider(),
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
                MidiVirtualMidiAbstractionTelemetryProvider::Provider(),
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
        MidiVirtualMidiAbstractionTelemetryProvider::Provider(),
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

    const ULONG deviceIdMaxSize = 255;
    wchar_t newDeviceId[deviceIdMaxSize]{ 0 };

    RETURN_IF_FAILED(m_MidiDeviceManager->ActivateVirtualParentDevice(
        0,
        nullptr,
        &createInfo,
        (PWSTR)newDeviceId,
        deviceIdMaxSize
    ));

    m_parentDeviceId = internal::NormalizeDeviceInstanceIdWStringCopy(newDeviceId);


    TraceLoggingWrite(
        MidiVirtualMidiAbstractionTelemetryProvider::Provider(),
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
CMidi2VirtualMidiEndpointManager::NegotiateAndRequestMetadata(std::wstring endpointId)
{
    TraceLoggingWrite(
        MidiVirtualMidiAbstractionTelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_INFO,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this"),
        TraceLoggingWideString(endpointId.c_str(), "endpoint id")
    );

    ENDPOINTPROTOCOLNEGOTIATIONPARAMS negotiationParams{ };

    negotiationParams.PreferredMidiProtocol = MIDI_PROP_CONFIGURED_PROTOCOL_MIDI2;
    negotiationParams.PreferToSendJRTimestampsToEndpoint = false;
    negotiationParams.PreferToReceiveJRTimestampsFromEndpoint = false;
    negotiationParams.TimeoutMilliseconds = 3000;

    RETURN_IF_FAILED(m_MidiProtocolManager->DiscoverAndNegotiate(
        AbstractionLayerGUID,
        endpointId.c_str(),
        negotiationParams,
        nullptr                 // TODO: Provide callback
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
        MidiVirtualMidiAbstractionTelemetryProvider::Provider(),
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

    //bool requiresMetadataHandler = true;
    bool multiClient = true;
    bool generateIncomingTimestamps = true;

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


    const ULONG deviceInterfaceIdMaxSize = 255;
    wchar_t newDeviceInterfaceId[deviceInterfaceIdMaxSize]{ 0 };


    MIDIENDPOINTCOMMONPROPERTIES commonProperties;
    commonProperties.AbstractionLayerGuid = ABSTRACTION_LAYER_GUID;
    commonProperties.EndpointPurpose = MidiEndpointDevicePurposePropertyValue::NormalMessageEndpoint;
    commonProperties.FriendlyName = friendlyName.c_str();
    commonProperties.TransportCode = transportCode.c_str();
    commonProperties.TransportSuppliedEndpointName = endpointName.c_str();
    commonProperties.TransportSuppliedEndpointDescription = endpointDescription.c_str();
    commonProperties.UserSuppliedEndpointName = nullptr;
    commonProperties.UserSuppliedEndpointDescription = nullptr;
    commonProperties.UniqueIdentifier = entry.ShortUniqueId.c_str();
    commonProperties.SupportedDataFormats = MidiDataFormat::MidiDataFormat_UMP;
    commonProperties.NativeDataFormat = MIDI_PROP_NATIVEDATAFORMAT_UMP;
    commonProperties.SupportsMultiClient = multiClient;
    //commonProperties.RequiresMetadataHandler = requiresMetadataHandler;
    commonProperties.GenerateIncomingTimestamps = generateIncomingTimestamps;
    commonProperties.ManufacturerName = entry.Manufacturer.c_str();
    commonProperties.SupportsMidi1ProtocolDefaultValue = true;
    commonProperties.SupportsMidi2ProtocolDefaultValue = true;

    RETURN_IF_FAILED(m_MidiDeviceManager->ActivateEndpoint(
        (PCWSTR)m_parentDeviceId.c_str(),                       // parent instance Id
        entry.UMPOnly,                                          // UMP-only. When set to false, WinMM-visible ports are created for older apps
        MidiFlow::MidiFlowBidirectional,                        // MIDI Flow
        &commonProperties,
        (ULONG)interfaceDeviceProperties.size(),
        (ULONG)0,
        (PVOID)interfaceDeviceProperties.data(),
        (PVOID)nullptr,
        (PVOID)&createInfo,
        (LPWSTR)&newDeviceInterfaceId,
        deviceInterfaceIdMaxSize));


     // we need this for removal later
    entry.CreatedShortClientInstanceId = instanceId;
    entry.CreatedClientEndpointId = internal::NormalizeEndpointInterfaceIdWStringCopy(newDeviceInterfaceId);
    entry.MidiClientBiDi = nullptr;

    // time to do protocol negotiation, request endpoint metadata, function blocks, etc.

    LOG_IF_FAILED(NegotiateAndRequestMetadata(newDeviceInterfaceId));


    TraceLoggingWrite(
        MidiVirtualMidiAbstractionTelemetryProvider::Provider(),
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
        MidiVirtualMidiAbstractionTelemetryProvider::Provider(),
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

    //bool requiresMetadataHandler = false;
    bool multiClient = false;
    bool generateIncomingTimestamps = true;

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

    const ULONG deviceInterfaceIdMaxSize = 255;
    wchar_t newDeviceInterfaceId[deviceInterfaceIdMaxSize]{ 0 };


    MIDIENDPOINTCOMMONPROPERTIES commonProperties{};
    commonProperties.AbstractionLayerGuid = ABSTRACTION_LAYER_GUID;
    commonProperties.EndpointPurpose = MidiEndpointDevicePurposePropertyValue::VirtualDeviceResponder;
    commonProperties.FriendlyName = friendlyName.c_str();
    commonProperties.TransportCode = transportCode.c_str();
    commonProperties.TransportSuppliedEndpointName = endpointName.c_str();
    commonProperties.TransportSuppliedEndpointDescription = endpointDescription.c_str();
    commonProperties.UserSuppliedEndpointName = nullptr;
    commonProperties.UserSuppliedEndpointDescription = nullptr;
    commonProperties.UniqueIdentifier = entry.ShortUniqueId.c_str();
    commonProperties.SupportedDataFormats = MidiDataFormat::MidiDataFormat_UMP;
    commonProperties.NativeDataFormat = MIDI_PROP_NATIVEDATAFORMAT_UMP;
    commonProperties.SupportsMultiClient = multiClient;
    //commonProperties.RequiresMetadataHandler = requiresMetadataHandler;
    commonProperties.GenerateIncomingTimestamps = generateIncomingTimestamps;
    commonProperties.ManufacturerName = entry.Manufacturer.c_str();
    commonProperties.SupportsMidi1ProtocolDefaultValue = true;
    commonProperties.SupportsMidi2ProtocolDefaultValue = true;

    RETURN_IF_FAILED(m_MidiDeviceManager->ActivateEndpoint(
        (PCWSTR)m_parentDeviceId.c_str(),                       // parent instance Id
        true,                                                   // UMP-only is always true for the device-side
        MidiFlow::MidiFlowBidirectional,                        // MIDI Flow
        &commonProperties,
        (ULONG)interfaceDeviceProperties.size(),
        (ULONG)0,
        (PVOID)interfaceDeviceProperties.data(),
        (PVOID)nullptr,
        (PVOID)&createInfo,
        (LPWSTR)&newDeviceInterfaceId,
        deviceInterfaceIdMaxSize));


    // we need this for device removal later
    entry.CreatedShortDeviceInstanceId = instanceId;
    entry.CreatedDeviceEndpointId = internal::NormalizeEndpointInterfaceIdWStringCopy(newDeviceInterfaceId);
    entry.CreatedClientEndpointId = L"";
    entry.CreatedShortClientInstanceId = L"";
    entry.MidiDeviceBiDi = nullptr;
    entry.MidiClientBiDi = nullptr;

    RETURN_IF_FAILED(AbstractionState::Current().GetEndpointTable()->AddCreatedEndpointDevice(entry));

    TraceLoggingWrite(
        MidiVirtualMidiAbstractionTelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_INFO,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this"),
        TraceLoggingWideString(L"Created device-side endpoint", MIDI_TRACE_EVENT_MESSAGE_FIELD)
    );

    return S_OK;

}


HRESULT
CMidi2VirtualMidiEndpointManager::Cleanup()
{
    TraceLoggingWrite(
        MidiVirtualMidiAbstractionTelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_INFO,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this")
    );

    // destroy and release all the devices we have created

    LOG_IF_FAILED(AbstractionState::Current().GetEndpointTable()->Cleanup());
    AbstractionState::Current().Cleanup();

    m_MidiDeviceManager.reset();
    m_MidiProtocolManager.reset();

    return S_OK;
}

