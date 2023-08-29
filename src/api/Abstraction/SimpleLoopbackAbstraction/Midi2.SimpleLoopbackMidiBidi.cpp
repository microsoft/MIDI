// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://github.com/microsoft/MIDI/
// ============================================================================


#include "pch.h"
#include "midi2.SimpleLoopbackabstraction.h"

#include "loopback_defs.h"


_Use_decl_annotations_
HRESULT
CMidi2SimpleLoopbackMidiBiDi::Initialize(
    LPCWSTR endpointId,
    DWORD *,
    IMidiCallback * callback
)
{
    TraceLoggingWrite(
        MidiSimpleLoopbackAbstractionTelemetryProvider::Provider(),
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
CMidi2SimpleLoopbackMidiBiDi::Cleanup()
{
    TraceLoggingWrite(
        MidiSimpleLoopbackAbstractionTelemetryProvider::Provider(),
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
CMidi2SimpleLoopbackMidiBiDi::SendMidiMessage(
    PVOID message,
    UINT size,
    LONGLONG timestamp
)
{
    if (message == nullptr)
    {
        // TODO log that message was null
        return E_INVALIDARG;
    }

    if (size < sizeof(uint32_t))
    {
        // TODO log that data was smaller than minimum UMP size
        return E_INVALIDARG;
    }

    if (m_midiDevice == nullptr)
    {
        // TODO log that midi device is null
        return E_POINTER;
    }

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
CMidi2SimpleLoopbackMidiBiDi::Callback(
    PVOID message,
    UINT size,
    LONGLONG timestamp
)
{
    if (m_callback == nullptr)
    {
        // TODO log that callback is null
        return E_POINTER;
    }

    if (message == nullptr)
    {
        // TODO log that message was null
        return E_INVALIDARG;
    }

    if (size < sizeof(uint32_t))
    {
        // TODO log that data was smaller than minimum UMP size
        return E_INVALIDARG;
    }


    return m_callback->Callback(message, size, timestamp);
}

