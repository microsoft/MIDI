// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://github.com/microsoft/MIDI/
// ============================================================================


#include "pch.h"
//#include "midi2.DiagnosticsAbstraction.h"

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


    std::wstring id{ endpointId };
    InPlaceToLower(id);

    std::wstring pingBiDiId{ DEFAULT_PING_BIDI_ID };
    InPlaceToLower(pingBiDiId);

    std::wstring loopBiDiAId{ DEFAULT_LOOPBACK_BIDI_A_ID };
    InPlaceToLower(loopBiDiAId);

    std::wstring loopBiDiBId{ DEFAULT_LOOPBACK_BIDI_B_ID };
    InPlaceToLower(loopBiDiBId);

    
    // Both loopback endpoints share the same internal device as a simple way of routing between the two. 
    // The Ping device is separate

    m_isEndpointA = false;
    m_loopbackMidiDevice = nullptr;
    m_pingMidiDevice = nullptr;



    if (id.find(pingBiDiId) != std::wstring::npos)
    {
        // ping bidi endpoint

        m_isPing = true;

        m_pingMidiDevice = (MidiPingBidiDevice*)(MidiDeviceTable::Current().GetPingDevice());
        m_pingMidiDevice->SetCallback(this);
    }
    else if (id.find(loopBiDiAId) != std::wstring::npos)
    {
        // bidi endpoint A

        m_isEndpointA = true;
        m_loopbackMidiDevice = (MidiLoopbackBidiDevice*)(MidiDeviceTable::Current().GetBidiDevice());
        m_loopbackMidiDevice->SetCallbackA(this);
    }
    else if (id.find(loopBiDiBId) != std::wstring::npos)
    {
        // bidi endpoint B

        m_isEndpointA = false;
        m_loopbackMidiDevice = (MidiLoopbackBidiDevice*)(MidiDeviceTable::Current().GetBidiDevice());
        m_loopbackMidiDevice->SetCallbackB(this);
    }
    else
    {
        return E_FAIL;
    }

    //RETURN_HR_IF_NULL(E_POINTER, m_midiDevice);

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


    if (m_loopbackMidiDevice != nullptr && m_isEndpointA)
    {
        m_loopbackMidiDevice->CleanupA();
        m_loopbackMidiDevice = nullptr;
    }
    else if (m_loopbackMidiDevice != nullptr && !m_isEndpointA)
    {
        m_loopbackMidiDevice->CleanupB();
        m_loopbackMidiDevice = nullptr;
    }
    else if (m_pingMidiDevice != nullptr && m_isPing)
    {
        m_pingMidiDevice->Cleanup();
        m_pingMidiDevice = nullptr;
    }

    m_callback = nullptr;

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
    RETURN_HR_IF(E_INVALIDARG, size < sizeof(uint32_t));
    
    if (m_isPing)
    {
        RETURN_HR_IF_NULL(E_POINTER, m_pingMidiDevice);
        return m_pingMidiDevice->SendMidiMessage(message, size, timestamp);
    }
    else if (m_isEndpointA)
    {
        RETURN_HR_IF_NULL(E_POINTER, m_loopbackMidiDevice);
        return m_loopbackMidiDevice->SendMidiMessageFromAToB(message, size, timestamp);
    }
    else
    {
        RETURN_HR_IF_NULL(E_POINTER, m_loopbackMidiDevice);
        return m_loopbackMidiDevice->SendMidiMessageFromBToA(message, size, timestamp);
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

