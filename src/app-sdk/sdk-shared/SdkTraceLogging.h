// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://github.com/microsoft/MIDI/
// ============================================================================


class Midi2SdkTelemetryProvider : public wil::TraceLoggingProvider
{
    IMPLEMENT_TRACELOGGING_CLASS_WITH_MICROSOFT_TELEMETRY(
        Midi2SdkTelemetryProvider,
        "Microsoft.Windows.Midi2.Sdk",
        //  PS> [System.Diagnostics.Tracing.EventSource]::new("Microsoft.Windows.Midi2.Sdk").Guid
        // {d7c0ce6e-6762-5795-cc08-ab155270fc72}
        (0xd7c0ce6e, 0x6762, 0x5795, 0xcc, 0x08, 0xab, 0x15, 0x52, 0x70, 0xfc, 0x72))
};


#define MIDI_TRACE_LOGGING_LOCATION_FIELD               "location"
#define MIDI_TRACE_LOGGING_MESSAGE_FIELD                "message"
#define MIDI_TRACE_LOGGING_ENDPOINT_DEVICE_ID_FIELD     "endpoint id"
#define MIDI_TRACE_LOGGING_HRESULT_FIELD                "hresult"
#define MIDI_TRACE_LOGGING_ERROR_FIELD                  "error"
#define MIDI_TRACE_LOGGING_MESSAGE_TIMESTAMP_FIELD      "message timestamp"
#define MIDI_TRACE_LOGGING_MESSAGE_WORD0_FIELD          "ump word0"