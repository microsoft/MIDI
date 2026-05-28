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
CMidi2BasicLoopbackMidiBidi::Initialize(
    LPCWSTR endpointId,
    PTRANSPORTCREATIONPARAMS,
    DWORD *,
    IMidiCallback * Callback,
    LONGLONG Context,
    GUID /* SessionId */
)
{
    TraceLoggingWrite(
        MidiBasicLoopbackMidiTransportTelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_INFO,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this"),
        TraceLoggingWideString(endpointId, "endpoint id")
        );

    RETURN_HR_IF_NULL(E_INVALIDARG, Callback);
    RETURN_HR_IF_NULL(E_INVALIDARG, endpointId);

    m_callbackContext = Context;
    m_endpointId = internal::NormalizeEndpointInterfaceIdWStringCopy(endpointId);
  
    RETURN_HR_IF(E_INVALIDARG, m_endpointId.empty());

    // TODO: This should use SWD properties and not a string search

    if (internal::EndpointInterfaceIdContainsString(m_endpointId, MIDI_BASIC_LOOP_INSTANCE_ID_PREFIX))
    {
        TraceLoggingWrite(
            MidiBasicLoopbackMidiTransportTelemetryProvider::Provider(),
            MIDI_TRACE_EVENT_INFO,
            TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
            TraceLoggingLevel(WINEVENT_LEVEL_INFO),
            TraceLoggingPointer(this, "this"),
            TraceLoggingWideString(L"Initializing Bidi", MIDI_TRACE_EVENT_MESSAGE_FIELD),
            TraceLoggingWideString(m_endpointId.c_str(), "endpoint id")
        );

        m_device = TransportState::Current().GetEndpointTable()->GetDeviceById(endpointId);
        RETURN_HR_IF_NULL(E_INVALIDARG, m_device);

        RETURN_IF_FAILED(m_device->Initialize(Callback));
    }
    else
    {
        // we don't understand this endpoint id

        TraceLoggingWrite(
            MidiBasicLoopbackMidiTransportTelemetryProvider::Provider(),
            MIDI_TRACE_EVENT_ERROR,
            TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
            TraceLoggingLevel(WINEVENT_LEVEL_ERROR),
            TraceLoggingPointer(this, "this"),
            TraceLoggingWideString(L"We don't understand the endpoint Id", MIDI_TRACE_EVENT_MESSAGE_FIELD),
            TraceLoggingWideString(m_endpointId.c_str(), "endpoint id")
        );

        return E_INVALIDARG;
    }

    TraceLoggingWrite(
        MidiBasicLoopbackMidiTransportTelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_INFO,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this"),
        TraceLoggingWideString(L"Exit", MIDI_TRACE_EVENT_MESSAGE_FIELD),
        TraceLoggingWideString(m_endpointId.c_str(), "endpoint id")
    );

    return S_OK;
}

HRESULT
CMidi2BasicLoopbackMidiBidi::Shutdown()
{
    TraceLoggingWrite(
        MidiBasicLoopbackMidiTransportTelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_INFO,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this"),
        TraceLoggingWideString(m_endpointId.c_str(), "endpoint id")
        );

    if (m_device)
    {
        LOG_IF_FAILED(m_device->Shutdown());
        m_device.reset();
    }

    return S_OK;
}

#pragma push_macro("SendMessage")
#undef SendMessage
_Use_decl_annotations_
HRESULT
CMidi2BasicLoopbackMidiBidi::SendMidiMessage(
    MessageOptionFlags optionFlags,
    PVOID Message,
    UINT Size,
    LONGLONG Position
)
{

    //TraceLoggingWrite(
    //    MidiLoopbackMidiTransportTelemetryProvider::Provider(),
    //    MIDI_TRACE_EVENT_INFO,
    //    TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
    //    TraceLoggingLevel(WINEVENT_LEVEL_INFO),
    //    TraceLoggingPointer(this, "this"),
    //    TraceLoggingWideString(m_endpointId.c_str(), "endpoint id"),
    //    TraceLoggingBool(m_isEndpointA, "is endpoint A")
    //);

    RETURN_HR_IF_NULL(E_INVALIDARG, Message);
    RETURN_HR_IF(E_INVALIDARG, Size < sizeof(uint32_t));
    RETURN_HR_IF_NULL(E_UNEXPECTED, m_device);

    RETURN_IF_FAILED(m_device->SendMessage(optionFlags, Message, Size, Position, m_callbackContext));

    return S_OK;
}
#pragma pop_macro("SendMessage")

_Use_decl_annotations_
HRESULT
CMidi2BasicLoopbackMidiBidi::Callback(
    MessageOptionFlags /*optionFlags*/ ,
    PVOID /*Message*/ ,
    UINT /*Size*/ ,
    LONGLONG /*Position*/ ,
    LONGLONG /*Context*/
)
{

    // we are a bidi to be compatible with the API, but because this is a MIDI 1.0-style loopback device, 
    // there's no need to implement the callback. Instead, to save one indirection,
    // we directly wire up to the provided callback in Initialize

    return S_OK;
}

