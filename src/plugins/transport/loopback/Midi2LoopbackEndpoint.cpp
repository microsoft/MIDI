// Copyright (c) Microsoft Corporation. All rights reserved.

#include "pch.h"
#include "Midi2LoopbackEndpoint.h"

_Use_decl_annotations_
HRESULT
CMidi2LoopbackEndpoint::Initialize(
    LPCWSTR,
    DWORD*,
    IMidiCallback*
)
{

    TraceLoggingWrite(
        MidiLoopbackTransportAbstractionTelemetryProvider::Provider(),
        __FUNCTION__,
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this")
    );

    // TODO: Any initialization code

    return E_NOTIMPL;
}

HRESULT
CMidi2LoopbackEndpoint::Cleanup()
{
    TraceLoggingWrite(
        MidiLoopbackTransportAbstractionTelemetryProvider::Provider(),
        __FUNCTION__,
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this")
    );

    // TODO: Any cleanup code

    return E_NOTIMPL;
}

_Use_decl_annotations_
HRESULT
CMidi2LoopbackEndpoint::SendMidiMessage(
    PVOID,
    UINT,
    LONGLONG
)
{
    // TODO: Send the MIDI Message

    return E_NOTIMPL;
}

_Use_decl_annotations_
HRESULT
CMidi2LoopbackEndpoint::Callback(
    PVOID,
    UINT,
    LONGLONG
)
{
    // TODO: Callback for the received MIDI message

    return E_NOTIMPL;
}

