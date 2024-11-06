// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================


#include "pch.h"
//#include "midi2.DiagnosticsTransport.h"


_Use_decl_annotations_
HRESULT
CMidi2PingMidiBiDi::Initialize(
    LPCWSTR endpointId,
    PTRANSPORTCREATIONPARAMS creationParams,
    DWORD*,
    IMidiCallback* callback,
    LONGLONG context,
    GUID /* sessionId */
)
{
    TraceLoggingWrite(
        MidiDiagnosticsTransportTelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_INFO,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this")
    );

    RETURN_HR_IF_NULL(E_INVALIDARG, callback);
    RETURN_HR_IF_NULL(E_INVALIDARG, creationParams);

    // ping supports only UMP, reject requests for bytestream
    if (creationParams->DataFormat != MidiDataFormats_UMP && 
        creationParams->DataFormat != MidiDataFormats_Any)
    {
        RETURN_IF_FAILED(HRESULT_FROM_WIN32(ERROR_UNSUPPORTED_TYPE));
    }

    creationParams->DataFormat = MidiDataFormats_UMP;
    m_Callback = callback;
    m_Context = context;

    // really should uppercase this

    std::wstring id{ endpointId };

    m_MidiDevice = (MidiPingBidiDevice*)(MidiDeviceTable::Current().GetPingDevice());

    RETURN_HR_IF_NULL(E_POINTER, m_MidiDevice);

    m_MidiDevice->SetCallback(this, 0);


    return S_OK;
}

HRESULT
CMidi2PingMidiBiDi::Shutdown()
{
    TraceLoggingWrite(
        MidiDiagnosticsTransportTelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_INFO,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this")
    );


    if (m_MidiDevice != nullptr) m_MidiDevice->Shutdown();

    m_MidiDevice = nullptr;

    m_Callback = nullptr;
    m_Context = 0;

    return S_OK;
}

_Use_decl_annotations_
HRESULT
CMidi2PingMidiBiDi::SendMidiMessage(
    PVOID message,
    UINT size,
    LONGLONG timestamp
)
{
    RETURN_HR_IF_NULL(E_INVALIDARG, message);
    RETURN_HR_IF_NULL(E_POINTER, m_MidiDevice);
    RETURN_HR_IF(E_INVALIDARG, size < sizeof(uint32_t));


    return m_MidiDevice->SendMidiMessage(message, size, timestamp);
}

_Use_decl_annotations_
HRESULT
CMidi2PingMidiBiDi::Callback(
    PVOID message,
    UINT size,
    LONGLONG timestamp,
    LONGLONG
)
{
    RETURN_HR_IF_NULL(E_INVALIDARG, message);
    RETURN_HR_IF_NULL(E_POINTER, m_Callback);
    RETURN_HR_IF(E_INVALIDARG, size < sizeof(uint32_t));

    return m_Callback->Callback(message, size, timestamp, m_Context);
}

