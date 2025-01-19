// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

#include "stdafx.h"

void CMidiSrvTraceLogger::LogMidi2CreateClient(
    HRESULT hr, LPCWSTR DeviceInstanceId, LPCWSTR ProcessName, MidiApi Api, MidiFlow Flow, MidiDataFormats Format, DWORD ClientProcessId)
{
    TraceLoggingWrite(
        MidiSrvTelemetryProvider::Provider(),
        "Midi2CreateClient",
        TraceLoggingKeyword(MICROSOFT_KEYWORD_MEASURES),
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingHResult(hr, "HResult"),
        TraceLoggingValue(DeviceInstanceId, "DeviceInstanceId"),
        TraceLoggingValue(ProcessName, "ProcessName"),
        TraceLoggingUInt8((static_cast<UINT8>(Api)), "CallingApi"),
        TraceLoggingUInt8((static_cast<UINT8>(Flow)), "Flow"),
        TraceLoggingUInt8((static_cast<UINT8>(Format)), "DataFormat"),
        TraceLoggingValue(ClientProcessId, "ClientProcessId")
        );
}

void CMidiSrvTraceLogger::LogMidi2DestroyClient(HRESULT hr)
{
    TraceLoggingWrite(
        MidiSrvTelemetryProvider::Provider(),
        "Midi2DestroyClient",
        TraceLoggingKeyword(MICROSOFT_KEYWORD_MEASURES),
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingHResult(hr, "HResult"));
}
