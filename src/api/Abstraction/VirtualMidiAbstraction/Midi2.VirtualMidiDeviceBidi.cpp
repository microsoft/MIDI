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

    m_deviceCallback = Callback;
    m_deviceCallbackContext = Context;


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

    m_deviceCallback = nullptr;
    m_deviceCallbackContext = 0;
    
    //m_LinkedClientBiDi->Release();
    m_linkedClientBiDi = nullptr;

    return S_OK;
}

_Use_decl_annotations_
HRESULT
CMidi2VirtualMidiDeviceBiDi::SendMidiMessage(
    PVOID Message,
    UINT Size,
    LONGLONG Position
)
{
    // message received from the device

    RETURN_HR_IF_NULL(E_INVALIDARG, Message);
    RETURN_HR_IF(E_INVALIDARG, Size < sizeof(uint32_t));

    // if there's no linked bidi, it's not a failure. We just lose the message
    if (m_linkedClientBiDi != nullptr)
    {
        // is this right, or should it be the callback?
        return m_linkedClientBiDi->SendMidiMessage(Message, Size, Position);
    }

    return S_OK;

}

_Use_decl_annotations_
HRESULT
CMidi2VirtualMidiDeviceBiDi::Callback(
    PVOID Message,
    UINT Size,
    LONGLONG Position,
    LONGLONG Context
)
{
    // message received from the client

    if (m_deviceCallback != nullptr)
    {
        return m_deviceCallback->Callback(Message, Size, Position, Context);
    }

    return S_OK;
}

