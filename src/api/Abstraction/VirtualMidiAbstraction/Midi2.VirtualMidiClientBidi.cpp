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
CMidi2VirtualMidiClientBiDi::Initialize(
    LPCWSTR,
    PABSTRACTIONCREATIONPARAMS,
    DWORD*,
    IMidiCallback* Callback,
    LONGLONG Context
)
{

    TraceLoggingWrite(
        MidiVirtualMidiAbstractionTelemetryProvider::Provider(),
        __FUNCTION__,
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this")
    );

    m_clientCallback = Callback;
    m_clientCallbackContext = Context;

    // TODO: Using the device Id parameter, associate with the endpoint in the endpoint device table

    return S_OK;
}

HRESULT
CMidi2VirtualMidiClientBiDi::Cleanup()
{
    TraceLoggingWrite(
        MidiVirtualMidiAbstractionTelemetryProvider::Provider(),
        __FUNCTION__,
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this")
    );

    m_clientCallback = nullptr;
    m_clientCallbackContext = 0;

    m_linkedDeviceBiDi->Release();
    m_linkedDeviceBiDi = nullptr;

    return S_OK;
}

_Use_decl_annotations_
HRESULT
CMidi2VirtualMidiClientBiDi::SendMidiMessage(
    PVOID Message,
    UINT Size,
    LONGLONG Position
)
{
    // message received from the client

    RETURN_HR_IF_NULL(E_INVALIDARG, Message);
    RETURN_HR_IF(E_INVALIDARG, Size < sizeof(uint32_t));

    // if there's no linked bidi, it's not a failure. We just lose the message
    if (m_linkedDeviceBiDi != nullptr)
    {
        // is this right, or should it be the callback?
        return m_linkedDeviceBiDi->SendMidiMessage(Message, Size, Position);
    }

    return S_OK;

}

_Use_decl_annotations_
HRESULT
CMidi2VirtualMidiClientBiDi::Callback(
    PVOID Message,
    UINT Size,
    LONGLONG Position,
    LONGLONG Context
)
{
    // message received from the device

    if (m_clientCallback != nullptr)
    {
        return m_clientCallback->Callback(Message, Size, Position, Context);
    }

    return S_OK;
}

