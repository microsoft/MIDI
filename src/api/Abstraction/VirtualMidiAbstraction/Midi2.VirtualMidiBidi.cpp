// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================


#include "pch.h"
#include "midi2.VirtualMidiabstraction.h"

_Use_decl_annotations_
HRESULT
CMidi2VirtualMidiBiDi::Initialize(
    LPCWSTR endpointId,
    PABSTRACTIONCREATIONPARAMS,
    DWORD *,
    IMidiCallback * Callback,
    LONGLONG Context,
    GUID /* SessionId */
)
{
    TraceLoggingWrite(
        MidiVirtualMidiAbstractionTelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_INFO,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this"),
        TraceLoggingWideString(L"Enter", MIDI_TRACE_EVENT_MESSAGE_FIELD),
        TraceLoggingWideString(endpointId, MIDI_TRACE_EVENT_DEVICE_SWD_ID_FIELD)
        );

    m_callbackContext = Context;
    m_endpointId = internal::NormalizeEndpointInterfaceIdWStringCopy(endpointId);
  
    //if (Context != MIDI_PROTOCOL_MANAGER_ENDPOINT_CREATION_CONTEXT)
    {
        HRESULT hr = S_OK;

        // TODO: This should use SWD properties and not a string search

        if (internal::EndpointInterfaceIdContainsString(m_endpointId, MIDI_VIRT_INSTANCE_ID_DEVICE_PREFIX))
        {
            TraceLoggingWrite(
                MidiVirtualMidiAbstractionTelemetryProvider::Provider(),
                MIDI_TRACE_EVENT_VERBOSE,
                TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
                TraceLoggingLevel(WINEVENT_LEVEL_VERBOSE),
                TraceLoggingPointer(this, "this"),
                TraceLoggingWideString(L"Initializing device-side BiDi", MIDI_TRACE_EVENT_MESSAGE_FIELD),
                TraceLoggingWideString(m_endpointId.c_str(), MIDI_TRACE_EVENT_DEVICE_SWD_ID_FIELD)
            );

            m_callback = Callback;
            m_isDeviceSide = true;

            LOG_IF_FAILED(hr = AbstractionState::Current().GetEndpointTable()->OnDeviceConnected(m_endpointId, this));
        }
        else if (internal::EndpointInterfaceIdContainsString(m_endpointId, MIDI_VIRT_INSTANCE_ID_CLIENT_PREFIX))
        {
            TraceLoggingWrite(
                MidiVirtualMidiAbstractionTelemetryProvider::Provider(),
                MIDI_TRACE_EVENT_VERBOSE,
                TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
                TraceLoggingLevel(WINEVENT_LEVEL_VERBOSE),
                TraceLoggingPointer(this, "this"),
                TraceLoggingWideString(L"Initializing client-side BiDi", MIDI_TRACE_EVENT_MESSAGE_FIELD),
                TraceLoggingWideString(m_endpointId.c_str(), MIDI_TRACE_EVENT_DEVICE_SWD_ID_FIELD)
            );

            m_callback = Callback;
            m_isDeviceSide = false;

            LOG_IF_FAILED(hr = AbstractionState::Current().GetEndpointTable()->OnClientConnected(m_endpointId, this));
        }
        else
        {
            TraceLoggingWrite(
                MidiVirtualMidiAbstractionTelemetryProvider::Provider(),
                MIDI_TRACE_EVENT_ERROR,
                TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
                TraceLoggingLevel(WINEVENT_LEVEL_ERROR),
                TraceLoggingPointer(this, "this"),
                TraceLoggingWideString(L"We don't understand the endpoint Id", MIDI_TRACE_EVENT_MESSAGE_FIELD),
                TraceLoggingWideString(m_endpointId.c_str(), MIDI_TRACE_EVENT_DEVICE_SWD_ID_FIELD)
                );

            // we don't understand this endpoint id

            hr = E_FAIL;
        }

        return hr;
    }
    //else
    //{
    //    // we're in protocol negotiation
    //    m_isDeviceSide = false;
    //    return S_OK;
    //}
}

HRESULT
CMidi2VirtualMidiBiDi::Cleanup()
{
    TraceLoggingWrite(
        MidiVirtualMidiAbstractionTelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_INFO,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this"),
        TraceLoggingWideString(L"Enter", MIDI_TRACE_EVENT_MESSAGE_FIELD),
        TraceLoggingWideString(m_endpointId.c_str(), MIDI_TRACE_EVENT_DEVICE_SWD_ID_FIELD),
        TraceLoggingBool(m_isDeviceSide, "is device side")
        );

    m_callback = nullptr;
    m_callbackContext = 0;

    UnlinkAllAssociatedBiDi();

    if (m_isDeviceSide)
    {
        LOG_IF_FAILED(AbstractionState::Current().GetEndpointTable()->OnDeviceDisconnected(m_endpointId));
    }
    else
    {
        LOG_IF_FAILED(AbstractionState::Current().GetEndpointTable()->OnClientDisconnected(m_endpointId, this));
    }

    return S_OK;
}

_Use_decl_annotations_
HRESULT
CMidi2VirtualMidiBiDi::SendMidiMessage(
    PVOID Message,
    UINT Size,
    LONGLONG Position
)
{
    TraceLoggingWrite(
        MidiVirtualMidiAbstractionTelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_VERBOSE,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_VERBOSE),
        TraceLoggingPointer(this, "this"),
        TraceLoggingWideString(L"Enter", MIDI_TRACE_EVENT_MESSAGE_FIELD),
        TraceLoggingWideString(m_endpointId.c_str(), MIDI_TRACE_EVENT_DEVICE_SWD_ID_FIELD),
        TraceLoggingBool(m_isDeviceSide, "is device side"),
        TraceLoggingUInt32(Size, "bytes"),
        TraceLoggingUInt64(Position, "timestamp")
    );


    // message received from the device

    RETURN_HR_IF_NULL(E_INVALIDARG, Message);
    RETURN_HR_IF(E_INVALIDARG, Size < sizeof(uint32_t));

    if (m_linkedBiDiConnections.size() > 0)
    {
        for (auto connection : m_linkedBiDiConnections)
        {
            auto cb = connection->GetCallback();

            if (cb != nullptr)
            {
                LOG_IF_FAILED(cb->Callback(Message, Size, Position, m_callbackContext));
            }
        }
    }
    else
    {
        TraceLoggingWrite(
            MidiVirtualMidiAbstractionTelemetryProvider::Provider(),
            MIDI_TRACE_EVENT_VERBOSE,
            TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
            TraceLoggingLevel(WINEVENT_LEVEL_VERBOSE),
            TraceLoggingPointer(this, "this"),
            TraceLoggingWideString(L"No linked connections", MIDI_TRACE_EVENT_MESSAGE_FIELD),
            TraceLoggingWideString(m_endpointId.c_str(), MIDI_TRACE_EVENT_DEVICE_SWD_ID_FIELD),
            TraceLoggingBool(m_isDeviceSide, "is device side"),
            TraceLoggingUInt32(Size, "bytes"),
            TraceLoggingUInt64(Position, "timestamp")
        );
    }

    return S_OK;

}

_Use_decl_annotations_
HRESULT
CMidi2VirtualMidiBiDi::Callback(
    PVOID Message,
    UINT Size,
    LONGLONG Position,
    LONGLONG Context
)
{
    TraceLoggingWrite(
        MidiVirtualMidiAbstractionTelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_VERBOSE,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_VERBOSE),
        TraceLoggingPointer(this, "this"),
        TraceLoggingWideString(L"Enter", MIDI_TRACE_EVENT_MESSAGE_FIELD),
        TraceLoggingWideString(m_endpointId.c_str(), MIDI_TRACE_EVENT_DEVICE_SWD_ID_FIELD),
        TraceLoggingBool(m_isDeviceSide, "is device side"),
        TraceLoggingUInt32(Size, "bytes"),
        TraceLoggingUInt64(Position, "timestamp")
    );

    // message received from the client

    if (m_callback != nullptr)
    {
        return m_callback->Callback(Message, Size, Position, Context);
    }

    return S_OK;
}

