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

    m_ClientCallback = Callback;
    m_ClientCallbackContext = Context;

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

    m_ClientCallback = nullptr;
    m_ClientCallbackContext = 0;
    m_EndpointBiDi = nullptr;

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

    if (Size < sizeof(uint32_t))
    {
        // TODO log that data was smaller than minimum UMP size
        return E_FAIL;
    }

    if (m_EndpointBiDi != nullptr)
    {
        // is this right, or should it be the callback?
        return m_EndpointBiDi->SendMidiMessage(Message, Size, Position);
    }

    return S_OK;

}

_Use_decl_annotations_
HRESULT
CMidi2VirtualMidiClientBiDi::Callback(
    PVOID Message,
    UINT Size,
    LONGLONG Position,
    LONGLONG
)
{
    // message received from the device

    if (m_ClientCallback != nullptr)
    {
        return m_ClientCallback->Callback(Message, Size, Position, m_ClientCallbackContext);
    }

    return S_OK;
}

