// Copyright (c) Microsoft Corporation. All rights reserved.

#include "pch.h"

_Use_decl_annotations_
HRESULT
CMidi2MidiSrvBiDi::Initialize(
    LPCWSTR,
    DWORD *,
    IMidiCallback *
)
{

    TraceLoggingWrite(
        MidiSrvAbstractionTelemetryProvider::Provider(),
        __FUNCTION__,
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this")
        );

    return E_NOTIMPL;
}

HRESULT
CMidi2MidiSrvBiDi::Cleanup()
{
    TraceLoggingWrite(
        MidiSrvAbstractionTelemetryProvider::Provider(),
        __FUNCTION__,
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this")
        );

    return E_NOTIMPL;
}

_Use_decl_annotations_
HRESULT
CMidi2MidiSrvBiDi::SendMidiMessage(
    PVOID,
    UINT,
    LONGLONG
)
{
    return E_NOTIMPL;
}

_Use_decl_annotations_
HRESULT
CMidi2MidiSrvBiDi::Callback(
    PVOID,
    UINT,
    LONGLONG
)
{
    return E_NOTIMPL;
}

