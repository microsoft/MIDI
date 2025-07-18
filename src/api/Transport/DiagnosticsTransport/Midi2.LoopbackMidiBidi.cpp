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
CMidi2LoopbackMidiBidi::Initialize(
    LPCWSTR endpointId,
    PTRANSPORTCREATIONPARAMS creationParams,
    DWORD *,
    IMidiCallback * callback,
    LONGLONG context,
    GUID /* SessionId */

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

    m_Callback = callback;
    m_Context = context;

    // loopback supports only UMP, reject requests for bytestream
    if (creationParams->DataFormat != MidiDataFormats_UMP && 
        creationParams->DataFormat != MidiDataFormats_Any)
    {
        RETURN_IF_FAILED(HRESULT_FROM_WIN32(ERROR_UNSUPPORTED_TYPE));
    }

    creationParams->DataFormat = MidiDataFormats_UMP;

    std::wstring id{ endpointId };
    internal::InPlaceToLower(id);

    std::wstring pingBidiId{ DEFAULT_PING_BIDI_ID };
    internal::InPlaceToLower(pingBidiId);

    std::wstring loopBidiAId{ DEFAULT_LOOPBACK_BIDI_A_ID };
    internal::InPlaceToLower(loopBidiAId);

    std::wstring loopBidiBId{ DEFAULT_LOOPBACK_BIDI_B_ID };
    internal::InPlaceToLower(loopBidiBId);

    
    // Both loopback endpoints share the same internal device as a simple way of routing between the two. 
    // The Ping device is separate

    m_IsEndpointA = false;
    m_LoopbackMidiDevice = nullptr;
    m_PingMidiDevice = nullptr;

    if (id.find(pingBidiId) != std::wstring::npos)
    {
        // ping bidi endpoint

        m_IsPing = true;

        m_PingMidiDevice = (MidiPingBidiDevice*)(MidiDeviceTable::Current().GetPingDevice());
        m_PingMidiDevice->SetCallback(this, 0);
    }
    else if (id.find(loopBidiAId) != std::wstring::npos)
    {
        // bidi endpoint A

        m_IsEndpointA = true;
        m_LoopbackMidiDevice = (MidiLoopbackBidiDevice*)(MidiDeviceTable::Current().GetBidiDevice());
        m_LoopbackMidiDevice->SetCallbackA(this, 0);
    }
    else if (id.find(loopBidiBId) != std::wstring::npos)
    {
        // bidi endpoint B

        m_IsEndpointA = false;
        m_LoopbackMidiDevice = (MidiLoopbackBidiDevice*)(MidiDeviceTable::Current().GetBidiDevice());
        m_LoopbackMidiDevice->SetCallbackB(this, 0);
    }
    else
    {
        return E_FAIL;
    }

    //RETURN_HR_IF_NULL(E_POINTER, m_MidiDevice);

    return S_OK;
}

HRESULT
CMidi2LoopbackMidiBidi::Shutdown()
{
    TraceLoggingWrite(
        MidiDiagnosticsTransportTelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_INFO,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this")
        );

    if (m_LoopbackMidiDevice != nullptr && m_IsEndpointA)
    {
        m_LoopbackMidiDevice->ShutdownA();
        m_LoopbackMidiDevice = nullptr;
    }
    else if (m_LoopbackMidiDevice != nullptr && !m_IsEndpointA)
    {
        m_LoopbackMidiDevice->ShutdownB();
        m_LoopbackMidiDevice = nullptr;
    }
    else if (m_PingMidiDevice != nullptr && m_IsPing)
    {
        m_PingMidiDevice->Shutdown();
        m_PingMidiDevice = nullptr;
    }

    m_Callback = nullptr;
    m_Context = 0;

    return S_OK;
}

_Use_decl_annotations_
HRESULT
CMidi2LoopbackMidiBidi::SendMidiMessage(
    PVOID message,
    UINT size,
    LONGLONG timestamp
)
{
    RETURN_HR_IF_NULL(E_INVALIDARG, message);
    RETURN_HR_IF(E_INVALIDARG, size < sizeof(uint32_t));
    
    if (m_IsPing)
    {
        RETURN_HR_IF_NULL(E_POINTER, m_PingMidiDevice);
        return m_PingMidiDevice->SendMidiMessage(message, size, timestamp);
    }
    else if (m_IsEndpointA)
    {
        RETURN_HR_IF_NULL(E_POINTER, m_LoopbackMidiDevice);
        return m_LoopbackMidiDevice->SendMidiMessageFromAToB(message, size, timestamp);
    }
    else
    {
        RETURN_HR_IF_NULL(E_POINTER, m_LoopbackMidiDevice);
        return m_LoopbackMidiDevice->SendMidiMessageFromBToA(message, size, timestamp);
    }
}

_Use_decl_annotations_
HRESULT
CMidi2LoopbackMidiBidi::Callback(
    PVOID message,
    UINT size,
    LONGLONG timestamp,
    LONGLONG
)
{
    TraceLoggingWrite(
        MidiDiagnosticsTransportTelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_INFO,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this")
    );

    RETURN_HR_IF_NULL(E_INVALIDARG, message);
    RETURN_HR_IF_NULL(E_POINTER, m_Callback);
    RETURN_HR_IF(E_INVALIDARG, size < sizeof(uint32_t));

    return m_Callback->Callback(message, size, timestamp, m_Context);
}

