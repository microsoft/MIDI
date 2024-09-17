// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================


#include "pch.h"

_Use_decl_annotations_
HRESULT
CMidi2MidiSrvBiDi::Initialize(
    LPCWSTR Device,
    PABSTRACTIONCREATIONPARAMS CreationParams,
    DWORD * MmcssTaskId,
    IMidiCallback * Callback,
    LONGLONG Context,
    GUID SessionId
)
{
    RETURN_HR_IF(E_INVALIDARG, nullptr == Callback);
    RETURN_HR_IF(E_INVALIDARG, nullptr == Device);
    RETURN_HR_IF(E_INVALIDARG, nullptr == MmcssTaskId);
    RETURN_HR_IF(E_INVALIDARG, nullptr == CreationParams);

    TraceLoggingWrite(
        MidiSrvAbstractionTelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_INFO,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this")
        );


    std::unique_ptr<CMidi2MidiSrv> midiSrv(new (std::nothrow) CMidi2MidiSrv());
    RETURN_IF_NULL_ALLOC(midiSrv);

    RETURN_IF_FAILED(midiSrv->Initialize(Device, MidiFlowBidirectional, CreationParams, MmcssTaskId, Callback, Context, SessionId));
    m_MidiSrv = std::move(midiSrv);

    return S_OK;
}

HRESULT
CMidi2MidiSrvBiDi::Cleanup()
{
    TraceLoggingWrite(
        MidiSrvAbstractionTelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_INFO,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this")
        );

    if (m_MidiSrv)
    {
        RETURN_IF_FAILED(m_MidiSrv->Cleanup());
        m_MidiSrv.reset();
    }

    return S_OK;
}

_Use_decl_annotations_
HRESULT
CMidi2MidiSrvBiDi::SendMidiMessage(
    PVOID Data,
    UINT Length,
    LONGLONG Position
)
{
    if (m_MidiSrv)
    {
        //return m_MidiSrv->SendMidiMessage(Data, Length, Position);

        auto hr = m_MidiSrv->SendMidiMessage(Data, Length, Position);
        LOG_IF_FAILED(hr);

        return hr;

    }

    return E_ABORT;
}

