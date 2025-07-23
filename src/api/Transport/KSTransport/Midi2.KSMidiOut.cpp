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
CMidi2KSMidiOut::Initialize(
    LPCWSTR device,   
    PTRANSPORTCREATIONPARAMS creationParams,
    DWORD * mmcssTaskId,
    GUID SessionId
)
{
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
        TraceLoggingGuid(SessionId, "Session Id")
    );

    std::unique_ptr<CMidi2KSMidi> midiDevice(new (std::nothrow) CMidi2KSMidi());
    RETURN_IF_NULL_ALLOC(midiDevice);

    RETURN_IF_FAILED(midiDevice->Initialize(device, MidiFlowOut, creationParams, mmcssTaskId, nullptr, 0));
    m_midiDevice = std::move(midiDevice);

    return S_OK;
}

HRESULT
CMidi2KSMidiOut::Shutdown()
{
    TraceLoggingWrite(
        MidiKSTransportTelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_INFO,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this")
        );

    if (m_midiDevice)
    {
        m_midiDevice->Shutdown();
        m_midiDevice.reset();
    }

    return S_OK;
}

_Use_decl_annotations_
HRESULT
CMidi2KSMidiOut::SendMidiMessage(
    MessageOptionFlags optionFlags,
    PVOID data,
    UINT length,
    LONGLONG position
)
{
    if (m_midiDevice)
    {
        return m_midiDevice->SendMidiMessage(optionFlags, data, length, position);
    }

    return E_ABORT;
}

