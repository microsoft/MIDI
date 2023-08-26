// Copyright (c) Microsoft Corporation. All rights reserved.

#include "pch.h"
#include "midi2.ksabstraction.h"

_Use_decl_annotations_
HRESULT
CMidi2KSMidiIn::Initialize(
    LPCWSTR Device,
    DWORD * MmCssTaskId,
    IMidiCallback * Callback
)
{
    RETURN_HR_IF(E_INVALIDARG, nullptr == Callback);
    RETURN_HR_IF(E_INVALIDARG, nullptr == Device);
    RETURN_HR_IF(E_INVALIDARG, nullptr == MmCssTaskId);

    TraceLoggingWrite(
        MidiKSAbstractionTelemetryProvider::Provider(),
        __FUNCTION__,
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this"),
        TraceLoggingWideString(Device, "Device"),
        TraceLoggingHexUInt32(*MmCssTaskId, "MmCss Id"),
        TraceLoggingPointer(Callback, "callback")
        );

    std::unique_ptr<CMidi2KSMidi> midiDevice(new (std::nothrow) CMidi2KSMidi());
    RETURN_IF_NULL_ALLOC(midiDevice);

    RETURN_IF_FAILED(midiDevice->Initialize(Device, MidiFlowIn, MmCssTaskId, Callback));
    m_MidiDevice = std::move(midiDevice);

    return S_OK;
}

HRESULT
CMidi2KSMidiIn::Cleanup()
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

