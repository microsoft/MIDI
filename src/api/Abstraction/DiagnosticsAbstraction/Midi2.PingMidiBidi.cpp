// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://github.com/microsoft/MIDI/
// ============================================================================


#include "pch.h"
#include "midi2.DiagnosticsAbstraction.h"

#include "diagnostics_defs.h"


_Use_decl_annotations_
HRESULT
CMidi2PingMidiBiDi::Initialize(
    LPCWSTR endpointId,
    DWORD*,
    IMidiCallback* callback
)
{
    TraceLoggingWrite(
        MidiDiagnosticsAbstractionTelemetryProvider::Provider(),
        __FUNCTION__,
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this")
    );

    RETURN_HR_IF_NULL(E_INVALIDARG, callback);
    m_callback = callback;

    // really should uppercase this

    std::wstring id{ endpointId };

    m_midiDevice = (MidiPingBidiDevice*)(MidiDeviceTable::Current().GetPingDevice());

    RETURN_HR_IF_NULL(E_POINTER, m_midiDevice);

    m_midiDevice->SetCallback(this);


    return S_OK;
}

HRESULT
CMidi2PingMidiBiDi::Cleanup()
{
    TraceLoggingWrite(
        MidiDiagnosticsAbstractionTelemetryProvider::Provider(),
        __FUNCTION__,
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this")
    );

    m_callback = nullptr;
    m_midiDevice = nullptr;

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
    RETURN_HR_IF_NULL(E_POINTER, m_midiDevice);
    RETURN_HR_IF(E_INVALIDARG, size < sizeof(uint32_t));


    return m_midiDevice->SendMidiMessage(message, size, timestamp);
}

_Use_decl_annotations_
HRESULT
CMidi2PingMidiBiDi::Callback(
    PVOID message,
    UINT size,
    LONGLONG timestamp
)
{
    RETURN_HR_IF_NULL(E_INVALIDARG, message);
    RETURN_HR_IF_NULL(E_POINTER, m_callback);
    RETURN_HR_IF(E_INVALIDARG, size < sizeof(uint32_t));

    return m_callback->Callback(message, size, timestamp);
}

