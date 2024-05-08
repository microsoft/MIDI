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
    LONGLONG,
    IUnknown* /*MidiDeviceManager*/
)
{

    //TraceLoggingWrite(
    //    MidiSampleTransformTelemetryProvider::Provider(),
    //    MIDI_TRACE_EVENT_INFO,
    //    TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
    //    TraceLoggingLevel(WINEVENT_LEVEL_INFO),
    //    TraceLoggingPointer(this, "this")
    //    );

    return E_NOTIMPL;
}

HRESULT
CMidi2SampleMidiTransform::Cleanup()
{
    //TraceLoggingWrite(
    //    MidiSampleTransformTelemetryProvider::Provider(),
    //    MIDI_TRACE_EVENT_INFO,
    //    TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
    //    TraceLoggingLevel(WINEVENT_LEVEL_INFO),
    //    TraceLoggingPointer(this, "this")
    //    );

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

