// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://github.com/microsoft/MIDI/
// ============================================================================


#include "pch.h"

_Use_decl_annotations_
HRESULT
CMidi2JitterReductionMidiBiDi::Initialize(
    LPCWSTR EndpointId,
    PABSTRACTIONCREATIONPARAMS,
    DWORD *,
    IMidiCallback * Callback,
    LONGLONG Context
)
{
    TraceLoggingWrite(
        MidiJitterReductionAbstractionTelemetryProvider::Provider(),
        __FUNCTION__,
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this")
        );

    RETURN_HR_IF_NULL(E_INVALIDARG, Callback);
    m_Callback = Callback;
    m_Context = Context;


    std::wstring id{ EndpointId };
    Windows::Devices::Midi2::Internal::InPlaceToLower(id);

    return S_OK;
}

HRESULT
CMidi2JitterReductionMidiBiDi::Cleanup()
{
    TraceLoggingWrite(
        MidiJitterReductionAbstractionTelemetryProvider::Provider(),
        __FUNCTION__,
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this")
        );


    m_Callback = nullptr;
    m_Context = 0;

    return S_OK;
}

_Use_decl_annotations_
HRESULT
CMidi2JitterReductionMidiBiDi::SendMidiMessage(
    PVOID Message,
    UINT Size,
    LONGLONG Timestamp
)
{
    RETURN_HR_IF_NULL(E_INVALIDARG, Message);
    RETURN_HR_IF(E_INVALIDARG, Size < sizeof(uint32_t));
    

    // TODO
    UNREFERENCED_PARAMETER(Timestamp);


    return S_OK;
}

_Use_decl_annotations_
HRESULT
CMidi2JitterReductionMidiBiDi::Callback(
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

