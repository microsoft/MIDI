// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
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


#define MIDI_SDK_TRACE_EVENT_ERROR                      "MidiSdk.Error"
#define MIDI_SDK_TRACE_EVENT_WARNING                    "MidiSdk.Warning"
#define MIDI_SDK_TRACE_EVENT_INFO                       "MidiSdk.Info"


#define MIDI_SDK_TRACE_LOCATION_FIELD                   "location"
#define MIDI_SDK_TRACE_THIS_FIELD                       "this"
#define MIDI_SDK_TRACE_MESSAGE_FIELD                    "message"
#define MIDI_SDK_TRACE_ENDPOINT_DEVICE_ID_FIELD         "endpoint id"
#define MIDI_SDK_TRACE_HRESULT_FIELD                    "hresult"
#define MIDI_SDK_TRACE_ERROR_FIELD                      "error"
#define MIDI_SDK_TRACE_CONNECTION_ID_FIELD              "connection id"
#define MIDI_SDK_TRACE_SESSION_ID_FIELD                 "session id"
#define MIDI_SDK_TRACE_MESSAGE_SIZE_BYTES_FIELD         "message size bytes"
#define MIDI_SDK_TRACE_MESSAGE_SIZE_WORDS_FIELD         "message size words"
#define MIDI_SDK_TRACE_UMP_TYPE_FIELD                   "packet type"
#define MIDI_SDK_TRACE_PROPERTY_KEY_FIELD               "property key"

#define MIDI_SDK_STATIC_THIS_PLACEHOLDER_FIELD_VALUE    L"(static)"

#define MIDI_SDK_TRACE_MESSAGE_TIMESTAMP_FIELD          "message timestamp"
#define MIDI_SDK_TRACE_MESSAGE_WORD0_FIELD              "ump word0"