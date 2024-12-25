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
MidiNetworkSession::Initialize(IMidiCallback* callback)
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

    return S_OK;
}

_Use_decl_annotations_
HRESULT 
MidiNetworkSession::ReceiveMidiMessagesFromNetwork(uint16_t const sequenceNumber, std::vector<uint32_t> const& words)
{
    UNREFERENCED_PARAMETER(sequenceNumber);
    UNREFERENCED_PARAMETER(words);

    // receive a block of message data from the network, parse them, and begin sending 
    // them to the BiDi callback, one message at a time

    // timestamps are generated here



    return S_OK;
}


_Use_decl_annotations_
HRESULT
MidiNetworkSession::SendMidiMessage(
    PVOID message,
    UINT size,
    LONGLONG timestamp
)
{
    UNREFERENCED_PARAMETER(message);
    UNREFERENCED_PARAMETER(size);
    UNREFERENCED_PARAMETER(timestamp);


    // we simply enqueue messages so the host can send them




    return S_OK;
}



HRESULT 
MidiNetworkSession::Shutdown()
{
    TraceLoggingWrite(
        MidiNetworkMidiTransportTelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_INFO,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this"),
        TraceLoggingWideString(L"Enter", MIDI_TRACE_EVENT_MESSAGE_FIELD)
    );



    return S_OK;
}
