// Copyright (c) Microsoft Corporation. All rights reserved.

#include "pch.h"
#include "midi2.sampletransform.h"

_Use_decl_annotations_
HRESULT
CMidi2SampleMidiTransform::Initialize(
    LPCWSTR,
    PTRANSFORMCREATIONPARAMS,
    DWORD *,
    IMidiCallback *,
    LONGLONG
)
{

    TraceLoggingWrite(
        MidiSampleTransformTelemetryProvider::Provider(),
        __FUNCTION__,
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this")
        );

    return E_NOTIMPL;
}

HRESULT
CMidi2SampleMidiTransform::Cleanup()
{
    TraceLoggingWrite(
        MidiSampleTransformTelemetryProvider::Provider(),
        __FUNCTION__,
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this")
        );

    return E_NOTIMPL;
}

_Use_decl_annotations_
HRESULT
CMidi2SampleMidiTransform::SendMidiMessage(
    PVOID,
    UINT,
    LONGLONG
)
{
    return E_NOTIMPL;
}

