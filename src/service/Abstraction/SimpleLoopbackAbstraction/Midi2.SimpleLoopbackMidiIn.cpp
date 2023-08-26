// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://github.com/microsoft/MIDI/
// ============================================================================


#include "pch.h"
#include "midi2.SimpleLoopbackabstraction.h"

_Use_decl_annotations_
HRESULT
CMidi2SimpleLoopbackMidiIn::Initialize(
    LPCWSTR,
    DWORD *,
    IMidiCallback *
)
{
    TraceLoggingWrite(
        MidiSimpleLoopbackAbstractionTelemetryProvider::Provider(),
        __FUNCTION__,
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this")
        );

    return E_NOTIMPL;
}

HRESULT
CMidi2SimpleLoopbackMidiIn::Cleanup()
{
    TraceLoggingWrite(
        MidiSimpleLoopbackAbstractionTelemetryProvider::Provider(),
        __FUNCTION__,
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this")
        );

    return E_NOTIMPL;
}

_Use_decl_annotations_
HRESULT
CMidi2SimpleLoopbackMidiIn::Callback(
    PVOID,
    UINT,
    LONGLONG
)
{
    return E_NOTIMPL;
}

