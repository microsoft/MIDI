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
    GUID /* SessionId */
)
{
    TraceLoggingWrite(
        MidiNetworkMidiTransportTelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_INFO,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this"),
        TraceLoggingWideString(L"Enter", MIDI_TRACE_EVENT_MESSAGE_FIELD)
    );

    RETURN_HR_IF_NULL(E_INVALIDARG, callback);

    m_callback = callback;
    m_context = context;

    m_endpointDeviceInterfaceId = internal::NormalizeEndpointInterfaceIdWStringCopy(endpointDeviceInterfaceId);


    // look up the endpointDeviceInterfaceId in our list of endpoints, and connect to it

    m_connection = TransportState::Current().GetSessionConnection(m_endpointDeviceInterfaceId);
    RETURN_HR_IF_NULL(E_INVALIDARG, m_connection);

    m_connection->SetMidiCallback(callback);

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

    auto conn = TransportState::Current().GetSessionConnection(m_endpointDeviceInterfaceId);

    RETURN_HR_IF_NULL(E_UNEXPECTED, conn);

    RETURN_IF_FAILED(conn->SendMidiMessagesToNetwork(message, size));

    return S_OK;
}

_Use_decl_annotations_
HRESULT
CMidi2NetworkMidiBiDi::Callback(
    PVOID,
    UINT,
    LONGLONG,
    LONGLONG
)
{
    // this shouldn't be called. The connection uses the direct callback instead
    return E_NOTIMPL;
}

