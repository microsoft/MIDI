// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://github.com/microsoft/MIDI/
// ============================================================================


#include "pch.h"
#include "midi2.LoopbackMidiAbstraction.h"

using namespace wil;
using namespace Microsoft::WRL;
using namespace Microsoft::WRL::Wrappers;

#define MAX_DEVICE_ID_LEN 200 // size in chars

GUID AbstractionLayerGUID = __uuidof(Midi2LoopbackMidiAbstraction);


_Use_decl_annotations_
HRESULT
CMidi2LoopbackMidiEndpointManager::Initialize(
    IUnknown* MidiDeviceManager, 
    IUnknown* MidiEndpointProtocolManager
)
{
    TraceLoggingWrite(
        MidiLoopbackMidiAbstractionTelemetryProvider::Provider(),
        __FUNCTION__,
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this")
    );

    RETURN_HR_IF(E_INVALIDARG, nullptr == MidiDeviceManager);
    RETURN_HR_IF(E_INVALIDARG, nullptr == MidiEndpointProtocolManager);

    RETURN_IF_FAILED(MidiDeviceManager->QueryInterface(__uuidof(IMidiDeviceManagerInterface), (void**)&m_MidiDeviceManager));
    RETURN_IF_FAILED(MidiEndpointProtocolManager->QueryInterface(__uuidof(IMidiEndpointProtocolManagerInterface), (void**)&m_MidiProtocolManager));


    m_TransportAbstractionId = AbstractionLayerGUID;    // this is needed so MidiSrv can instantiate the correct transport
    m_ContainerId = m_TransportAbstractionId;           // we use the transport ID as the container ID for convenience

    RETURN_IF_FAILED(CreateParentDevice());

    return S_OK;
}



_Use_decl_annotations_
HRESULT
CMidi2LoopbackMidiEndpointManager::DeleteEndpointPair(std::shared_ptr<MidiLoopbackDeviceDefinition> definition)
{

    return S_OK;
}



HRESULT
CMidi2LoopbackMidiEndpointManager::CreateParentDevice()
{
    TraceLoggingWrite(
        MidiLoopbackMidiAbstractionTelemetryProvider::Provider(),
        __FUNCTION__,
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
        MidiLoopbackMidiAbstractionTelemetryProvider::Provider(),
        __FUNCTION__,
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this"),
        TraceLoggingWideString(newDeviceId, "New parent device instance id")
    );

    return S_OK;
}



_Use_decl_annotations_
HRESULT 
CMidi2LoopbackMidiEndpointManager::CreateEndpointPair(
    std::shared_ptr<MidiLoopbackDeviceDefinition> definition
)
{
    TraceLoggingWrite(
        MidiLoopbackMidiAbstractionTelemetryProvider::Provider(),
        __FUNCTION__,
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this")
    );


    // workflow:
    // Configuration manager adds the entries to the endpoint table
    // Then calls this function to create the pair of endpoints
    // 
    // When endpoints BiDi are instantiated, they look at the endpoint
    // table to see what they are supposed to connect to.
    // 
    // They can be created in any order, so that will need to be
    // checked for each created endpoint.
    //
    // Perhaps the table itself should just contain IMidiBiDi-like
    // functions that are called by the endpoints? Then the endpoints
    // don't need to know anything other than where to send the message.
    // That would also be a pattern we can use for other types of
    // routing.
    //
    // So perhaps a set of definitions associated by association id and
    // then a related table of routing which has the required functions
    // and pointers. Or maybe it all stays in the same definition class?
    // and just rename it to MidiLoopbackEndpointDevice ?










    return S_OK;
}



HRESULT
CMidi2LoopbackMidiEndpointManager::Cleanup()
{
    TraceLoggingWrite(
        MidiLoopbackMidiAbstractionTelemetryProvider::Provider(),
        __FUNCTION__,
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this")
    );


    // destroy and release all the devices we have created

//    LOG_IF_FAILED(AbstractionState::Current().GetEndpointTable()->Cleanup());

    return S_OK;
}

