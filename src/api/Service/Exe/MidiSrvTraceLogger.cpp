// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

#include "stdafx.h"

_Use_decl_annotations_
void CMidiSrvTraceLogger::LogMidi2CreateClient(
    HRESULT hr, LPCWSTR deviceInstanceId, LPCWSTR processName, GUID api, MidiFlow flow, MidiDataFormats format, DWORD clientProcessId)
{
    TraceLoggingWrite(
        MidiSrvTelemetryProvider::Provider(),
        "Midi2CreateClient",
        TraceLoggingKeyword(MICROSOFT_KEYWORD_MEASURES),
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingHResult(hr, "HResult"),
        TraceLoggingValue(deviceInstanceId, "DeviceInstanceId"),
        TraceLoggingValue(processName, "ProcessName"),
        TraceLoggingGuid(api, "CallingApi"),
        TraceLoggingUInt8((static_cast<UINT8>(flow)), "Flow"),
        TraceLoggingUInt8((static_cast<UINT8>(format)), "DataFormat"),
        TraceLoggingValue(clientProcessId, "ClientProcessId")
        );
}

_Use_decl_annotations_
void CMidiSrvTraceLogger::LogMidi2DestroyClient(HRESULT hr)
{
    TraceLoggingWrite(
        MidiSrvTelemetryProvider::Provider(),
        "Midi2DestroyClient",
        TraceLoggingKeyword(MICROSOFT_KEYWORD_MEASURES),
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingHResult(hr, "HResult"));
}
