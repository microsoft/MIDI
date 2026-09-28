// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// The service's view of one rtpMIDI connection's endpoint. No tracing on the message
// path: it runs for every message on every connection.
// ============================================================================

#include "pch.h"

_Use_decl_annotations_
HRESULT
CMidi2RtpMidiBidi::Initialize(
    LPCWSTR endpointDeviceInterfaceId,
    PTRANSPORTCREATIONPARAMS,
    DWORD*,
    IMidiCallback* callback,
    LONGLONG context,
    GUID sessionId)
{
    TraceLoggingWrite(
        MidiRtpMidiTransportTelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_INFO,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this"),
        TraceLoggingWideString(L"Enter", MIDI_TRACE_EVENT_MESSAGE_FIELD),
        TraceLoggingWideString(endpointDeviceInterfaceId, MIDI_TRACE_EVENT_DEVICE_SWD_ID_FIELD),
        TraceLoggingGuid(sessionId, "session")
    );

    try
    {
        RETURN_HR_IF_NULL(E_INVALIDARG, endpointDeviceInterfaceId);
        RETURN_HR_IF_NULL(E_INVALIDARG, callback);

        auto endpointManager = TransportState::Current().GetEndpointManager();
        RETURN_HR_IF_NULL(E_UNEXPECTED, endpointManager);

        auto connection = endpointManager->FindConnectionByEndpointDeviceInterfaceId(endpointDeviceInterfaceId);
        RETURN_HR_IF_NULL(E_NOTFOUND, connection);

        {
            auto lock = std::scoped_lock{ m_lock };

            m_callback = callback;
            m_connection = connection;
        }

        RETURN_IF_FAILED(connection->ConnectMidiCallback(this, context));

        return S_OK;
    }
    CATCH_RETURN();
}


HRESULT
CMidi2RtpMidiBidi::Shutdown()
{
    TraceLoggingWrite(
        MidiRtpMidiTransportTelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_INFO,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this"),
        TraceLoggingWideString(L"Enter", MIDI_TRACE_EVENT_MESSAGE_FIELD)
    );

    try
    {
        std::shared_ptr<RtpMidiConnection> connection{ nullptr };

        {
            auto lock = std::scoped_lock{ m_lock };
            connection = m_connection.lock();
        }

        // before the callback goes, so the connection stops handing messages to this endpoint
        if (connection != nullptr)
        {
            LOG_IF_FAILED(connection->DisconnectMidiCallbackIfCurrent(this));
        }

        wil::com_ptr_nothrow<IMidiCallback> callback{ nullptr };

        {
            auto lock = std::scoped_lock{ m_lock };

            m_connection.reset();
            callback = std::move(m_callback);
        }

        // released outside the lock, and a message already on its way holds its own reference
        callback.reset();

        return S_OK;
    }
    CATCH_RETURN();
}


_Use_decl_annotations_
HRESULT
CMidi2RtpMidiBidi::SendMidiMessage(
    MessageOptionFlags,
    PVOID data,
    UINT length,
    LONGLONG)
{
    try
    {
        RETURN_HR_IF_NULL(E_INVALIDARG, data);
        RETURN_HR_IF(E_INVALIDARG, length < sizeof(uint32_t));

        std::shared_ptr<RtpMidiConnection> connection{ nullptr };

        {
            auto lock = std::scoped_lock{ m_lock };
            connection = m_connection.lock();
        }

        RETURN_HR_IF_NULL(HRESULT_FROM_WIN32(ERROR_DEVICE_NOT_CONNECTED), connection);

        return connection->SendToNetwork(data, length);
    }
    CATCH_RETURN();
}


_Use_decl_annotations_
HRESULT
CMidi2RtpMidiBidi::Callback(
    MessageOptionFlags optionFlags,
    PVOID data,
    UINT length,
    LONGLONG timestamp,
    LONGLONG context)
{
    try
    {
        RETURN_HR_IF(E_INVALIDARG, length < sizeof(uint32_t));

        wil::com_ptr_nothrow<IMidiCallback> callback{ nullptr };

        {
            auto lock = std::scoped_lock{ m_lock };
            callback = m_callback;
        }

        // the endpoint was closed while this message was on its way
        if (callback == nullptr) return S_FALSE;

        return callback->Callback(optionFlags, data, length, timestamp, context);
    }
    CATCH_RETURN();
}
