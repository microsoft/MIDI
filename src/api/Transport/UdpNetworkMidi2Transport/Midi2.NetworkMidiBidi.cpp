// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://github.com/microsoft/MIDI/
// ============================================================================


#include "pch.h"
#include "midi2.NetworkMidiTransport.h"

_Use_decl_annotations_
HRESULT
CMidi2NetworkMidiBiDi::Initialize(
    LPCWSTR endpointDeviceInterfaceId,
    PTRANSPORTCREATIONPARAMS,
    DWORD *,
    IMidiCallback * callback,
    LONGLONG context,
    GUID sessionId
)
{
    TraceLoggingWrite(
        MidiNetworkMidiTransportTelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_INFO,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this"),
        TraceLoggingWideString(L"Enter", MIDI_TRACE_EVENT_MESSAGE_FIELD),
        TraceLoggingWideString(endpointDeviceInterfaceId, MIDI_TRACE_EVENT_DEVICE_SWD_ID_FIELD),
        TraceLoggingGuid(sessionId, "Session")
    );

    RETURN_HR_IF_NULL(E_INVALIDARG, callback);

    m_callback = callback;
    m_context = context;

    m_endpointDeviceInterfaceId = internal::NormalizeEndpointInterfaceIdWStringCopy(endpointDeviceInterfaceId);


    // look up the endpointDeviceInterfaceId in our list of endpoints, and connect to it

    m_connection = TransportState::Current().GetSessionConnection(m_endpointDeviceInterfaceId);

    if (auto conn = m_connection.lock())
    {
        RETURN_IF_FAILED(conn->ConnectMidiCallback(this));
    }
    else
    {
        RETURN_IF_FAILED(E_INVALIDARG);
    }


    return S_OK;
}

HRESULT
CMidi2NetworkMidiBiDi::Shutdown()
{
    TraceLoggingWrite(
        MidiNetworkMidiTransportTelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_INFO,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this"),
        TraceLoggingWideString(L"Enter", MIDI_TRACE_EVENT_MESSAGE_FIELD)
    );

    // TODO: This causes the service to crash when the remote network endpoint disconnects. Need to look into this further.

    if (auto ptr = m_connection.lock())
    {
        ptr->DisconnectMidiCallback();
        m_connection.reset();
    }

    m_callback = nullptr;
    m_context = 0;

    return S_OK;
}

_Use_decl_annotations_
HRESULT
CMidi2NetworkMidiBiDi::SendMidiMessage(
    PVOID message,
    UINT size,
    LONGLONG position
)
{
    UNREFERENCED_PARAMETER(position);

    RETURN_HR_IF_NULL(E_INVALIDARG, message);
    RETURN_HR_IF(E_INVALIDARG, size < sizeof(uint32_t));

    if (auto conn = m_connection.lock())
    {
        RETURN_IF_FAILED(conn->QueueMidiMessagesToSendToNetwork(message, size));
    }

    return S_OK;
}

_Use_decl_annotations_
HRESULT
CMidi2NetworkMidiBiDi::Callback(
    PVOID message,
    UINT size,
    LONGLONG timestamp,
    LONGLONG context
)
{
    RETURN_HR_IF_NULL(E_UNEXPECTED, m_callback);
    RETURN_HR_IF(E_INVALIDARG, size < sizeof(uint32_t));

    RETURN_IF_FAILED(m_callback->Callback(message, size, timestamp, context));

    return S_OK;
}

