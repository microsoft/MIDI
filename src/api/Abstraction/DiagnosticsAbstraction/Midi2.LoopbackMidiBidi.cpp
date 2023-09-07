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
CMidi2LoopbackMidiBiDi::Initialize(
    LPCWSTR endpointId,
    DWORD *,
    IMidiCallback * callback
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
    
    // Both endpoints share the same internal device as a simple way of routing between the two.

    if (id.find(DEFAULT_LOOPBACK_BIDI_A_ID) != std::wstring::npos)
    {
        // bidi endpoint A

        m_isEndpointA = true;
        m_midiDevice = (MidiLoopbackBidiDevice*)(MidiDeviceTable::Current().GetBidiDevice());
        m_midiDevice->SetCallbackA(this);
    }
    else if (id.find(DEFAULT_LOOPBACK_BIDI_B_ID) != std::wstring::npos)
    {
        // bidi endpoint B

        m_isEndpointA = false;
        m_midiDevice = (MidiLoopbackBidiDevice*)(MidiDeviceTable::Current().GetBidiDevice());
        m_midiDevice->SetCallbackB(this);
    }
    else
    {
        return E_FAIL;
    }

    RETURN_HR_IF_NULL(E_POINTER, m_midiDevice);

    return S_OK;
}

HRESULT
CMidi2LoopbackMidiBiDi::Cleanup()
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
CMidi2LoopbackMidiBiDi::SendMidiMessage(
    PVOID message,
    UINT size,
    LONGLONG timestamp
)
{
    RETURN_HR_IF_NULL(E_INVALIDARG, message);
    RETURN_HR_IF_NULL(E_POINTER, m_midiDevice);
    RETURN_HR_IF(E_INVALIDARG, size < sizeof(uint32_t));
    

    if (m_isEndpointA)
    {
        return m_midiDevice->SendMidiMessageFromAToB(message, size, timestamp);
    }
    else
    {
        return m_midiDevice->SendMidiMessageFromBToA(message, size, timestamp);
    }
}

_Use_decl_annotations_
HRESULT
CMidi2LoopbackMidiBiDi::Callback(
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

