// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://github.com/microsoft/MIDI/
// ============================================================================


#include "pch.h"
#include "midi2.NetworkMidiTransport.h"

_Use_decl_annotations_
HRESULT
CMidi2NetworkMidiBiDi::Initialize(
    LPCWSTR,
    PTRANSPORTCREATIONPARAMS,
    DWORD *,
    IMidiCallback * callback,
    LONGLONG context,
    GUID /* SessionId */
)
{
    TraceLoggingWrite(
        MidiNetworkMidiTransportTelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_INFO,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this"),
        TraceLoggingWideString(L"Enter", MIDI_TRACE_EVENT_MESSAGE_FIELD)
    );

    m_callback = callback;
    m_context = context;

    return S_OK;
}

HRESULT
CMidi2NetworkMidiBiDi::Shutdown()
{
    TraceLoggingWrite(
        MidiNetworkMidiTransportTelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_INFO,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this"),
        TraceLoggingWideString(L"Enter", MIDI_TRACE_EVENT_MESSAGE_FIELD)
    );

    m_callback = nullptr;
    m_context = 0;

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

    m_callback->Callback(message, size, position, m_context);

    return S_OK;

}

_Use_decl_annotations_
HRESULT
CMidi2NetworkMidiBiDi::Callback(
    PVOID,
    UINT,
    LONGLONG,
    LONGLONG
)
{
    //return E_NOTIMPL;

    // just eat it for this simple loopback
    return S_OK;
}

