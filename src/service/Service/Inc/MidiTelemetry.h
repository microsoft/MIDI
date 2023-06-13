// Copyright (c) Microsoft Corporation. All rights reserved.
#pragma once

class MidiSrvTelemetryProvider : public wil::TraceLoggingProvider
{
    IMPLEMENT_TRACELOGGING_CLASS_WITH_MICROSOFT_TELEMETRY(
        MidiSrvTelemetryProvider,
        "Microsoft.Windows.Midi2.MidiSrv",
        // {f42d2441-aac3-5216-0150-3c0f50006b64}
        (0xf42d2441,0xaac3,0x5216,0x01,0x50,0x3c,0x0f,0x50,0x00,0x6b,0x64));

public:
};

