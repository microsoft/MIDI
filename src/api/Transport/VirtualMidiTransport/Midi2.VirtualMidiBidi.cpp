// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================


#include "pch.h"
#include "midi2.VirtualMiditransport.h"

_Use_decl_annotations_
HRESULT
CMidi2VirtualMidiBidi::Initialize(
    LPCWSTR endpointId,
    PTRANSPORTCREATIONPARAMS,
    DWORD *,
    IMidiCallback * callback,
    LONGLONG context,
    GUID sessionId
)
{
    TraceLoggingWrite(
        MidiVirtualMidiTransportTelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_INFO,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this"),
        TraceLoggingWideString(L"Enter", MIDI_TRACE_EVENT_MESSAGE_FIELD),
        TraceLoggingWideString(endpointId, MIDI_TRACE_EVENT_DEVICE_SWD_ID_FIELD)
        );

    m_sessionId = sessionId;
    m_callbackContext = context;
    m_callback = callback;
    m_endpointId = internal::NormalizeEndpointInterfaceIdWStringCopy(endpointId);
  
    //if (context != MIDI_PROTOCOL_MANAGER_ENDPOINT_CREATION_CONTEXT)
    {
        HRESULT hr = S_OK;

        // TODO: This should use SWD properties and not a string search

        if (internal::EndpointInterfaceIdContainsString(m_endpointId, MIDI_VIRT_INSTANCE_ID_DEVICE_PREFIX))
        {
            TraceLoggingWrite(
                MidiVirtualMidiTransportTelemetryProvider::Provider(),
                MIDI_TRACE_EVENT_VERBOSE,
                TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
                TraceLoggingLevel(WINEVENT_LEVEL_VERBOSE),
                TraceLoggingPointer(this, "this"),
                TraceLoggingWideString(L"Initializing device-side Bidi", MIDI_TRACE_EVENT_MESSAGE_FIELD),
                TraceLoggingWideString(m_endpointId.c_str(), MIDI_TRACE_EVENT_DEVICE_SWD_ID_FIELD),
                TraceLoggingGuid(m_sessionId, "session id")
            );

            m_isDeviceSide = true;

            LOG_IF_FAILED(hr = TransportState::Current().GetEndpointTable()->OnDeviceConnected(m_endpointId, this));
        }
        else if (internal::EndpointInterfaceIdContainsString(m_endpointId, MIDI_VIRT_INSTANCE_ID_CLIENT_PREFIX))
        {
            TraceLoggingWrite(
                MidiVirtualMidiTransportTelemetryProvider::Provider(),
                MIDI_TRACE_EVENT_VERBOSE,
                TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
                TraceLoggingLevel(WINEVENT_LEVEL_VERBOSE),
                TraceLoggingPointer(this, "this"),
                TraceLoggingWideString(L"Initializing client-side Bidi", MIDI_TRACE_EVENT_MESSAGE_FIELD),
                TraceLoggingWideString(m_endpointId.c_str(), MIDI_TRACE_EVENT_DEVICE_SWD_ID_FIELD),
                TraceLoggingGuid(m_sessionId, "session id")
            );

            m_isDeviceSide = false;

            LOG_IF_FAILED(hr = TransportState::Current().GetEndpointTable()->OnClientConnected(m_endpointId, this));
        }
        else
        {
            TraceLoggingWrite(
                MidiVirtualMidiTransportTelemetryProvider::Provider(),
                MIDI_TRACE_EVENT_ERROR,
                TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
                TraceLoggingLevel(WINEVENT_LEVEL_ERROR),
                TraceLoggingPointer(this, "this"),
                TraceLoggingWideString(L"We don't understand the endpoint Id", MIDI_TRACE_EVENT_MESSAGE_FIELD),
                TraceLoggingWideString(m_endpointId.c_str(), MIDI_TRACE_EVENT_DEVICE_SWD_ID_FIELD),
                TraceLoggingGuid(m_sessionId, "session id")
            );

            // we don't understand this endpoint id

            hr = E_INVALIDARG;
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
CMidi2VirtualMidiBidi::Shutdown()
{
    TraceLoggingWrite(
        MidiVirtualMidiTransportTelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_INFO,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this"),
        TraceLoggingWideString(L"Enter", MIDI_TRACE_EVENT_MESSAGE_FIELD),
        TraceLoggingWideString(m_endpointId.c_str(), MIDI_TRACE_EVENT_DEVICE_SWD_ID_FIELD),
        TraceLoggingBool(m_isDeviceSide, "is device side"),
        TraceLoggingGuid(m_sessionId, "session id")
    );

    m_callback = nullptr;
    m_callbackContext = 0;

    UnlinkAssociatedCallback();
    //m_linkedBidi = nullptr;
    //m_linkedBidiCallback = nullptr;


    if (m_isDeviceSide)
    {
        RETURN_IF_FAILED(TransportState::Current().GetEndpointTable()->OnDeviceDisconnected(m_endpointId));
    }
    else
    {
        RETURN_IF_FAILED(TransportState::Current().GetEndpointTable()->OnClientDisconnected(m_endpointId));
    }

    return S_OK;
}

_Use_decl_annotations_
HRESULT
CMidi2VirtualMidiBidi::SendMidiMessage(
    MessageOptionFlags optionFlags,
    PVOID Message,
    UINT Size,
    LONGLONG Position
)
{
#ifdef _DEBUG
    TraceLoggingWrite(
        MidiVirtualMidiTransportTelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_VERBOSE,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_VERBOSE),
        TraceLoggingPointer(this, "this"),
        TraceLoggingWideString(L"Enter", MIDI_TRACE_EVENT_MESSAGE_FIELD),
        TraceLoggingWideString(m_endpointId.c_str(), MIDI_TRACE_EVENT_DEVICE_SWD_ID_FIELD),
        TraceLoggingBool(m_isDeviceSide, "is device side"),
        TraceLoggingUInt32(static_cast<uint32_t>(optionFlags), "optionFlags"),
        TraceLoggingUInt32(Size, "bytes"),
        TraceLoggingUInt64(Position, "timestamp"),
        TraceLoggingGuid(m_sessionId, "session id")
    );
#endif

    // message received from the device

    RETURN_HR_IF_NULL(E_INVALIDARG, Message);
    RETURN_HR_IF(E_INVALIDARG, Size < sizeof(uint32_t));

    if (m_linkedBidiCallback != nullptr)
    {
        RETURN_IF_FAILED(m_linkedBidiCallback->Callback(optionFlags, Message, Size, Position, m_callbackContext));

        return S_OK;
    }
    else
    {
        if (m_isDeviceSide)
        {
            return S_OK;    // ok for device side to send when nothing is listening
        }
        else
        {
            TraceLoggingWrite(
                MidiVirtualMidiTransportTelemetryProvider::Provider(),
                MIDI_TRACE_EVENT_VERBOSE,
                TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
                TraceLoggingLevel(WINEVENT_LEVEL_VERBOSE),
                TraceLoggingPointer(this, "this"),
                TraceLoggingWideString(L"No linked connections", MIDI_TRACE_EVENT_MESSAGE_FIELD),
                TraceLoggingWideString(m_endpointId.c_str(), MIDI_TRACE_EVENT_DEVICE_SWD_ID_FIELD),
                TraceLoggingBool(m_isDeviceSide, "is device side"),
                TraceLoggingUInt32(Size, "bytes"),
                TraceLoggingUInt64(Position, "timestamp"),
                TraceLoggingGuid(m_sessionId, "session id")
            );

            RETURN_IF_FAILED(E_UNEXPECTED);  // but if the client-side is not connected to a device, something is really wrong.
        }
    }
}

_Use_decl_annotations_
HRESULT
CMidi2VirtualMidiBidi::Callback(
    MessageOptionFlags optionFlags,
    PVOID Message,
    UINT Size,
    LONGLONG Position,
    LONGLONG context
)
{
#ifdef _DEBUG
    TraceLoggingWrite(
        MidiVirtualMidiTransportTelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_VERBOSE,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_VERBOSE),
        TraceLoggingPointer(this, "this"),
        TraceLoggingWideString(L"Enter", MIDI_TRACE_EVENT_MESSAGE_FIELD),
        TraceLoggingWideString(m_endpointId.c_str(), MIDI_TRACE_EVENT_DEVICE_SWD_ID_FIELD),
        TraceLoggingBool(m_isDeviceSide, "is device side"),
        TraceLoggingUInt32(static_cast<uint32_t>(optionFlags), "optionFlags"),
        TraceLoggingUInt32(Size, "bytes"),
        TraceLoggingUInt64(Position, "timestamp"),
        TraceLoggingGuid(m_sessionId, "session id")
    );
#endif

    // message received from the client
    RETURN_HR_IF(E_INVALIDARG, Size < sizeof(uint32_t));

    if (m_callback != nullptr)
    {
        RETURN_IF_FAILED(m_callback->Callback(optionFlags, Message, Size, Position, context));
    }
    else
    {
        RETURN_IF_FAILED(E_UNEXPECTED);
    }

    return S_OK;
}

