// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================


#include "pch.h"





_Use_decl_annotations_
HRESULT
CMidi2KSAggregateMidiBiDi::Initialize(
    LPCWSTR Device,
    PABSTRACTIONCREATIONPARAMS CreationParams,
    DWORD * MmCssTaskId,
    IMidiCallback * Callback,
    LONGLONG Context,
    GUID /* SessionId */
)
{
    RETURN_HR_IF(E_INVALIDARG, nullptr == Callback);
    RETURN_HR_IF(E_INVALIDARG, nullptr == Device);
    RETURN_HR_IF(E_INVALIDARG, nullptr == CreationParams);

    TraceLoggingWrite(
        MidiKSAggregateAbstractionTelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_INFO,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this"),
        TraceLoggingWideString(Device, MIDI_TRACE_EVENT_DEVICE_SWD_ID_FIELD)
        );

    m_endpointDeviceId = internal::NormalizeEndpointInterfaceIdWStringCopy(Device);

    m_countMidiMessageSent = 0;

    //std::unique_ptr<CMidi2KSAggregateMidi> midiDevice(new (std::nothrow) CMidi2KSAggregateMidi());
    auto midiDevice = std::make_unique<CMidi2KSAggregateMidi>();
    RETURN_IF_NULL_ALLOC(midiDevice);

    RETURN_IF_FAILED(
        midiDevice->Initialize(
            Device, 
            MidiFlowBidirectional, 
            CreationParams, 
            MmCssTaskId, 
            Callback, 
            Context
        )
    );

    m_MidiDevice = std::move(midiDevice);

    TraceLoggingWrite(
        MidiKSAggregateAbstractionTelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_INFO,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this"),
        TraceLoggingWideString(L"Initialization complete", MIDI_TRACE_EVENT_MESSAGE_FIELD),
        TraceLoggingWideString(m_endpointDeviceId.c_str(), MIDI_TRACE_EVENT_DEVICE_SWD_ID_FIELD)
        );

    return S_OK;
}

HRESULT
CMidi2KSAggregateMidiBiDi::Cleanup()
{
    TraceLoggingWrite(
        MidiKSAggregateAbstractionTelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_METRICS,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this"),
        TraceLoggingWideString(L"Cleaning up", MIDI_TRACE_EVENT_MESSAGE_FIELD),
        TraceLoggingWideString(m_endpointDeviceId.c_str(), MIDI_TRACE_EVENT_DEVICE_SWD_ID_FIELD),
        TraceLoggingUInt64(m_countMidiMessageSent, "Count messages sent")
        );

    if (m_MidiDevice)
    {
        m_MidiDevice->Cleanup();
        m_MidiDevice = nullptr;
    }

    return S_OK;
}

_Use_decl_annotations_
HRESULT
CMidi2KSAggregateMidiBiDi::SendMidiMessage(
    PVOID Data,
    UINT Length,
    LONGLONG Timestamp
)
{
    if (m_MidiDevice != nullptr)
    {
        TraceLoggingWrite(
            MidiKSAggregateAbstractionTelemetryProvider::Provider(),
            MIDI_TRACE_EVENT_VERBOSE,
            TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
            TraceLoggingLevel(WINEVENT_LEVEL_VERBOSE),
            TraceLoggingPointer(this, "this"),
            TraceLoggingWideString(L"Received MIDI Message. Sending to device.", MIDI_TRACE_EVENT_MESSAGE_FIELD),
            TraceLoggingUInt32(Length, "data length"),
            TraceLoggingUInt64(Timestamp, MIDI_TRACE_EVENT_MESSAGE_TIMESTAMP_FIELD)
        );

        RETURN_IF_FAILED(m_MidiDevice->SendMidiMessage(Data, Length, Timestamp));

        m_countMidiMessageSent++;

        TraceLoggingWrite(
            MidiKSAggregateAbstractionTelemetryProvider::Provider(),
            MIDI_TRACE_EVENT_METRICS,
            TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
            TraceLoggingLevel(WINEVENT_LEVEL_VERBOSE),
            TraceLoggingPointer(this, "this"),
            TraceLoggingWideString(L"Messages sent so far.", MIDI_TRACE_EVENT_MESSAGE_FIELD),
            TraceLoggingUInt64(m_countMidiMessageSent, "count")
        );

        return S_OK;
    }
    else
    {
        TraceLoggingWrite(
            MidiKSAggregateAbstractionTelemetryProvider::Provider(),
            MIDI_TRACE_EVENT_ERROR,
            TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
            TraceLoggingLevel(WINEVENT_LEVEL_ERROR),
            TraceLoggingPointer(this, "this"),
            TraceLoggingWideString(L"MidiDevice is nullptr. Returning E_FAIL", MIDI_TRACE_EVENT_MESSAGE_FIELD),
            TraceLoggingWideString(m_endpointDeviceId.c_str(), MIDI_TRACE_EVENT_DEVICE_SWD_ID_FIELD),
            TraceLoggingUInt32(Length, "data length"),
            TraceLoggingUInt64(Timestamp, MIDI_TRACE_EVENT_MESSAGE_TIMESTAMP_FIELD),
            TraceLoggingUInt64(m_countMidiMessageSent, "Count messages sent so far")
        );

        return E_FAIL;
    }

}

