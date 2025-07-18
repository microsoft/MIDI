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
CMidi2KSAggregateMidiBidi::Initialize(
    LPCWSTR device,
    PTRANSPORTCREATIONPARAMS creationParams,
    DWORD * mmCssTaskId,
    IMidiCallback * callback,
    LONGLONG context,
    GUID /* sessionId */
)
{
    RETURN_HR_IF(E_INVALIDARG, nullptr == callback);
    RETURN_HR_IF(E_INVALIDARG, nullptr == mmCssTaskId);
    RETURN_HR_IF(E_INVALIDARG, nullptr == device);
    RETURN_HR_IF(E_INVALIDARG, nullptr == creationParams);

    TraceLoggingWrite(
        MidiKSAggregateTransportTelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_INFO,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this"),
        TraceLoggingWideString(device, MIDI_TRACE_EVENT_DEVICE_SWD_ID_FIELD)
        );

    m_endpointDeviceId = internal::NormalizeEndpointInterfaceIdWStringCopy(device);

    m_countMidiMessageSent = 0;

    //std::unique_ptr<CMidi2KSAggregateMidi> midiDevice(new (std::nothrow) CMidi2KSAggregateMidi());
    auto midiDevice = std::make_unique<CMidi2KSAggregateMidi>();
    RETURN_IF_NULL_ALLOC(midiDevice);

    RETURN_IF_FAILED(
        midiDevice->Initialize(
            device, 
            MidiFlowBidirectional, 
            creationParams, 
            mmCssTaskId, 
            callback, 
            context
        )
    );

    m_midiDevice = std::move(midiDevice);

    TraceLoggingWrite(
        MidiKSAggregateTransportTelemetryProvider::Provider(),
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
CMidi2KSAggregateMidiBidi::Shutdown()
{
    TraceLoggingWrite(
        MidiKSAggregateTransportTelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_METRICS,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this"),
        TraceLoggingWideString(L"Cleaning up", MIDI_TRACE_EVENT_MESSAGE_FIELD),
        TraceLoggingWideString(m_endpointDeviceId.c_str(), MIDI_TRACE_EVENT_DEVICE_SWD_ID_FIELD),
        TraceLoggingUInt64(m_countMidiMessageSent, "Count messages sent")
        );

    if (m_midiDevice)
    {
        m_midiDevice->Shutdown();
        m_midiDevice = nullptr;
    }

    return S_OK;
}

_Use_decl_annotations_
HRESULT
CMidi2KSAggregateMidiBidi::SendMidiMessage(
    MessageOptionFlags optionFlags,
    PVOID data,
    UINT length,
    LONGLONG timestamp
)
{
    if (m_midiDevice != nullptr)
    {
        TraceLoggingWrite(
            MidiKSAggregateTransportTelemetryProvider::Provider(),
            MIDI_TRACE_EVENT_VERBOSE,
            TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
            TraceLoggingLevel(WINEVENT_LEVEL_VERBOSE),
            TraceLoggingPointer(this, "this"),
            TraceLoggingWideString(L"Received MIDI Message. Sending to device.", MIDI_TRACE_EVENT_MESSAGE_FIELD),
            TraceLoggingHexUInt32Array(static_cast<uint32_t*>(data), static_cast<uint16_t>(length/sizeof(uint32_t)), "data"),
            TraceLoggingUInt32(static_cast<uint32_t>(length), "length bytes"),
            TraceLoggingUInt64(static_cast<uint64_t>(timestamp), MIDI_TRACE_EVENT_MESSAGE_TIMESTAMP_FIELD)
        );

        RETURN_IF_FAILED(m_midiDevice->SendMidiMessage(optionFlags, data, length, timestamp));

        m_countMidiMessageSent++;

        //TraceLoggingWrite(
        //    MidiKSAggregateTransportTelemetryProvider::Provider(),
        //    MIDI_TRACE_EVENT_METRICS,
        //    TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        //    TraceLoggingLevel(WINEVENT_LEVEL_VERBOSE),
        //    TraceLoggingPointer(this, "this"),
        //    TraceLoggingWideString(L"Messages sent so far.", MIDI_TRACE_EVENT_MESSAGE_FIELD),
        //    TraceLoggingUInt64(m_countMidiMessageSent, "count")
        //);

        return S_OK;
    }
    else
    {
        TraceLoggingWrite(
            MidiKSAggregateTransportTelemetryProvider::Provider(),
            MIDI_TRACE_EVENT_ERROR,
            TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
            TraceLoggingLevel(WINEVENT_LEVEL_ERROR),
            TraceLoggingPointer(this, "this"),
            TraceLoggingWideString(L"MidiDevice is nullptr. Returning E_FAIL", MIDI_TRACE_EVENT_MESSAGE_FIELD),
            TraceLoggingWideString(m_endpointDeviceId.c_str(), MIDI_TRACE_EVENT_DEVICE_SWD_ID_FIELD),
            TraceLoggingUInt32(length, "data length"),
            TraceLoggingUInt64(timestamp, MIDI_TRACE_EVENT_MESSAGE_TIMESTAMP_FIELD),
            TraceLoggingUInt64(m_countMidiMessageSent, "Count messages sent so far")
        );

        return E_FAIL;
    }

}
