// Copyright (c) Microsoft Corporation. All rights reserved.

#include "pch.h"

_Use_decl_annotations_
HRESULT
CMidi2MidiSrvOut::Initialize(
    LPCWSTR Device,
    DWORD * MmcssTaskId
)
{
    TraceLoggingWrite(
        MidiSrvAbstractionTelemetryProvider::Provider(),
        __FUNCTION__,
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this")
        );

    std::unique_ptr<CMidi2MidiSrv> midiSrv(new (std::nothrow) CMidi2MidiSrv());
    RETURN_IF_NULL_ALLOC(midiSrv);

    RETURN_IF_FAILED(midiSrv->Initialize(Device, MidiFlowOut, MmcssTaskId, nullptr));
    m_MidiSrv = std::move(midiSrv);

    return S_OK;
}

HRESULT
CMidi2MidiSrvOut::Cleanup()
{
    TraceLoggingWrite(
        MidiSrvAbstractionTelemetryProvider::Provider(),
        __FUNCTION__,
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
CMidi2MidiSrvOut::SendMidiMessage(
    PVOID Data,
    UINT Length,
    LONGLONG Position
)
{
    if (m_MidiSrv)
    {
        return m_MidiSrv->SendMidiMessage(Data, Length, Position);
    }

    return E_ABORT;
}

