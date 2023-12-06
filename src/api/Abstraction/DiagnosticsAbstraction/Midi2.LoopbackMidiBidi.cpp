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
    LPCWSTR EndpointId,
    PABSTRACTIONCREATIONPARAMS CreationParams,
    DWORD *,
    IMidiCallback * Callback,
    LONGLONG Context
)
{
    TraceLoggingWrite(
        MidiDiagnosticsAbstractionTelemetryProvider::Provider(),
        __FUNCTION__,
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this")
        );

    RETURN_HR_IF_NULL(E_INVALIDARG, Callback);
    RETURN_HR_IF_NULL(E_INVALIDARG, CreationParams);

    m_Callback = Callback;
    m_Context = Context;

    // loopback supports only UMP, reject requests for bytestream
    if (CreationParams->DataFormat != MidiDataFormat_UMP && 
        CreationParams->DataFormat != MidiDataFormat_Any)
    {
        RETURN_IF_FAILED(HRESULT_FROM_WIN32(ERROR_UNSUPPORTED_TYPE));
    }

    CreationParams->DataFormat = MidiDataFormat_UMP;

    std::wstring id{ EndpointId };
    InPlaceToLower(id);
    //OutputDebugString(id.c_str());

    std::wstring pingBiDiId{ DEFAULT_PING_BIDI_ID };
    InPlaceToLower(pingBiDiId);
    //OutputDebugString(pingBiDiId.c_str());

    std::wstring loopBiDiAId{ DEFAULT_LOOPBACK_BIDI_A_ID };
    InPlaceToLower(loopBiDiAId);
    //OutputDebugString(loopBiDiAId.c_str());


    std::wstring loopBiDiBId{ DEFAULT_LOOPBACK_BIDI_B_ID };
    InPlaceToLower(loopBiDiBId);
    //OutputDebugString(loopBiDiBId.c_str());

    
    // Both loopback endpoints share the same internal device as a simple way of routing between the two. 
    // The Ping device is separate

    m_IsEndpointA = false;
    m_LoopbackMidiDevice = nullptr;
    m_PingMidiDevice = nullptr;

    if (id.find(pingBiDiId) != std::wstring::npos)
    {
        // ping bidi endpoint

        m_IsPing = true;

        m_PingMidiDevice = (MidiPingBidiDevice*)(MidiDeviceTable::Current().GetPingDevice());
        m_PingMidiDevice->SetCallback(this, 0);
    }
    else if (id.find(loopBiDiAId) != std::wstring::npos)
    {
        // bidi endpoint A

        m_IsEndpointA = true;
        m_LoopbackMidiDevice = (MidiLoopbackBidiDevice*)(MidiDeviceTable::Current().GetBidiDevice());
        m_LoopbackMidiDevice->SetCallbackA(this, 0);
    }
    else if (id.find(loopBiDiBId) != std::wstring::npos)
    {
        // bidi endpoint B

        m_IsEndpointA = false;
        m_LoopbackMidiDevice = (MidiLoopbackBidiDevice*)(MidiDeviceTable::Current().GetBidiDevice());
        m_LoopbackMidiDevice->SetCallbackB(this, 0);
    }
    else
    {
        return E_FAIL;
    }

    //RETURN_HR_IF_NULL(E_POINTER, m_MidiDevice);

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

    if (m_LoopbackMidiDevice != nullptr && m_IsEndpointA)
    {
        m_LoopbackMidiDevice->CleanupA();
        m_LoopbackMidiDevice = nullptr;
    }
    else if (m_LoopbackMidiDevice != nullptr && !m_IsEndpointA)
    {
        m_LoopbackMidiDevice->CleanupB();
        m_LoopbackMidiDevice = nullptr;
    }
    else if (m_PingMidiDevice != nullptr && m_IsPing)
    {
        m_PingMidiDevice->Cleanup();
        m_PingMidiDevice = nullptr;
    }

    m_Callback = nullptr;
    m_Context = 0;

    return S_OK;
}

_Use_decl_annotations_
HRESULT
CMidi2LoopbackMidiBiDi::SendMidiMessage(
    PVOID Message,
    UINT Size,
    LONGLONG Timestamp
)
{
    RETURN_HR_IF_NULL(E_INVALIDARG, Message);
    RETURN_HR_IF(E_INVALIDARG, Size < sizeof(uint32_t));
    
    if (m_IsPing)
    {
        RETURN_HR_IF_NULL(E_POINTER, m_PingMidiDevice);
        return m_PingMidiDevice->SendMidiMessage(Message, Size, Timestamp);
    }
    else if (m_IsEndpointA)
    {
        RETURN_HR_IF_NULL(E_POINTER, m_LoopbackMidiDevice);
        return m_LoopbackMidiDevice->SendMidiMessageFromAToB(Message, Size, Timestamp);
    }
    else
    {
        RETURN_HR_IF_NULL(E_POINTER, m_LoopbackMidiDevice);
        return m_LoopbackMidiDevice->SendMidiMessageFromBToA(Message, Size, Timestamp);
    }
}

_Use_decl_annotations_
HRESULT
CMidi2LoopbackMidiBiDi::Callback(
    PVOID Message,
    UINT Size,
    LONGLONG Timestamp,
    LONGLONG
)
{
    RETURN_HR_IF_NULL(E_INVALIDARG, Message);
    RETURN_HR_IF_NULL(E_POINTER, m_Callback);
    RETURN_HR_IF(E_INVALIDARG, Size < sizeof(uint32_t));

    return m_Callback->Callback(Message, Size, Timestamp, m_Context);
}

