// Copyright (c) Microsoft Corporation. All rights reserved.

#include "pch.h"
#include "midi2.ksabstraction.h"


_Use_decl_annotations_
HRESULT
CMidi2KSMidiOut::Initialize(
    LPCWSTR Device,
    DWORD * MmCssTaskId
)
{
    RETURN_HR_IF(E_INVALIDARG, nullptr == Device);
    RETURN_HR_IF(E_INVALIDARG, nullptr == MmCssTaskId);

    TraceLoggingWrite(
        MidiKSAbstractionTelemetryProvider::Provider(),
        __FUNCTION__,
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this"),
        TraceLoggingWideString(Device, "Device"),
        TraceLoggingHexUInt32(*MmCssTaskId, "MmCss Id")
        );

    std::unique_ptr<CMidi2KSMidi> midiDevice(new (std::nothrow) CMidi2KSMidi());
    RETURN_IF_NULL_ALLOC(midiDevice);

    RETURN_IF_FAILED(midiDevice->Initialize(Device, FALSE, TRUE, MmCssTaskId, nullptr));
    m_MidiDevice = std::move(midiDevice);

    return S_OK;
}

HRESULT
CMidi2KSMidiOut::Cleanup()
{
    TraceLoggingWrite(
        MidiKSAbstractionTelemetryProvider::Provider(),
        __FUNCTION__,
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

