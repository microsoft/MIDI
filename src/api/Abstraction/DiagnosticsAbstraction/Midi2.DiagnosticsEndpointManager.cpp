// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://github.com/microsoft/MIDI/
// ============================================================================


#include "pch.h"
//#include "midi2.DiagnosticsAbstraction.h"

using namespace wil;
using namespace Microsoft::WRL;
using namespace Microsoft::WRL::Wrappers;

#define MAX_DEVICE_ID_LEN 200 // size in chars


GUID AbstractionLayerGUID = __uuidof(Midi2DiagnosticsAbstraction);

_Use_decl_annotations_
HRESULT
CMidi2DiagnosticsEndpointManager::Initialize(
    IUnknown* MidiDeviceManager,
    IUnknown* /*midiEndpointProtocolManager*/,
    LPCWSTR /*ConfigurationJson*/

)
{
    OutputDebugString(L"" __FUNCTION__ " Enter");

    TraceLoggingWrite(
        MidiDiagnosticsAbstractionTelemetryProvider::Provider(),
        __FUNCTION__,
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this")
    );

    RETURN_HR_IF(E_INVALIDARG, nullptr == MidiDeviceManager);

    RETURN_IF_FAILED(MidiDeviceManager->QueryInterface(__uuidof(IMidiDeviceManagerInterface), (void**)&m_MidiDeviceManager));

    m_TransportAbstractionId = AbstractionLayerGUID;    // this is needed so MidiSrv can instantiate the correct transport
    m_ContainerId = m_TransportAbstractionId;           // we use the transport ID as the container ID for convenience

    RETURN_IF_FAILED(CreateParentDevice());

    RETURN_IF_FAILED(CreateLoopbackEndpoint(DEFAULT_LOOPBACK_BIDI_A_ID, DEFAULT_LOOPBACK_BIDI_A_NAME, MidiFlow::MidiFlowBidirectional));
    RETURN_IF_FAILED(CreateLoopbackEndpoint(DEFAULT_LOOPBACK_BIDI_B_ID, DEFAULT_LOOPBACK_BIDI_B_NAME, MidiFlow::MidiFlowBidirectional));

    RETURN_IF_FAILED(CreatePingEndpoint(DEFAULT_PING_BIDI_ID, DEFAULT_PING_BIDI_NAME, MidiFlow::MidiFlowBidirectional));


    return S_OK;
}


HRESULT
CMidi2DiagnosticsEndpointManager::CreateParentDevice()
{
    OutputDebugString(L"" __FUNCTION__);

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

    const ULONG deviceIdMaxSize = 255;
    wchar_t newDeviceId[deviceIdMaxSize]{ 0 };

    RETURN_IF_FAILED(m_MidiDeviceManager->ActivateVirtualParentDevice(
        0,
        nullptr,
        &createInfo,
        (PWSTR)newDeviceId,
        deviceIdMaxSize
        ));

    m_parentDeviceId = std::wstring(newDeviceId);

    OutputDebugString(__FUNCTION__ L" New parent device id: ");
    OutputDebugString(newDeviceId);
    OutputDebugString(L"\n");

    return S_OK;
}


// This creates the standard loopback interfaces. These are always
// present on the system and have known IDs. Additional ones cannot
// be created by the API. Virtual devices have far more flexibility
// for that use-case

_Use_decl_annotations_
HRESULT 
CMidi2DiagnosticsEndpointManager::CreateLoopbackEndpoint(
    std::wstring const InstanceId,
    std::wstring const Name,
    MidiFlow const Flow
)
{
    OutputDebugString(__FUNCTION__ L": Enter\n");

    //put all of the devproperties we want into arrays and pass into ActivateEndpoint:

    std::wstring mnemonic(TRANSPORT_MNEMONIC);

    DEVPROP_BOOLEAN devPropTrue = DEVPROP_TRUE;
    DEVPROP_BOOLEAN devPropFalse = DEVPROP_FALSE;
    BYTE nativeDataFormat = MIDI_PROP_NATIVEDATAFORMAT_UMP;
    uint32_t supportedDataFormat = (BYTE)MidiDataFormat::MidiDataFormat_UMP;

    std::wstring description = L"Diagnostics loopback endpoint. For testing purposes.";

    auto endpointPurpose = (uint32_t)MidiEndpointDevicePurposePropertyValue::DiagnosticLoopback;

    OutputDebugString(__FUNCTION__ L": Building DEVPROPERTY interfaceDevProperties[]\n");

    DEVPROPERTY interfaceDevProperties[] = {
        {{PKEY_MIDI_AssociatedUMP, DEVPROP_STORE_SYSTEM, nullptr},
            DEVPROP_TYPE_EMPTY, 0, nullptr},

        {{PKEY_MIDI_SupportedDataFormats, DEVPROP_STORE_SYSTEM, nullptr},
            DEVPROP_TYPE_BYTE, static_cast<ULONG>(sizeof(BYTE)), &supportedDataFormat},


        {{DEVPKEY_DeviceInterface_FriendlyName, DEVPROP_STORE_SYSTEM, nullptr},
            DEVPROP_TYPE_STRING, static_cast<ULONG>((Name.length() + 1) * sizeof(WCHAR)), (PVOID)Name.c_str()},
        {{PKEY_MIDI_TransportSuppliedEndpointName, DEVPROP_STORE_SYSTEM, nullptr},
            DEVPROP_TYPE_STRING, static_cast<ULONG>((Name.length() + 1) * sizeof(WCHAR)), (PVOID)Name.c_str()},


        {{PKEY_MIDI_EndpointRequiresMetadataHandler, DEVPROP_STORE_SYSTEM, nullptr},
            DEVPROP_TYPE_BOOLEAN, static_cast<ULONG>(sizeof(devPropTrue)), &devPropTrue},           


        {{PKEY_MIDI_EndpointDevicePurpose, DEVPROP_STORE_SYSTEM, nullptr},
            DEVPROP_TYPE_UINT32, static_cast<ULONG>(sizeof(endpointPurpose)),(PVOID)&endpointPurpose},
        {{PKEY_MIDI_UserSuppliedDescription, DEVPROP_STORE_SYSTEM, nullptr},
            DEVPROP_TYPE_STRING, static_cast<ULONG>((description.length() + 1) * sizeof(WCHAR)), (PVOID)description.c_str() },
        {{PKEY_MIDI_NativeDataFormat, DEVPROP_STORE_SYSTEM, nullptr},
            DEVPROP_TYPE_BYTE, static_cast<ULONG>(sizeof(BYTE)), (PVOID)&nativeDataFormat},

        // do not generate incoming (from device) timestamps automatically.
        // for the loopback endpoints, we expect a zero timestamp to come back through as zero
        {{PKEY_MIDI_GenerateIncomingTimestamp, DEVPROP_STORE_SYSTEM, nullptr},
            DEVPROP_TYPE_BOOLEAN, static_cast<ULONG>(sizeof(devPropFalse)), (PVOID)&devPropFalse},


        {{PKEY_MIDI_AbstractionLayer, DEVPROP_STORE_SYSTEM, nullptr},
            DEVPROP_TYPE_GUID, static_cast<ULONG>(sizeof(GUID)), (PVOID)&AbstractionLayerGUID },        // essential to instantiate the right endpoint types
        {{PKEY_MIDI_TransportMnemonic, DEVPROP_STORE_SYSTEM, nullptr},
            DEVPROP_TYPE_STRING, static_cast<ULONG>((mnemonic.length() + 1) * sizeof(WCHAR)), (PVOID)mnemonic.c_str()},
    };

    DEVPROPERTY deviceDevProperties[] = {
        {{DEVPKEY_Device_PresenceNotForDevice, DEVPROP_STORE_SYSTEM, nullptr},
            DEVPROP_TYPE_BOOLEAN, static_cast<ULONG>(sizeof(devPropTrue)), &devPropTrue},
        {{DEVPKEY_Device_NoConnectSound, DEVPROP_STORE_SYSTEM, nullptr},
            DEVPROP_TYPE_BOOLEAN, static_cast<ULONG>(sizeof(devPropTrue)),&devPropTrue}
    };

    OutputDebugString(__FUNCTION__ L": Building SW_DEVICE_CREATE_INFO\n");

    SW_DEVICE_CREATE_INFO createInfo = {};
    createInfo.cbSize = sizeof(createInfo);

    createInfo.pszInstanceId = InstanceId.c_str();
    createInfo.CapabilityFlags = SWDeviceCapabilitiesNone;
    createInfo.pszDeviceDescription = Name.c_str();


    const ULONG deviceInterfaceIdMaxSize = 255;
    wchar_t newDeviceInterfaceId[deviceInterfaceIdMaxSize]{ 0 };


    OutputDebugString(__FUNCTION__ L": About to ActivateEndpoint\n");

    RETURN_IF_FAILED(m_MidiDeviceManager->ActivateEndpoint(
        (PCWSTR)m_parentDeviceId.c_str(),                       // parent instance Id
        true,                                                   // UMP-only
        Flow,                                                   // MIDI Flow
        ARRAYSIZE(interfaceDevProperties),
        ARRAYSIZE(deviceDevProperties),
        (PVOID)interfaceDevProperties,
        (PVOID)deviceDevProperties,
        (PVOID)&createInfo,
        (LPWSTR)&newDeviceInterfaceId,
        deviceInterfaceIdMaxSize));


    // now delete all the properties that have been discovered in-protocol
    // we have to do this because they end up cached by PNP and come back
    // when you recreate a device with the same Id. This is a real problem 
    // if you are testing function blocks or endpoint properties with this
    // loopback transport.
    m_MidiDeviceManager->DeleteAllEndpointInProtocolDiscoveredProperties(newDeviceInterfaceId);

    // TODO: Invoke the protocol negotiator to now capture updated endpoint info.
    

    
    
    
    // todo: store the interface id and use it for matches later instead of the current partial string match

    OutputDebugString(__FUNCTION__ L": Complete\n");

    return S_OK;
}

_Use_decl_annotations_
HRESULT
CMidi2DiagnosticsEndpointManager::CreatePingEndpoint(
    std::wstring const InstanceId,
    std::wstring const Name, 
    MidiFlow const Flow
)
{
    OutputDebugString(L"" __FUNCTION__);

    //put all of the devproperties we want into arrays and pass into ActivateEndpoint:

    std::wstring mnemonic(TRANSPORT_MNEMONIC);

    DEVPROP_BOOLEAN devPropTrue = DEVPROP_TRUE;
    DEVPROP_BOOLEAN devPropFalse = DEVPROP_FALSE;
    BYTE nativeDataFormat = MIDI_PROP_NATIVEDATAFORMAT_UMP;
    BYTE supportedDataFormat = (BYTE)MidiDataFormat::MidiDataFormat_UMP;

    auto endpointPurpose = (uint32_t)MidiEndpointDevicePurposePropertyValue::DiagnosticPing;

    std::wstring description = L"Internal UMP Ping endpoint. Do not send messages to this endpoint.";

    DEVPROPERTY interfaceDevProperties[] = {
        {{DEVPKEY_DeviceInterface_FriendlyName, DEVPROP_STORE_SYSTEM, nullptr},
            DEVPROP_TYPE_STRING, static_cast<ULONG>((Name.length() + 1) * sizeof(WCHAR)), (PVOID)Name.c_str()},
        {{PKEY_MIDI_TransportSuppliedEndpointName, DEVPROP_STORE_SYSTEM, nullptr},
            DEVPROP_TYPE_STRING, static_cast<ULONG>((Name.length() + 1) * sizeof(WCHAR)), (PVOID)Name.c_str()},

        {{PKEY_MIDI_EndpointRequiresMetadataHandler, DEVPROP_STORE_SYSTEM, nullptr},
            DEVPROP_TYPE_BOOLEAN, static_cast<ULONG>(sizeof(devPropFalse)), &devPropFalse},

        {{PKEY_MIDI_SupportedDataFormats, DEVPROP_STORE_SYSTEM, nullptr},
            DEVPROP_TYPE_BYTE, static_cast<ULONG>(sizeof(BYTE)), &supportedDataFormat},



        {{PKEY_MIDI_AbstractionLayer, DEVPROP_STORE_SYSTEM, nullptr},
            DEVPROP_TYPE_GUID, static_cast<ULONG>(sizeof(GUID)), (PVOID)&AbstractionLayerGUID },        // essential to instantiate the right endpoint types
        {{PKEY_MIDI_EndpointDevicePurpose, DEVPROP_STORE_SYSTEM, nullptr},
            DEVPROP_TYPE_UINT32, static_cast<ULONG>(sizeof(endpointPurpose)),(PVOID)&endpointPurpose},
        {{PKEY_MIDI_UserSuppliedDescription, DEVPROP_STORE_SYSTEM, nullptr},
            DEVPROP_TYPE_STRING, static_cast<ULONG>((description.length() + 1) * sizeof(WCHAR)), (PVOID)description.c_str() },
        {{PKEY_MIDI_NativeDataFormat, DEVPROP_STORE_SYSTEM, nullptr},
            DEVPROP_TYPE_BYTE, static_cast<ULONG>(sizeof(BYTE)), (PVOID)&nativeDataFormat},
        {{PKEY_MIDI_TransportMnemonic, DEVPROP_STORE_SYSTEM, nullptr},
            DEVPROP_TYPE_STRING, static_cast<ULONG>((mnemonic.length() + 1) * sizeof(WCHAR)), (PVOID)mnemonic.c_str()}


    };


    DEVPROPERTY deviceDevProperties[] = {
        {{DEVPKEY_Device_PresenceNotForDevice, DEVPROP_STORE_SYSTEM, nullptr},
            DEVPROP_TYPE_BOOLEAN, static_cast<ULONG>(sizeof(devPropTrue)), &devPropTrue},
        {{DEVPKEY_Device_NoConnectSound, DEVPROP_STORE_SYSTEM, nullptr},
            DEVPROP_TYPE_BOOLEAN, static_cast<ULONG>(sizeof(devPropTrue)),&devPropTrue}
    };

    SW_DEVICE_CREATE_INFO createInfo = {};
    createInfo.cbSize = sizeof(createInfo);

    createInfo.pszInstanceId = InstanceId.c_str();
    createInfo.CapabilityFlags = SWDeviceCapabilitiesNone;
    createInfo.pszDeviceDescription = Name.c_str();


    const ULONG deviceInterfaceIdMaxSize = 255;
    wchar_t newDeviceInterfaceId[deviceInterfaceIdMaxSize]{ 0 };

    RETURN_IF_FAILED(m_MidiDeviceManager->ActivateEndpoint(
        m_parentDeviceId.c_str(),                               // parent instance Id
        true,                                                   // UMP-only
        Flow,                                                   // MIDI Flow
        ARRAYSIZE(interfaceDevProperties),
        ARRAYSIZE(deviceDevProperties),
        (PVOID)interfaceDevProperties,
        (PVOID)deviceDevProperties,
        (PVOID)&createInfo,
        (LPWSTR)&newDeviceInterfaceId,
        deviceInterfaceIdMaxSize));

    // TODO: Get the device interface id and store it for comparison later.

    OutputDebugString(__FUNCTION__ L": Complete\n");

    return S_OK;
}

HRESULT
CMidi2DiagnosticsEndpointManager::Cleanup()
{
    OutputDebugString(L"" __FUNCTION__ " Enter");

    TraceLoggingWrite(
        MidiDiagnosticsAbstractionTelemetryProvider::Provider(),
        __FUNCTION__,
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this")
    );

    return S_OK;
}
