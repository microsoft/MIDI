// Copyright (c) Microsoft Corporation. All rights reserved.

#include "pch.h"
#include "midi2.sampleabstraction.h"

_Use_decl_annotations_
HRESULT
CMidi2SampleMidiBiDi::Initialize(
    LPCWSTR,
    DWORD *,
    IMidiCallback *
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
CMidi2SampleMidiBiDi::Cleanup()
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
CMidi2SampleMidiBiDi::SendMidiMessage(
    PVOID,
    UINT,
    LONGLONG
)
{
    return E_NOTIMPL;
}

_Use_decl_annotations_
HRESULT
CMidi2SampleMidiBiDi::Callback(
    PVOID,
    UINT,
    LONGLONG
)
{
    return E_NOTIMPL;
}

