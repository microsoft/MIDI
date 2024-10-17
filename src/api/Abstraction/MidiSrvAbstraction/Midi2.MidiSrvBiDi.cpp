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
    LPCWSTR device,
    PTRANSPORTCREATIONPARAMS creationParams,
    DWORD * mmcssTaskId,
    IMidiCallback * callback,
    LONGLONG context,
    GUID sessionId
)
{
    RETURN_HR_IF(E_INVALIDARG, nullptr == callback);
    RETURN_HR_IF(E_INVALIDARG, nullptr == device);
    RETURN_HR_IF(E_INVALIDARG, nullptr == mmcssTaskId);
    RETURN_HR_IF(E_INVALIDARG, nullptr == creationParams);

    TraceLoggingWrite(
        MidiSrvAbstractionTelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_INFO,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this")
        );


    std::unique_ptr<CMidi2MidiSrv> midiSrv(new (std::nothrow) CMidi2MidiSrv());
    RETURN_IF_NULL_ALLOC(midiSrv);

    RETURN_IF_FAILED(midiSrv->Initialize(device, MidiFlowBidirectional, creationParams, mmcssTaskId, callback, context, sessionId));
    m_MidiSrv = std::move(midiSrv);

    return S_OK;
}

HRESULT
CMidi2MidiSrvBiDi::Shutdown()
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
        RETURN_IF_FAILED(m_MidiSrv->Shutdown());
        m_MidiSrv.reset();
    }

    return S_OK;
}

_Use_decl_annotations_
HRESULT
CMidi2MidiSrvBiDi::SendMidiMessage(
    PVOID data,
    UINT length,
    LONGLONG position
)
{
    if (m_MidiSrv)
    {
        //return m_MidiSrv->SendMidiMessage(data, length, position);

        auto hr = m_MidiSrv->SendMidiMessage(data, length, position);
        LOG_IF_FAILED(hr);

        return hr;

    }

    return E_ABORT;
}

