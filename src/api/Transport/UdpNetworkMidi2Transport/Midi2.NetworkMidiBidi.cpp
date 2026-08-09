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
CMidi2NetworkMidiBidi::Initialize(
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
CMidi2NetworkMidiBidi::Shutdown()
{
    TraceLoggingWrite(
        MidiNetworkMidiTransportTelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_INFO,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this"),
        TraceLoggingWideString(L"Enter", MIDI_TRACE_EVENT_MESSAGE_FIELD)
    );

    // The connection holds a reference on us. Dropping it via DisconnectMidiCallback can be the
    // last one, so we hold ourselves alive until this call returns.
    Microsoft::WRL::ComPtr<IMidiBidirectional> keepAlive(this);

    if (auto ptr = m_connection.lock())
    {
        LOG_IF_FAILED(ptr->DisconnectMidiCallback());
    }

    m_connection.reset();
    m_callback = nullptr;
    m_context = 0;

    return S_OK;
}

_Use_decl_annotations_
HRESULT
CMidi2NetworkMidiBidi::SendMidiMessage(
    MessageOptionFlags optionFlags,
    PVOID data,
    UINT length,
    LONGLONG position
)
{
#ifdef _DEBUG
    TraceLoggingWrite(
        MidiNetworkMidiTransportTelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_INFO,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this"),
        TraceLoggingWideString(L"Enter", MIDI_TRACE_EVENT_MESSAGE_FIELD),
        TraceLoggingHexUInt32Array(static_cast<uint32_t*>(data), static_cast<uint16_t>(length / sizeof(uint32_t)), "data"),
        TraceLoggingUInt32(length, "Byte count")
    );
#else
    TraceLoggingWrite(
        MidiNetworkMidiTransportTelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_INFO,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this"),
        TraceLoggingWideString(L"Enter", MIDI_TRACE_EVENT_MESSAGE_FIELD),
        TraceLoggingPointer(data, "data pointer"),
        TraceLoggingUInt32(length, "Byte count")
    );
#endif

    UNREFERENCED_PARAMETER(position);
    UNREFERENCED_PARAMETER(optionFlags);

    RETURN_HR_IF_NULL(E_INVALIDARG, data);
    RETURN_HR_IF(E_INVALIDARG, length < sizeof(uint32_t));

    if (auto conn = m_connection.lock())
    {
        RETURN_IF_FAILED(conn->QueueMidiMessagesToSendToNetwork(data, length));
    }

    return S_OK;
}

_Use_decl_annotations_
HRESULT
CMidi2NetworkMidiBidi::Callback(
    MessageOptionFlags optionFlags,
    PVOID data,
    UINT length,
    LONGLONG timestamp,
    LONGLONG context
)
{
#ifdef _DEBUG
    TraceLoggingWrite(
        MidiNetworkMidiTransportTelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_INFO,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this"),
        TraceLoggingWideString(L"Enter", MIDI_TRACE_EVENT_MESSAGE_FIELD),
        TraceLoggingHexUInt32Array(static_cast<uint32_t*>(data), static_cast<uint16_t>(length / sizeof(uint32_t)), "data"),
        TraceLoggingUInt32(length, "Byte count")
    );
#else
    TraceLoggingWrite(
        MidiNetworkMidiTransportTelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_INFO,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this"),
        TraceLoggingWideString(L"Enter", MIDI_TRACE_EVENT_MESSAGE_FIELD),
        TraceLoggingPointer(data, "data pointer"),
        TraceLoggingUInt32(length, "Byte count")
    );
#endif

    RETURN_HR_IF_NULL(E_INVALIDARG, data);
    RETURN_HR_IF(E_INVALIDARG, length < sizeof(uint32_t));

    auto callback = m_callback;
    RETURN_HR_IF_NULL(E_UNEXPECTED, callback);

    RETURN_IF_FAILED(callback->Callback(optionFlags, data, length, timestamp, context));

    return S_OK;
}

