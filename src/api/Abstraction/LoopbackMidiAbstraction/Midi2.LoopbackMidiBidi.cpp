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
CMidi2LoopbackMidiBiDi::Initialize(
    LPCWSTR endpointId,
    PABSTRACTIONCREATIONPARAMS,
    DWORD *,
    IMidiCallback * Callback,
    LONGLONG Context,
    GUID /* SessionId */
)
{
    TraceLoggingWrite(
        MidiLoopbackMidiAbstractionTelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_INFO,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this"),
        TraceLoggingWideString(endpointId, "endpoint id")
        );

    m_callbackContext = Context;
    m_endpointId = internal::NormalizeEndpointInterfaceIdWStringCopy(endpointId);
  
    m_associationId = internal::GetSwdPropertyVirtualEndpointAssociationId(m_endpointId);


    HRESULT hr = S_OK;

    // TODO: This should use SWD properties and not a string search

    if (internal::EndpointInterfaceIdContainsString(m_endpointId, MIDI_PERM_LOOP_INSTANCE_ID_A_PREFIX) ||
        internal::EndpointInterfaceIdContainsString(m_endpointId, MIDI_TEMP_LOOP_INSTANCE_ID_A_PREFIX))
    {
        TraceLoggingWrite(
            MidiLoopbackMidiAbstractionTelemetryProvider::Provider(),
            MIDI_TRACE_EVENT_INFO,
            TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
            TraceLoggingLevel(WINEVENT_LEVEL_INFO),
            TraceLoggingPointer(this, "this"),
            TraceLoggingWideString(L"Initializing Side-A BiDi", MIDI_TRACE_EVENT_MESSAGE_FIELD),
            TraceLoggingWideString(m_endpointId.c_str(), "endpoint id"),
            TraceLoggingWideString(m_associationId.c_str(), "association id")
        );

        m_callback = Callback;

        m_isEndpointA = true;

    }
    else if (internal::EndpointInterfaceIdContainsString(m_endpointId, MIDI_PERM_LOOP_INSTANCE_ID_B_PREFIX) ||
             internal::EndpointInterfaceIdContainsString(m_endpointId, MIDI_TEMP_LOOP_INSTANCE_ID_B_PREFIX))
    {
        TraceLoggingWrite(
            MidiLoopbackMidiAbstractionTelemetryProvider::Provider(),
            MIDI_TRACE_EVENT_INFO,
            TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
            TraceLoggingLevel(WINEVENT_LEVEL_INFO),
            TraceLoggingPointer(this, "this"),
            TraceLoggingWideString(L"Initializing Side-B BiDi", MIDI_TRACE_EVENT_MESSAGE_FIELD),
            TraceLoggingWideString(m_endpointId.c_str(), "endpoint id"),
            TraceLoggingWideString(m_associationId.c_str(), "association id")
        );

        m_callback = Callback;

        m_isEndpointA = false;
    }
    else
    {
        // we don't understand this endpoint id

        TraceLoggingWrite(
            MidiLoopbackMidiAbstractionTelemetryProvider::Provider(),
            MIDI_TRACE_EVENT_ERROR,
            TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
            TraceLoggingLevel(WINEVENT_LEVEL_ERROR),
            TraceLoggingPointer(this, "this"),
            TraceLoggingWideString(L"We don't understand the endpoint Id", MIDI_TRACE_EVENT_MESSAGE_FIELD),
            TraceLoggingWideString(m_endpointId.c_str(), "endpoint id"),
            TraceLoggingWideString(m_associationId.c_str(), "association id")
        );

        return E_FAIL;
    }

    // register this endpoint as part of a loopback device

    auto device = AbstractionState::Current().GetEndpointTable()->GetDevice(m_associationId);

    if (device)
    {
        if (m_isEndpointA)
        {
            device->RegisterEndpointA(this);
        }
        else
        {
            device->RegisterEndpointB(this);
        }
    }
    else
    {
        TraceLoggingWrite(
            MidiLoopbackMidiAbstractionTelemetryProvider::Provider(),
            MIDI_TRACE_EVENT_INFO,
            TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
            TraceLoggingLevel(WINEVENT_LEVEL_INFO),
            TraceLoggingPointer(this, "this"),
            TraceLoggingWideString(L"Unable to find matching device in device table", MIDI_TRACE_EVENT_MESSAGE_FIELD),
            TraceLoggingWideString(m_endpointId.c_str(), "endpoint id"),
            TraceLoggingWideString(m_associationId.c_str(), "association id")
        );
    }

    return hr;
}

HRESULT
CMidi2LoopbackMidiBiDi::Cleanup()
{
    TraceLoggingWrite(
        MidiLoopbackMidiAbstractionTelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_INFO,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this"),
        TraceLoggingWideString(m_endpointId.c_str(), "endpoint id")
        );

    m_callback = nullptr;

    return S_OK;
}

_Use_decl_annotations_
HRESULT
CMidi2LoopbackMidiBiDi::SendMidiMessage(
    PVOID Message,
    UINT Size,
    LONGLONG Position
)
{
    //TraceLoggingWrite(
    //    MidiLoopbackMidiAbstractionTelemetryProvider::Provider(),
    //    MIDI_TRACE_EVENT_INFO,
    //    TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
    //    TraceLoggingLevel(WINEVENT_LEVEL_INFO),
    //    TraceLoggingPointer(this, "this"),
    //    TraceLoggingWideString(m_endpointId.c_str(), "endpoint id"),
    //    TraceLoggingBool(m_isEndpointA, "is endpoint A")
    //);

    RETURN_HR_IF_NULL(E_INVALIDARG, Message);
    RETURN_HR_IF(E_INVALIDARG, Size < sizeof(uint32_t));

    if (m_isEndpointA)
    {
        // send endpoint A to B
        auto device = AbstractionState::Current().GetEndpointTable()->GetDevice(m_associationId);

        if (device)
        {
            return device->SendMessageAToB(Message, Size, Position, m_callbackContext);
        }
        else
        {
            TraceLoggingWrite(
                MidiLoopbackMidiAbstractionTelemetryProvider::Provider(),
                MIDI_TRACE_EVENT_ERROR,
                TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
                TraceLoggingLevel(WINEVENT_LEVEL_ERROR),
                TraceLoggingPointer(this, "this"),
                TraceLoggingWideString(m_endpointId.c_str(), "endpoint id"),
                TraceLoggingWideString(L"Send A to B : Unable to find endpoint device in table", MIDI_TRACE_EVENT_MESSAGE_FIELD)
                );

            return E_FAIL;
        }
    }
    else
    {
        // send endpoint B to A
        auto device = AbstractionState::Current().GetEndpointTable()->GetDevice(m_associationId);

        if (device)
        {
            return device->SendMessageBToA(Message, Size, Position, m_callbackContext);
        }
        else
        {
            TraceLoggingWrite(
                MidiLoopbackMidiAbstractionTelemetryProvider::Provider(),
                MIDI_TRACE_EVENT_ERROR,
                TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
                TraceLoggingLevel(WINEVENT_LEVEL_ERROR),
                TraceLoggingPointer(this, "this"),
                TraceLoggingWideString(m_endpointId.c_str(), "endpoint id"),
                TraceLoggingWideString(L"Send B to A : Unable to find endpoint device in table", MIDI_TRACE_EVENT_MESSAGE_FIELD)
            );

            return E_FAIL;
        }
    }
}

_Use_decl_annotations_
HRESULT
CMidi2LoopbackMidiBiDi::Callback(
    PVOID Message,
    UINT Size,
    LONGLONG Position,
    LONGLONG Context
)
{
    //TraceLoggingWrite(
    //    MidiLoopbackMidiAbstractionTelemetryProvider::Provider(),
    //    MIDI_TRACE_EVENT_INFO,
    //    TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
    //    TraceLoggingLevel(WINEVENT_LEVEL_INFO),
    //    TraceLoggingPointer(this, "this"),
    //    TraceLoggingWideString(m_endpointId.c_str(), "endpoint id")
    //);

    // message received from the client

    if (m_callback != nullptr)
    {
        return m_callback->Callback(Message, Size, Position, Context);
    }

    return S_OK;
}

