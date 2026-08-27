// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://github.com/microsoft/MIDI/
// ============================================================================


#include "pch.h"

_Use_decl_annotations_
HRESULT
CMidi2BluetoothMidiBidi::Initialize(
    LPCWSTR endpointDeviceInterfaceId,
    PTRANSPORTCREATIONPARAMS,
    DWORD *,
    IMidiCallback * callback,
    LONGLONG context,
    GUID sessionId
)
{
    TraceLoggingWrite(
        MidiBluetoothMidiTransportTelemetryProvider::Provider(),
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

    auto connection = TransportState::Current().GetConnectionByEndpointDeviceInterfaceId(m_endpointDeviceInterfaceId);
    RETURN_HR_IF_NULL(E_NOTFOUND, connection);

    m_connection = connection;

    RETURN_IF_FAILED(connection->ConnectMidiCallback(this, context));

    return S_OK;
}

HRESULT
CMidi2BluetoothMidiBidi::Shutdown()
{
    TraceLoggingWrite(
        MidiBluetoothMidiTransportTelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_INFO,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this"),
        TraceLoggingWideString(L"Enter", MIDI_TRACE_EVENT_MESSAGE_FIELD)
    );

    if (auto connection = m_connection.lock())
    {
        LOG_IF_FAILED(connection->DisconnectMidiCallback());
    }

    m_connection.reset();

    m_callback = nullptr;
    m_context = 0;

    return S_OK;
}

_Use_decl_annotations_
HRESULT
CMidi2BluetoothMidiBidi::SendMidiMessage(
    MessageOptionFlags optionFlags,
    PVOID data,
    UINT length,
    LONGLONG position
)
{
#ifdef _DEBUG
    TraceLoggingWrite(
        MidiBluetoothMidiTransportTelemetryProvider::Provider(),
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
        MidiBluetoothMidiTransportTelemetryProvider::Provider(),
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

    auto connection = m_connection.lock();
    RETURN_HR_IF_NULL(HRESULT_FROM_WIN32(ERROR_DEVICE_NOT_CONNECTED), connection);

    RETURN_IF_FAILED(connection->QueueMidiMessagesToSendToDevice(data, length));

    return S_OK;
}

_Use_decl_annotations_
HRESULT
CMidi2BluetoothMidiBidi::Callback(
    MessageOptionFlags optionFlags,
    PVOID data,
    UINT length,
    LONGLONG timestamp,
    LONGLONG context
)
{
#ifdef _DEBUG
    TraceLoggingWrite(
        MidiBluetoothMidiTransportTelemetryProvider::Provider(),
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
        MidiBluetoothMidiTransportTelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_INFO,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this"),
        TraceLoggingWideString(L"Enter", MIDI_TRACE_EVENT_MESSAGE_FIELD),
        TraceLoggingPointer(data, "data pointer"),
        TraceLoggingUInt32(length, "Byte count")
    );
#endif

    RETURN_HR_IF_NULL(E_UNEXPECTED, m_callback);
    RETURN_HR_IF(E_INVALIDARG, length < sizeof(uint32_t));

    RETURN_IF_FAILED(m_callback->Callback(optionFlags, data, length, timestamp, context));

    return S_OK;
}

