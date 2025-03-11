// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================


#include "pch.h"
#include "midi2.kstransport.h"

_Use_decl_annotations_
HRESULT
CMidi2KSMidiBidi::Initialize(
    LPCWSTR device,
    PTRANSPORTCREATIONPARAMS creationParams,
    DWORD * mmcssTaskId,
    IMidiCallback * callback,
    LONGLONG context,
    GUID SessionId
)
{
    RETURN_HR_IF(E_INVALIDARG, nullptr == callback);
    RETURN_HR_IF(E_INVALIDARG, nullptr == device);
    RETURN_HR_IF(E_INVALIDARG, nullptr == mmcssTaskId);
    RETURN_HR_IF(E_INVALIDARG, nullptr == creationParams);

    TraceLoggingWrite(
        MidiKSTransportTelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_INFO,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this"),
        TraceLoggingWideString(L"Enter", MIDI_TRACE_EVENT_MESSAGE_FIELD),
        TraceLoggingWideString(device, MIDI_TRACE_EVENT_DEVICE_SWD_ID_FIELD),
        TraceLoggingGuid(SessionId, "Session Id"),
        TraceLoggingPointer(callback, "callback")
    );


    std::unique_ptr<CMidi2KSMidi> midiDevice(new (std::nothrow) CMidi2KSMidi());
    RETURN_IF_NULL_ALLOC(midiDevice);

    RETURN_IF_FAILED(midiDevice->Initialize(device, MidiFlowBidirectional, creationParams, mmcssTaskId, callback, context));
    m_MidiDevice = std::move(midiDevice);

    return S_OK;
}

HRESULT
CMidi2KSMidiBidi::Shutdown()
{
    TraceLoggingWrite(
        MidiKSTransportTelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_INFO,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this"),
        TraceLoggingWideString(L"Enter", MIDI_TRACE_EVENT_MESSAGE_FIELD)
        );

    if (m_MidiDevice)
    {
        m_MidiDevice->Shutdown();
        m_MidiDevice.reset();
    }

    return S_OK;
}

_Use_decl_annotations_
HRESULT
CMidi2KSMidiBidi::SendMidiMessage(
    PVOID data,
    UINT length,
    LONGLONG position
)
{
    if (m_MidiDevice)
    {
        return m_MidiDevice->SendMidiMessage(data, length, position);
    }

    return E_ABORT;
}

