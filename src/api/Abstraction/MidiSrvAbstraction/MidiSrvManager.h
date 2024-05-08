// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://github.com/microsoft/MIDI/
// ============================================================================

#pragma once

// this is not used for any actual telemetry gathering. This is just
// to trigger service start when it is on-demand
// https://learn.microsoft.com/en-us/windows/win32/api/winsvc/ns-winsvc-service_trigger
class MidiSrvDemandStartEtwTrigger : public wil::TraceLoggingProvider
{
    IMPLEMENT_TRACELOGGING_CLASS_WITH_MICROSOFT_TELEMETRY(
        MidiSrvDemandStartEtwTrigger,
        "Microsoft.Windows.Midi2.MidiSrvTelemetryServiceDemandStartTrigger",
        // {c3263827-0519-1867-5309-51501984eded}
        (0xc3263827, 0x0519, 0x1867, 0x53, 0x09, 0x51, 0x50, 0x19, 0x84, 0xed, 0xed))
};


#define MIDI_SERVICE_NAME L"MidiSrv"


class MidiSrvManager
{
public:
    __success(return == true)
    static bool IsServiceInstalled();

    __success(return == true)
    static bool AttemptToStartService();
};




