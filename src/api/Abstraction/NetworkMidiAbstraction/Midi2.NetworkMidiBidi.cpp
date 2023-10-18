// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://github.com/microsoft/MIDI/
// ============================================================================


#include "pch.h"
#include "midi2.NetworkMidiabstraction.h"

_Use_decl_annotations_
HRESULT
CMidi2NetworkMidiBiDi::Initialize(
    LPCWSTR,
    DWORD *,
    IMidiCallback * callback
)
{

    TraceLoggingWrite(
        MidiNetworkMidiAbstractionTelemetryProvider::Provider(),
        __FUNCTION__,
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this")
        );

    m_callback = callback;

    return S_OK;
}

HRESULT
CMidi2NetworkMidiBiDi::Cleanup()
{
    TraceLoggingWrite(
        MidiNetworkMidiAbstractionTelemetryProvider::Provider(),
        __FUNCTION__,
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this")
        );

    m_callback = nullptr;

    return S_OK;
}

_Use_decl_annotations_
HRESULT
CMidi2NetworkMidiBiDi::SendMidiMessage(
    PVOID message,
    UINT size,
    LONGLONG position
)
{
    if (m_callback == nullptr)
    {
        // TODO log that callback is null
        return E_FAIL;
    }

    if (message == nullptr)
    {
        // TODO log that message was null
        return E_FAIL;
    }

    if (size < sizeof(uint32_t))
    {
        // TODO log that data was smaller than minimum UMP size
        return E_FAIL;
    }




    m_callback->Callback(message, size, position);

    return S_OK;

}

_Use_decl_annotations_
HRESULT
CMidi2NetworkMidiBiDi::Callback(
    PVOID,
    UINT,
    LONGLONG
)
{
    //return E_NOTIMPL;

    // just eat it for this simple loopback
    return S_OK;
}

