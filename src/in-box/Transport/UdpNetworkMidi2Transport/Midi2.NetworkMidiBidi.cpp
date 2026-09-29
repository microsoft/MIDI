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
    LONGLONG,
    GUID sessionId
)
try
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

    RETURN_HR_IF_NULL(E_INVALIDARG, endpointDeviceInterfaceId);
    RETURN_HR_IF_NULL(E_INVALIDARG, callback);

    m_endpointDeviceInterfaceId = internal::NormalizeEndpointInterfaceIdWStringCopy(endpointDeviceInterfaceId);

    auto connection = TransportState::Current().GetSessionConnection(m_endpointDeviceInterfaceId);
    RETURN_HR_IF_NULL(E_INVALIDARG, connection);

    {
        auto lock = m_lock.lock();

        m_callback = callback;
        m_connection = connection;
    }

    RETURN_IF_FAILED(connection->ConnectMidiCallback(this));

    return S_OK;
}
CATCH_RETURN()

HRESULT
CMidi2NetworkMidiBidi::Shutdown()
try
{
    TraceLoggingWrite(
        MidiNetworkMidiTransportTelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_INFO,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this"),
        TraceLoggingWideString(L"Enter", MIDI_TRACE_EVENT_MESSAGE_FIELD)
    );

    // The connection holds a reference on us, and dropping it can be the last one
    Microsoft::WRL::ComPtr<IMidiBidirectional> keepAlive(this);

    std::shared_ptr<MidiNetworkConnection> connection{ nullptr };

    {
        auto lock = m_lock.lock();
        connection = m_connection.lock();
    }

    // The service can shut an old instance down after a new one for the same endpoint has
    // connected, so only this instance's own callback is taken away.
    if (connection != nullptr)
    {
        LOG_IF_FAILED(connection->DisconnectMidiCallbackIfCurrent(this));
    }

    wil::com_ptr_nothrow<IMidiCallback> callback{ nullptr };

    {
        auto lock = m_lock.lock();

        m_connection.reset();
        callback = std::move(m_callback);
    }

    // released outside the lock, and a message already on its way holds its own reference
    callback.reset();

    return S_OK;
}
CATCH_RETURN()

_Use_decl_annotations_
HRESULT
CMidi2NetworkMidiBidi::SendMidiMessage(
    MessageOptionFlags optionFlags,
    PVOID data,
    UINT length,
    LONGLONG position
)
try
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

    std::shared_ptr<MidiNetworkConnection> connection{ nullptr };

    {
        auto lock = m_lock.lock();
        connection = m_connection.lock();
    }

    if (connection != nullptr)
    {
        RETURN_IF_FAILED(connection->QueueMidiMessagesToSendToNetwork(data, length));
    }

    return S_OK;
}
CATCH_RETURN()

_Use_decl_annotations_
HRESULT
CMidi2NetworkMidiBidi::Callback(
    MessageOptionFlags optionFlags,
    PVOID data,
    UINT length,
    LONGLONG timestamp,
    LONGLONG context
)
try
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

    wil::com_ptr_nothrow<IMidiCallback> callback{ nullptr };

    {
        auto lock = m_lock.lock();
        callback = m_callback;
    }

    RETURN_HR_IF_NULL(E_UNEXPECTED, callback);

    RETURN_IF_FAILED(callback->Callback(optionFlags, data, length, timestamp, context));

    return S_OK;
}
CATCH_RETURN()

