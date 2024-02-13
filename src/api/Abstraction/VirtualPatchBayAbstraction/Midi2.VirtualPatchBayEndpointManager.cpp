// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://github.com/microsoft/MIDI/
// ============================================================================


#include "pch.h"
#include "midi2.VirtualPatchBayAbstraction.h"

using namespace wil;
using namespace Microsoft::WRL;
using namespace Microsoft::WRL::Wrappers;

#define MAX_DEVICE_ID_LEN 200 // size in chars

GUID AbstractionLayerGUID = __uuidof(Midi2VirtualPatchBayAbstraction);

_Use_decl_annotations_
HRESULT
CMidi2VirtualPatchBayEndpointManager::Initialize(
    IUnknown* MidiDeviceManager, 
    IUnknown* /*midiEndpointProtocolManager*/
)
{
 //   OutputDebugString(L"" __FUNCTION__ " Enter");

    TraceLoggingWrite(
        MidiVirtualPatchBayAbstractionTelemetryProvider::Provider(),
        __FUNCTION__,
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this")
    );

    RETURN_HR_IF(E_INVALIDARG, nullptr == MidiDeviceManager);

    RETURN_IF_FAILED(MidiDeviceManager->QueryInterface(__uuidof(IMidiDeviceManagerInterface), (void**)&m_MidiDeviceManager));

    m_TransportAbstractionId = AbstractionLayerGUID;   // this is needed so MidiSrv can instantiate the correct transport
    m_ContainerId = m_TransportAbstractionId;                           // we use the transport ID as the container ID for convenience

//    RETURN_IF_FAILED(CreateParentDevice());



    return S_OK;
}




_Use_decl_annotations_
HRESULT 
CMidi2VirtualPatchBayEndpointManager::CreateEndpoint(
    )
{



    return S_OK;
}


HRESULT
CMidi2VirtualPatchBayEndpointManager::Cleanup()
{
    OutputDebugString(L"" __FUNCTION__ " Enter");

    TraceLoggingWrite(
        MidiVirtualPatchBayAbstractionTelemetryProvider::Provider(),
        __FUNCTION__,
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this")
    );


    return S_OK;
}

