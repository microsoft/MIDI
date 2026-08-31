// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services WinRT API and should be used
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


// Helper macros for logging exceptions from within [noexcept] members. These expand to the
// same TraceLogging error pattern used throughout the SDK. Use MIDI_SDK_TRACE_THIS_POINTER
// for instance methods and nullptr for static methods.
//
// Example:
//   catch (winrt::hresult_error const& ex)
//   {
//       MIDI_SDK_LOG_HRESULT_EXCEPTION(this, ex, L"hresult error doing the thing.");
//       return <default>;
//   }
//   catch (...)
//   {
//       MIDI_SDK_LOG_GENERAL_EXCEPTION(this, L"General exception doing the thing.");
//       return <default>;
//   }

#define MIDI_SDK_LOG_HRESULT_EXCEPTION(thisPointer, ex, messageText)                             \
    LOG_IF_FAILED(static_cast<HRESULT>((ex).code()));                                            \
    TraceLoggingWrite(                                                                           \
        Midi2SdkTelemetryProvider::Provider(),                                                  \
        MIDI_SDK_TRACE_EVENT_ERROR,                                                              \
        TraceLoggingString(__FUNCTION__, MIDI_SDK_TRACE_LOCATION_FIELD),                         \
        TraceLoggingLevel(WINEVENT_LEVEL_ERROR),                                                 \
        TraceLoggingPointer((thisPointer), MIDI_SDK_TRACE_THIS_FIELD),                           \
        TraceLoggingWideString((messageText), MIDI_SDK_TRACE_MESSAGE_FIELD),                     \
        TraceLoggingHResult(static_cast<HRESULT>((ex).code()), MIDI_SDK_TRACE_HRESULT_FIELD),    \
        TraceLoggingWideString((ex).message().c_str(), MIDI_SDK_TRACE_ERROR_FIELD)               \
    )

#define MIDI_SDK_LOG_GENERAL_EXCEPTION(thisPointer, messageText)                                 \
    LOG_IF_FAILED(E_FAIL);                                                                       \
    TraceLoggingWrite(                                                                           \
        Midi2SdkTelemetryProvider::Provider(),                                                  \
        MIDI_SDK_TRACE_EVENT_ERROR,                                                              \
        TraceLoggingString(__FUNCTION__, MIDI_SDK_TRACE_LOCATION_FIELD),                         \
        TraceLoggingLevel(WINEVENT_LEVEL_ERROR),                                                 \
        TraceLoggingPointer((thisPointer), MIDI_SDK_TRACE_THIS_FIELD),                           \
        TraceLoggingWideString((messageText), MIDI_SDK_TRACE_MESSAGE_FIELD)                      \
    )