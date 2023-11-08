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
CMidi2SchedulerMidiBiDi::Initialize(
    LPCWSTR endpointId,
    DWORD *,
    IMidiCallback * callback
)
{
    TraceLoggingWrite(
        MidiSchedulerAbstractionTelemetryProvider::Provider(),
        __FUNCTION__,
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this")
        );

    RETURN_HR_IF_NULL(E_INVALIDARG, callback);
    m_callback = callback;


    std::wstring id{ endpointId };
    InPlaceToLower(id);



    return S_OK;
}

HRESULT
CMidi2SchedulerMidiBiDi::Cleanup()
{
    TraceLoggingWrite(
        MidiSchedulerAbstractionTelemetryProvider::Provider(),
        __FUNCTION__,
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this")
        );


    m_callback = nullptr;

    return S_OK;
}

_Use_decl_annotations_
HRESULT
CMidi2SchedulerMidiBiDi::SendMidiMessage(
    PVOID message,
    UINT size,
    LONGLONG timestamp
)
{
    RETURN_HR_IF_NULL(E_INVALIDARG, message);
    RETURN_HR_IF(E_INVALIDARG, size < sizeof(uint32_t));
    

    // TODO
    UNREFERENCED_PARAMETER(timestamp);


    return S_OK;
}

_Use_decl_annotations_
HRESULT
CMidi2SchedulerMidiBiDi::Callback(
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

