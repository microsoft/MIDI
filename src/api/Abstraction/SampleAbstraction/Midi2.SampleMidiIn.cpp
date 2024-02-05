// Copyright (c) Microsoft Corporation. All rights reserved.

#include "pch.h"
#include "midi2.sampleabstraction.h"

_Use_decl_annotations_
HRESULT
CMidi2SampleMidiIn::Initialize(
    LPCWSTR,
    PABSTRACTIONCREATIONPARAMS,
    DWORD *,
    IMidiCallback *,
    LONGLONG,
    GUID /* SessionId */

)
{
    TraceLoggingWrite(
        MidiSampleAbstractionTelemetryProvider::Provider(),
        __FUNCTION__,
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this")
        );

    return E_NOTIMPL;
}

HRESULT
CMidi2SampleMidiIn::Cleanup()
{
    TraceLoggingWrite(
        MidiSampleAbstractionTelemetryProvider::Provider(),
        __FUNCTION__,
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this")
        );

    return E_NOTIMPL;
}

_Use_decl_annotations_
HRESULT
CMidi2SampleMidiIn::Callback(
    PVOID,
    UINT,
    LONGLONG,
    LONGLONG
)
{
    return E_NOTIMPL;
}

