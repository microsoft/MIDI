// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://github.com/microsoft/MIDI/
// ============================================================================


#include "pch.h"
#include "midi2.NetworkMidiAbstraction.h"

using namespace wil;
using namespace Microsoft::WRL;
using namespace Microsoft::WRL::Wrappers;

#define MAX_DEVICE_ID_LEN 200 // size in chars

//
// KsMidiPort properties
// These properties are related to the SWD created for a MIDI port
//
//DEFINE_DEVPROPKEY(DEVPKEY_KsMidiPort_KsFilterInterfaceId, 0x2bf8a79a, 0x79ff, 0x49bc, 0x91, 0x26, 0x37, 0x28, 0x15, 0xb1, 0x53, 0xc8, 1);     // DEVPROP_TYPE_STRING
//DEFINE_DEVPROPKEY(DEVPKEY_KsMidiPort_KsPinId, 0x2bf8a79a, 0x79ff, 0x49bc, 0x91, 0x26, 0x37, 0x28, 0x15, 0xb1, 0x53, 0xc8, 2);     // DEVPROP_TYPE_UINT32
//DEFINE_DEVPROPKEY(DEVPKEY_Midi_Device_Tag, 0xc659a048, 0x9933, 0x4ec2, 0xa8, 0xc4, 0x5c, 0xcc, 0x98, 0x8e, 0xc9, 0xf5, 1);     // DEVPROP_TYPE_UINT64

_Use_decl_annotations_
HRESULT
CMidi2NetworkMidiEndpointManager::Initialize(
    IUnknown* midiDeviceManager
)
{
    OutputDebugString(L"" __FUNCTION__ " Enter");

    TraceLoggingWrite(
        MidiNetworkMidiAbstractionTelemetryProvider::Provider(),
        __FUNCTION__,
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this")
    );

    RETURN_HR_IF(E_INVALIDARG, nullptr == midiDeviceManager);

    RETURN_IF_FAILED(midiDeviceManager->QueryInterface(__uuidof(IMidiDeviceManagerInterface), (void**)&m_MidiDeviceManager));


    RETURN_IF_FAILED(CreateParentDevice());
    //RETURN_IF_FAILED(CreateEndpoint());

    return S_OK;
}




void SwMidiParentDeviceCreateCallback(__in HSWDEVICE /*hSwDevice*/, __in HRESULT CreationResult, __in_opt PVOID pContext, __in_opt PCWSTR /* pszDeviceInstanceId */)
{
    OutputDebugString(L"" __FUNCTION__ " Enter");


    PPARENTDEVICECREATECONTEXT creationContext = (PPARENTDEVICECREATECONTEXT)pContext;

    // interface registration has started, assume
    // failure
    creationContext->MidiParentDevice->SwDeviceState = SWDEVICESTATE::Failed;

    

    LOG_IF_FAILED(CreationResult);

    //if (SUCCEEDED(CreationResult))
    //{
    //    CreationResult = SwDeviceInterfaceRegister(
    //        hSwDevice,
    //        &(creationContext->MidiPort->InterfaceCategory),
    //        nullptr,
    //        creationContext->IntPropertyCount,
    //        creationContext->InterfaceDevProperties,
    //        TRUE,
    //        wil::out_param(creationContext->MidiPort->DeviceInterfaceId));
    //    LOG_IF_FAILED(CreationResult);
    //}

    if (SUCCEEDED(CreationResult))
    {
        OutputDebugString(L"" __FUNCTION__ " - CreationResult indicates success");

        // success, mark the port as created
        creationContext->MidiParentDevice->SwDeviceState = SWDEVICESTATE::Created;
    }
    else
    {
        OutputDebugString(L"" __FUNCTION__ " - CreationResult indicates FAILURE");
    }

    // success or failure, signal we have completed.
    creationContext->creationCompleted.SetEvent();

    OutputDebugString(L"" __FUNCTION__ " Exit");

}



_Use_decl_annotations_
HRESULT
CMidi2NetworkMidiEndpointManager::CreateParentDevice()
{
    OutputDebugString(L"" __FUNCTION__ " Enter");

    // the parent device parameters are set by the transport (this)

    std::wstring parentDeviceName{ L"MIDI 2.0 Network Transport" };
    std::wstring parentDeviceId{ L"MIDIU_NETWORK_TRANSPORT" };

    SW_DEVICE_CREATE_INFO CreateInfo = {};
    CreateInfo.cbSize = sizeof(CreateInfo);
    CreateInfo.pszInstanceId = parentDeviceId.c_str();
    CreateInfo.CapabilityFlags = SWDeviceCapabilitiesNone;
    CreateInfo.pszDeviceDescription = parentDeviceName.c_str();

    SW_DEVICE_CREATE_INFO* createInfo = (SW_DEVICE_CREATE_INFO*)&CreateInfo;

    if (m_parentDevice != nullptr)
    {
        // already created
        OutputDebugString(L"" __FUNCTION__ " - Parent already created.");

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

    m_parentDevice->InstanceId = createInfo->pszInstanceId;
    //midiPort->MidiFlow = MidiFlow;

    //const GUID* interfaceCategory;
    //if (MidiFlow == MidiFlow::MidiFlowOut)
    //{
    //    interfaceCategory = &DEVINTERFACE_UNIVERSALMIDIPACKET_OUTPUT;
    //}
    //else if (MidiFlow == MidiFlow::MidiFlowIn)
    //{
    //    interfaceCategory = &DEVINTERFACE_UNIVERSALMIDIPACKET_INPUT;
    //}
    //else if (MidiFlow == MidiFlow::MidiFlowBidirectional)
    //{
    //    interfaceCategory = &DEVINTERFACE_UNIVERSALMIDIPACKET_BIDI;
    //}
    //else
    //{
    //    RETURN_IF_FAILED(E_UNEXPECTED);
    //}

    //    midiPort->InterfaceCategory = *interfaceCategory;

    OutputDebugString(L"" __FUNCTION__ " -- Calling SwDeviceCreate");

    GUID transportAbstractionId = __uuidof(Midi2NetworkMidiAbstraction);

    // we use the GUID for this transport COM object as the container id
    GUID containerId = transportAbstractionId;

    std::wstring rootDeviceId = L"HTREE\\ROOT\\0";
    std::wstring enumeratorName = L"MidiSrv";

    createInfo->pContainerId = &containerId;

    RETURN_IF_FAILED(SwDeviceCreate(
        L"MidiSrv",                         // this really should come from the service
        rootDeviceId.c_str(),               // root device
        createInfo, 
        0,                                  // count of properties
        NULL,                               // pointer to properties
        SwMidiParentDeviceCreateCallback,   // callback
        &creationContext,
        wil::out_param(m_parentDevice->SwDevice)));

    // wait for creation to complete
    creationContext.creationCompleted.wait();

       

    // confirm we were able to register the interface
    RETURN_HR_IF(E_FAIL, m_parentDevice->SwDeviceState != SWDEVICESTATE::Created);

    // success, transfer the midiPort to the list
 //   m_MidiPorts.push_back(std::move(midiPort));

    OutputDebugString(L"" __FUNCTION__ " Exit");

    return S_OK;
}




// this will be called from the runtime endpoint creation interface

_Use_decl_annotations_
HRESULT 
CMidi2NetworkMidiEndpointManager::CreateEndpoint()
{
    OutputDebugString(L"" __FUNCTION__ " Enter");

    //std::hash<std::wstring> hasher;
    //std::wstring hash;

    // Each instance should be stored in a endpoint manager-scoped vector of open endpoints

    std::wstring deviceName{ L"MIDI 2.0 Loopback 0001" };
    std::wstring deviceId{ L"MIDIU_LOOPBACK_BIDI" };

    //hash = std::to_wstring(hasher(deviceId));

    // hard-coding a number at the moment. will need to calculate this later.
    std::wstring deviceInstanceId{ L"MIDIU_LOOPBACK_BIDI_INST.0001" };


    OutputDebugString(deviceName.c_str());
    OutputDebugString(deviceId.c_str());
    OutputDebugString(deviceInstanceId.c_str());
    OutputDebugString(m_parentDevice->InstanceId.c_str());


    //put all of the devproperties we want into arrays and pass into ActivateEndpoint:

    OutputDebugString(L"" __FUNCTION__ " - Setting interface dev properties");
    
    // this name property will be provided by the API
    DEVPROPERTY interfaceDevProperties[] = {
        {{DEVPKEY_DeviceInterface_FriendlyName, DEVPROP_STORE_SYSTEM, nullptr},
            DEVPROP_TYPE_STRING, static_cast<ULONG>((deviceName.length() + 1) * sizeof(WCHAR)), (PVOID)deviceName.c_str()}
    };

    DEVPROP_BOOLEAN devPropTrue = DEVPROP_TRUE;

    OutputDebugString(L"" __FUNCTION__ " - Setting device dev properties");

    DEVPROPERTY deviceDevProperties[] = {
        {{DEVPKEY_Device_PresenceNotForDevice, DEVPROP_STORE_SYSTEM, nullptr},
            DEVPROP_TYPE_BOOLEAN, static_cast<ULONG>(sizeof(devPropTrue)), &devPropTrue}
    };

    SW_DEVICE_CREATE_INFO createInfo = {};
    createInfo.cbSize = sizeof(createInfo);

    createInfo.pszInstanceId = deviceInstanceId.c_str();
    createInfo.CapabilityFlags = SWDeviceCapabilitiesNone;
    createInfo.pszDeviceDescription = deviceName.c_str();

    OutputDebugString(L"" __FUNCTION__ " - Activating Endpoint");

    // MIDISRV here comes from the service. If that changes, it needs
    // to be changed here as well.
    RETURN_IF_FAILED(m_MidiDeviceManager->ActivateEndpoint(
        (std::wstring(L"SWD\\MIDISRV\\") + m_parentDevice->InstanceId).c_str(),     // parent instance Id
        true,                                                                       // UMP-only
        MidiFlow::MidiFlowBidirectional,                                            // MIDI Flow
        ARRAYSIZE(interfaceDevProperties),
        ARRAYSIZE(deviceDevProperties),
        (PVOID)interfaceDevProperties,
        (PVOID)deviceDevProperties,
        (PVOID)&createInfo));


    OutputDebugString(L"" __FUNCTION__ " Exit");

    return S_OK;
}



_Use_decl_annotations_
HRESULT
CMidi2NetworkMidiEndpointManager::Cleanup()
{
    OutputDebugString(L"" __FUNCTION__ " Enter");

    TraceLoggingWrite(
        MidiNetworkMidiAbstractionTelemetryProvider::Provider(),
        __FUNCTION__,
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this")
    );


    return S_OK;
}
