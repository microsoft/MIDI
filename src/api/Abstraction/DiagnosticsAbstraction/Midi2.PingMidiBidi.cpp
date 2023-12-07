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
CMidi2PingMidiBiDi::Initialize(
    LPCWSTR EndpointId,
    PABSTRACTIONCREATIONPARAMS CreationParams,
    DWORD*,
    IMidiCallback* Callback,
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

    // ping supports only UMP, reject requests for bytestream
    if (CreationParams->DataFormat != MidiDataFormat_UMP && 
        CreationParams->DataFormat != MidiDataFormat_Any)
    {
        RETURN_IF_FAILED(HRESULT_FROM_WIN32(ERROR_UNSUPPORTED_TYPE));
    }

    CreationParams->DataFormat = MidiDataFormat_UMP;
    m_Callback = Callback;
    m_Context = Context;

    // really should uppercase this

    std::wstring id{ EndpointId };

    m_MidiDevice = (MidiPingBidiDevice*)(MidiDeviceTable::Current().GetPingDevice());

    RETURN_HR_IF_NULL(E_POINTER, m_MidiDevice);

    m_MidiDevice->SetCallback(this, 0);


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


    if (m_MidiDevice != nullptr) m_MidiDevice->Cleanup();

    m_MidiDevice = nullptr;

    m_Callback = nullptr;
    m_Context = 0;

    return S_OK;
}

_Use_decl_annotations_
HRESULT
CMidi2PingMidiBiDi::SendMidiMessage(
    PVOID Message,
    UINT Size,
    LONGLONG Timestamp
)
{
    RETURN_HR_IF_NULL(E_INVALIDARG, Message);
    RETURN_HR_IF_NULL(E_POINTER, m_MidiDevice);
    RETURN_HR_IF(E_INVALIDARG, Size < sizeof(uint32_t));


    return m_MidiDevice->SendMidiMessage(Message, Size, Timestamp);
}

_Use_decl_annotations_
HRESULT
CMidi2PingMidiBiDi::Callback(
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

