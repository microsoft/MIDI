// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================



#include "pch.h"
#include "midi2.ksabstraction.h"


_Use_decl_annotations_
HRESULT
CMidi2KSMidiOut::Initialize(
    LPCWSTR Device,   
    PABSTRACTIONCREATIONPARAMS CreationParams,
    DWORD * MmCssTaskId,
    GUID /* SessionId */
)
{
    RETURN_HR_IF(E_INVALIDARG, nullptr == Device);
    RETURN_HR_IF(E_INVALIDARG, nullptr == MmCssTaskId);
    RETURN_HR_IF(E_INVALIDARG, nullptr == CreationParams);

    TraceLoggingWrite(
        MidiKSAbstractionTelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_INFO,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this"),
        TraceLoggingWideString(Device, MIDI_TRACE_EVENT_DEVICE_SWD_ID_FIELD),
        TraceLoggingHexUInt32(*MmCssTaskId, "MmCss Id")
        );

    std::unique_ptr<CMidi2KSMidi> midiDevice(new (std::nothrow) CMidi2KSMidi());
    RETURN_IF_NULL_ALLOC(midiDevice);

    RETURN_IF_FAILED(midiDevice->Initialize(Device, MidiFlowOut, CreationParams, MmCssTaskId, nullptr, 0));
    m_MidiDevice = std::move(midiDevice);

    return S_OK;
}

HRESULT
CMidi2KSMidiOut::Cleanup()
{
    TraceLoggingWrite(
        MidiKSAbstractionTelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_INFO,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this")
        );

    if (m_MidiDevice)
    {
        m_MidiDevice->Cleanup();
        m_MidiDevice.reset();
    }

    return S_OK;
}

_Use_decl_annotations_
HRESULT
CMidi2KSMidiOut::SendMidiMessage(
    PVOID Data,
    UINT Length,
    LONGLONG Position
)
{
    if (m_MidiDevice)
    {
        return m_MidiDevice->SendMidiMessage(Data, Length, Position);
    }

    return E_ABORT;
}

