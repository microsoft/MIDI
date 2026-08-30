// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

#include "pch.h"

#include "Midi2.MessageTypeFilterTransformImplementation.h"

#include "ump_iterator.h"

_Use_decl_annotations_
HRESULT
CMidi2MessageTypeFilterTransformImplementation::Initialize(
    LPCWSTR endpointDeviceInterfaceId,
    PTRANSFORMCREATIONPARAMS creationParams,
    DWORD*,
    IMidiCallback* callback,
    LONGLONG context,
    IMidiDeviceManager* /*midiDeviceManager*/
)
{
    TraceLoggingWrite(
        Midi2MessageTypeFilterTransformTelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_INFO,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this"),
        TraceLoggingWideString(L"Enter", MIDI_TRACE_EVENT_MESSAGE_FIELD),
        TraceLoggingWideString(endpointDeviceInterfaceId, MIDI_TRACE_EVENT_DEVICE_SWD_ID_FIELD)
    );

    // this only converts UMP to UMP
    RETURN_HR_IF(HRESULT_FROM_WIN32(ERROR_UNSUPPORTED_TYPE), creationParams->DataFormatIn != MidiDataFormats_UMP);
    RETURN_HR_IF(HRESULT_FROM_WIN32(ERROR_UNSUPPORTED_TYPE), creationParams->DataFormatOut != MidiDataFormats_UMP);

    m_callback = callback;
    m_context = context;
    m_endpointDeviceInterfaceId = endpointDeviceInterfaceId;

    return S_OK;
}




HRESULT
CMidi2MessageTypeFilterTransformImplementation::Shutdown()
{
    TraceLoggingWrite(
        Midi2MessageTypeFilterTransformTelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_INFO,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this"),
        TraceLoggingWideString(L"Enter", MIDI_TRACE_EVENT_MESSAGE_FIELD)
    );

    return S_OK;
}


_Use_decl_annotations_
HRESULT
CMidi2MessageTypeFilterTransformImplementation::SendMidiMessage(
    MessageOptionFlags optionFlags,
    PVOID inputData,
    UINT length,
    LONGLONG timestamp
)
{
    //TraceLoggingWrite(
    //    MidiUmpProtocolDownscalerTransformTelemetryProvider::Provider(),
    //    MIDI_TRACE_EVENT_INFO,
    //    TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
    //    TraceLoggingLevel(WINEVENT_LEVEL_INFO),
    //    TraceLoggingPointer(this, "this")
    //);

    // only 1 message may be downscaled at a time
    auto lock = m_sendLock.lock();

    UNREFERENCED_PARAMETER(optionFlags);
    UNREFERENCED_PARAMETER(inputData);
    UNREFERENCED_PARAMETER(length);
    UNREFERENCED_PARAMETER(timestamp);

    return S_OK;

}

