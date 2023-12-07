// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://github.com/microsoft/MIDI/
// ============================================================================


#include "pch.h"
#include "midi2.VirtualMidiabstraction.h"

_Use_decl_annotations_
HRESULT
CMidi2VirtualMidiDeviceBiDi::Initialize(
    LPCWSTR,
    PABSTRACTIONCREATIONPARAMS,
    DWORD *,
    IMidiCallback * Callback,
    LONGLONG Context
)
{

    TraceLoggingWrite(
        MidiVirtualMidiAbstractionTelemetryProvider::Provider(),
        __FUNCTION__,
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this")
        );

    m_ClientCallback = Callback;
    m_ClientCallbackContext = Context;


    // TODO: Spin up the client endpoint


    return S_OK;
}

HRESULT
CMidi2VirtualMidiDeviceBiDi::Cleanup()
{
    // TODO: Cleanup here needs additional logic to tear down the client endpoint
    // when this endpoint goes away

    TraceLoggingWrite(
        MidiVirtualMidiAbstractionTelemetryProvider::Provider(),
        __FUNCTION__,
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this")
        );

    m_ClientCallback = nullptr;
    m_ClientCallbackContext = 0;
    m_DeviceBiDi = nullptr;

    return S_OK;
}

_Use_decl_annotations_
HRESULT
CMidi2VirtualMidiDeviceBiDi::SendMidiMessage(
    PVOID /*Message*/,
    UINT /*Size*/,
    LONGLONG /*Position*/
)
{
    //// message received from the client

    //RETURN_HR_IF_NULL(E_INVALIDARG, Message);

    //if (Size < sizeof(uint32_t))
    //{
    //    // TODO log that data was smaller than minimum UMP size
    //    return E_FAIL;
    //}

    //if (m_ClientCallback != nullptr)
    //{
    //    return m_ClientCallback(Message, Size, Position, m_ClientCallbackContext);
    //}

    return S_OK;

}

_Use_decl_annotations_
HRESULT
CMidi2VirtualMidiDeviceBiDi::Callback(
    PVOID /*Message*/,
    UINT /*Size*/,
    LONGLONG /*Position*/,
    LONGLONG /*Context*/
)
{
    //if (m_ClientCallback != nullptr)
    //{
    //    return m_ClientCallback->Callback(Message, Size, Position, m_ClientCallbackContext);
    //}

    return S_OK;
}

