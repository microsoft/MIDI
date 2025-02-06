// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

#include "stdafx.h"

_Use_decl_annotations_
HRESULT
CMidiTransformPipe::Initialize(
    LPCWSTR device,
    PMIDISRV_TRANSFORMCREATION_PARAMS pipeCreationParams,
    DWORD* mmcssTaskId,
    IMidiDeviceManager* midiDeviceManager
)
{
    TraceLoggingWrite(
        MidiSrvTelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_INFO,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this"),
        TraceLoggingWideString(L"Enter", MIDI_TRACE_EVENT_MESSAGE_FIELD),
        TraceLoggingWideString(device, MIDI_TRACE_EVENT_DEVICE_SWD_ID_FIELD)
    );


    wil::com_ptr_nothrow<IMidiTransform> midiTransform;
    TRANSFORMCREATIONPARAMS creationParams {};

    m_TransformGuid = pipeCreationParams->TransformGuid;

    // Confirm that this component is either signed, or we are in developer mode.
    // Else, do not use it.
    RETURN_IF_FAILED(internal::IsComponentPermitted(m_TransformGuid));

    // Transforms are "bidirectional" from the midi pipes perspective,
    // so we always initialize the pipe as bidirectional.
    RETURN_IF_FAILED(CMidiPipe::Initialize(device, MidiFlowBidirectional));

    RETURN_IF_FAILED(SetDataFormatIn(pipeCreationParams->DataFormatIn));
    RETURN_IF_FAILED(SetDataFormatOut(pipeCreationParams->DataFormatOut));

    RETURN_IF_FAILED(CoCreateInstance(m_TransformGuid, nullptr, CLSCTX_ALL, IID_PPV_ARGS(&midiTransform)));
    RETURN_IF_FAILED(midiTransform->Activate(__uuidof(IMidiDataTransform), (void**)&m_MidiDataTransform));

    creationParams.DataFormatIn = DataFormatIn();
    creationParams.DataFormatOut = DataFormatOut();
    creationParams.UmpGroupIndex = pipeCreationParams->UmpGroupIndex;

    // transforms are initialized with the group index as the context.
    RETURN_IF_FAILED(m_MidiDataTransform->Initialize(device, &creationParams, mmcssTaskId, this, pipeCreationParams->UmpGroupIndex, midiDeviceManager));

    TraceLoggingWrite(
        MidiSrvTelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_INFO,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this"),
        TraceLoggingWideString(L"Exit success", MIDI_TRACE_EVENT_MESSAGE_FIELD),
        TraceLoggingWideString(device, MIDI_TRACE_EVENT_DEVICE_SWD_ID_FIELD)
    );

    return S_OK;
}

GUID CMidiTransformPipe::TransformGuid()
{
    return m_TransformGuid;
}

HRESULT
CMidiTransformPipe::Shutdown()
{
    TraceLoggingWrite(
        MidiSrvTelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_INFO,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this"),
        TraceLoggingWideString(L"Enter", MIDI_TRACE_EVENT_MESSAGE_FIELD)
    );

    if (m_MidiDataTransform)
    {
        RETURN_IF_FAILED(m_MidiDataTransform->Shutdown());
    }

    RETURN_IF_FAILED(CMidiPipe::Shutdown());

    TraceLoggingWrite(
        MidiSrvTelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_INFO,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this"),
        TraceLoggingWideString(L"Exit success", MIDI_TRACE_EVENT_MESSAGE_FIELD)
        );


    return S_OK;
}

_Use_decl_annotations_
HRESULT
CMidiTransformPipe::SendMidiMessage(
    PVOID data,
    UINT length,
    LONGLONG timestamp
)
{
    return m_MidiDataTransform->SendMidiMessage(data, length, timestamp);
}


_Use_decl_annotations_
HRESULT
CMidiTransformPipe::SendMidiMessageNow(
    PVOID data,
    UINT length,
    LONGLONG timestamp
)
{
    return m_MidiDataTransform->SendMidiMessage(data, length, timestamp);
}

