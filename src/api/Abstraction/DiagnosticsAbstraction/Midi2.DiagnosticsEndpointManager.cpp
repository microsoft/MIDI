// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://github.com/microsoft/MIDI/
// ============================================================================


#include "pch.h"
#include "midi2.DiagnosticsAbstraction.h"

using namespace wil;
using namespace Microsoft::WRL;
using namespace Microsoft::WRL::Wrappers;

#define MAX_DEVICE_ID_LEN 200 // size in chars


GUID AbstractionLayerGUID = __uuidof(Midi2DiagnosticsAbstraction);

_Use_decl_annotations_
HRESULT
CMidi2DiagnosticsEndpointManager::Initialize(
    IUnknown* midiDeviceManager,
    LPCWSTR /*configurationJson*/

)
{
    TraceLoggingWrite(
        MidiDiagnosticsAbstractionTelemetryProvider::Provider(),
        __FUNCTION__,
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this")
    );

    RETURN_HR_IF(E_INVALIDARG, nullptr == midiDeviceManager);

    RETURN_IF_FAILED(midiDeviceManager->QueryInterface(__uuidof(IMidiDeviceManagerInterface), (void**)&m_MidiDeviceManager));

    m_transportAbstractionId = AbstractionLayerGUID;   // this is needed so MidiSrv can instantiate the correct transport
    m_containerId = m_transportAbstractionId;                           // we use the transport ID as the container ID for convenience

    RETURN_IF_FAILED(CreateParentDevice());

    RETURN_IF_FAILED(CreateLoopbackEndpoint(DEFAULT_LOOPBACK_BIDI_A_ID, DEFAULT_LOOPBACK_BIDI_A_NAME, MidiFlow::MidiFlowBidirectional));
    RETURN_IF_FAILED(CreateLoopbackEndpoint(DEFAULT_LOOPBACK_BIDI_B_ID, DEFAULT_LOOPBACK_BIDI_B_NAME, MidiFlow::MidiFlowBidirectional));
//    RETURN_IF_FAILED(CreateLoopbackEndpoint(DEFAULT_LOOPBACK_OUT_ID, DEFAULT_LOOPBACK_OUT_NAME, MidiFlow::MidiFlowOut));
//    RETURN_IF_FAILED(CreateLoopbackEndpoint(DEFAULT_LOOPBACK_IN_ID, DEFAULT_LOOPBACK_IN_NAME, MidiFlow::MidiFlowIn));

    RETURN_IF_FAILED(CreatePingEndpoint(DEFAULT_PING_BIDI_ID, DEFAULT_PING_BIDI_NAME, MidiFlow::MidiFlowBidirectional));


    return S_OK;
}




void SwMidiParentDeviceCreateCallback(__in HSWDEVICE /*hSwDevice*/, __in HRESULT CreationResult, __in_opt PVOID pContext, __in_opt PCWSTR pszDeviceInstanceId)
{
    if (pContext == nullptr)
    {
        // TODO: Should log this.

        return;
    }

    PPARENTDEVICECREATECONTEXT creationContext = (PPARENTDEVICECREATECONTEXT)pContext;
   

    // interface registration has started, assume failure
    creationContext->MidiParentDevice->SwDeviceState = SWDEVICESTATE::Failed;  

    LOG_IF_FAILED(CreationResult);

    if (SUCCEEDED(CreationResult))
    {
        // success, mark the port as created
        creationContext->MidiParentDevice->SwDeviceState = SWDEVICESTATE::Created;

        // get the new device instance ID. This is usually modified from what we started with
        creationContext->MidiParentDevice->InstanceId = std::wstring(pszDeviceInstanceId);
    }
    else
    {
        OutputDebugString(L"" __FUNCTION__ " - CreationResult FAILURE");
    }

    // success or failure, signal we have completed.
    creationContext->creationCompleted.SetEvent();
}



HRESULT
CMidi2DiagnosticsEndpointManager::CreateParentDevice()
{
    // the parent device parameters are set by the transport (this)

    std::wstring parentDeviceName{ TRANSPORT_PARENT_DEVICE_NAME };
    std::wstring parentDeviceId{ TRANSPORT_PARENT_ID };

    SW_DEVICE_CREATE_INFO CreateInfo = {};
    CreateInfo.cbSize = sizeof(CreateInfo);
    CreateInfo.pszInstanceId = parentDeviceId.c_str();
    CreateInfo.CapabilityFlags = SWDeviceCapabilitiesNone;
    CreateInfo.pszDeviceDescription = parentDeviceName.c_str();

    SW_DEVICE_CREATE_INFO* createInfo = (SW_DEVICE_CREATE_INFO*)&CreateInfo;

    if (m_parentDevice != nullptr)
    {
        return S_OK;
    }

    m_parentDevice = std::make_unique<MidiEndpointParentDeviceInfo>();

    RETURN_IF_NULL_ALLOC(m_parentDevice);

    PARENTDEVICECREATECONTEXT creationContext;

    // lambdas can only be converted to a function pointer if they
    // don't do capture, so copy everything into the CREATECONTEXT
    // to share with the SwDeviceCreate callback.
    creationContext.MidiParentDevice = m_parentDevice.get();

    //creationContext.InterfaceDevProperties = (DEVPROPERTY*)InterfaceDevProperties;
    //creationContext.IntPropertyCount = IntPropertyCount;

    m_parentDevice->SwDeviceState = SWDEVICESTATE::CreatePending;

    //m_parentDevice->InstanceId = createInfo->pszInstanceId;
    
    DEVPROP_BOOLEAN devPropTrue = DEVPROP_TRUE;

    DEVPROPERTY deviceDevProperties[] = {
        {{DEVPKEY_Device_PresenceNotForDevice, DEVPROP_STORE_SYSTEM, nullptr},
            DEVPROP_TYPE_BOOLEAN, static_cast<ULONG>(sizeof(devPropTrue)), &devPropTrue},
        {{DEVPKEY_Device_NoConnectSound, DEVPROP_STORE_SYSTEM, nullptr},
            DEVPROP_TYPE_BOOLEAN, static_cast<ULONG>(sizeof(devPropTrue)),&devPropTrue}
    };


    std::wstring rootDeviceId = LOOPBACK_PARENT_ROOT;
    std::wstring enumeratorName = TRANSPORT_ENUMERATOR;

    createInfo->pContainerId = &m_containerId;

    RETURN_IF_FAILED(SwDeviceCreate(
        enumeratorName.c_str(),             // this really should come from the service
        rootDeviceId.c_str(),               // root device
        createInfo, 
        ARRAYSIZE(deviceDevProperties),     // count of properties
        (DEVPROPERTY*)deviceDevProperties,  // pointer to properties
        SwMidiParentDeviceCreateCallback,   // callback
        &creationContext,
        wil::out_param(m_parentDevice->SwDevice)));

    // wait for creation to complete
    creationContext.creationCompleted.wait();

    // confirm we were able to register the interface
    RETURN_HR_IF(E_FAIL, m_parentDevice->SwDeviceState != SWDEVICESTATE::Created);

    return S_OK;
}


// This creates the standard loopback interfaces. These are always
// present on the system and have known IDs. Additional ones cannot
// be created by the API. Virtual devices have far more flexibility
// for that use-case

_Use_decl_annotations_
HRESULT 
CMidi2DiagnosticsEndpointManager::CreateLoopbackEndpoint(std::wstring const instanceId, std::wstring const name, MidiFlow const flow)
{
    //put all of the devproperties we want into arrays and pass into ActivateEndpoint:

    std::wstring mnemonic(TRANSPORT_MNEMONIC);

    DEVPROP_BOOLEAN devPropTrue = DEVPROP_TRUE;

    DEVPROPERTY interfaceDevProperties[] = {
        {{DEVPKEY_DeviceInterface_FriendlyName, DEVPROP_STORE_SYSTEM, nullptr},
            DEVPROP_TYPE_STRING, static_cast<ULONG>((name.length() + 1) * sizeof(WCHAR)), (PVOID)name.c_str()},
        {{PKEY_MIDI_TransportSuppliedEndpointName, DEVPROP_STORE_SYSTEM, nullptr},
            DEVPROP_TYPE_STRING, static_cast<ULONG>((name.length() + 1) * sizeof(WCHAR)), (PVOID)name.c_str()},
        {{PKEY_MIDI_UmpLoopback, DEVPROP_STORE_SYSTEM, nullptr},
            DEVPROP_TYPE_BOOLEAN, static_cast<ULONG>(sizeof(devPropTrue)),&devPropTrue},
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

    SW_DEVICE_CREATE_INFO createInfo = {};
    createInfo.cbSize = sizeof(createInfo);

    createInfo.pszInstanceId = instanceId.c_str();
    createInfo.CapabilityFlags = SWDeviceCapabilitiesNone;
    createInfo.pszDeviceDescription = name.c_str();


    RETURN_IF_FAILED(m_MidiDeviceManager->ActivateEndpoint(
        std::wstring(m_parentDevice->InstanceId).c_str(),       // parent instance Id
        true,                                                   // UMP-only
        flow,                                                   // MIDI Flow
        ARRAYSIZE(interfaceDevProperties),
        ARRAYSIZE(deviceDevProperties),
        (PVOID)interfaceDevProperties,
        (PVOID)deviceDevProperties,
        (PVOID)&createInfo));

    return S_OK;
}

_Use_decl_annotations_
HRESULT
CMidi2DiagnosticsEndpointManager::CreatePingEndpoint(_In_ std::wstring const instanceId, _In_ std::wstring const name, _In_ MidiFlow const flow)
{
    //put all of the devproperties we want into arrays and pass into ActivateEndpoint:

    std::wstring mnemonic(TRANSPORT_MNEMONIC);

    DEVPROP_BOOLEAN devPropTrue = DEVPROP_TRUE;

    DEVPROPERTY interfaceDevProperties[] = {
        {{DEVPKEY_DeviceInterface_FriendlyName, DEVPROP_STORE_SYSTEM, nullptr},
            DEVPROP_TYPE_STRING, static_cast<ULONG>((name.length() + 1) * sizeof(WCHAR)), (PVOID)name.c_str()},
        {{PKEY_MIDI_TransportSuppliedEndpointName, DEVPROP_STORE_SYSTEM, nullptr},
            DEVPROP_TYPE_STRING, static_cast<ULONG>((name.length() + 1) * sizeof(WCHAR)), (PVOID)name.c_str()},
        {{PKEY_MIDI_AbstractionLayer, DEVPROP_STORE_SYSTEM, nullptr},
            DEVPROP_TYPE_GUID, static_cast<ULONG>(sizeof(GUID)), (PVOID)&AbstractionLayerGUID },        // essential to instantiate the right endpoint types
        {{PKEY_MIDI_UmpPing, DEVPROP_STORE_SYSTEM, nullptr},
            DEVPROP_TYPE_BOOLEAN, static_cast<ULONG>(sizeof(devPropTrue)),&devPropTrue},
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

    createInfo.pszInstanceId = instanceId.c_str();
    createInfo.CapabilityFlags = SWDeviceCapabilitiesNone;
    createInfo.pszDeviceDescription = name.c_str();


    RETURN_IF_FAILED(m_MidiDeviceManager->ActivateEndpoint(
        std::wstring(m_parentDevice->InstanceId).c_str(),       // parent instance Id
        true,                                                   // UMP-only
        flow,                                                   // MIDI Flow
        ARRAYSIZE(interfaceDevProperties),
        ARRAYSIZE(deviceDevProperties),
        (PVOID)interfaceDevProperties,
        (PVOID)deviceDevProperties,
        (PVOID)&createInfo));

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
