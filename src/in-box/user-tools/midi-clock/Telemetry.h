// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

class MidiClockTelemetryProvider : public wil::TraceLoggingProvider
{
    IMPLEMENT_TRACELOGGING_CLASS_WITH_MICROSOFT_TELEMETRY(
        MidiClockTelemetryProvider,
        "Microsoft.Windows.Midi2.Clock",
        //  PS> [System.Diagnostics.Tracing.EventSource]::new("Microsoft.Windows.Midi2.Clock").Guid
        // {b66ed50c-069b-55bb-3c25-e6a2d8583a2c}
        (0xb66ed50c, 0x069b, 0x55bb, 0x3c, 0x25, 0xe6, 0xa2, 0xd8, 0x58, 0x3a, 0x2c))
};

#define MIDI_CLOCK_TRACE_EVENT_ERROR                  "MidiClock.Error"
#define MIDI_CLOCK_TRACE_EVENT_WARNING                "MidiClock.Warning"
#define MIDI_CLOCK_TRACE_EVENT_INFO                   "MidiClock.Info"

#define MIDI_CLOCK_TRACE_LOCATION_FIELD               "location"
#define MIDI_CLOCK_TRACE_MESSAGE_FIELD                "message"
#define MIDI_CLOCK_TRACE_HRESULT_FIELD                "hresult"
#define MIDI_CLOCK_TRACE_ERROR_FIELD                  "error"
#define MIDI_CLOCK_TRACE_ENDPOINT_DEVICE_ID_FIELD     "endpoint id"

#define MIDI_CLOCK_LOG_INFO(messageText)                                                         \
    TraceLoggingWrite(                                                                           \
        MidiClockTelemetryProvider::Provider(),                                                  \
        MIDI_CLOCK_TRACE_EVENT_INFO,                                                             \
        TraceLoggingString(__FUNCTION__, MIDI_CLOCK_TRACE_LOCATION_FIELD),                       \
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),                                                  \
        TraceLoggingWideString((messageText), MIDI_CLOCK_TRACE_MESSAGE_FIELD)                    \
    )

#define MIDI_CLOCK_LOG_INFO_WITH_ENDPOINT(messageText, endpointId)                               \
    TraceLoggingWrite(                                                                           \
        MidiClockTelemetryProvider::Provider(),                                                  \
        MIDI_CLOCK_TRACE_EVENT_INFO,                                                             \
        TraceLoggingString(__FUNCTION__, MIDI_CLOCK_TRACE_LOCATION_FIELD),                       \
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),                                                  \
        TraceLoggingWideString((messageText), MIDI_CLOCK_TRACE_MESSAGE_FIELD),                   \
        TraceLoggingWideString((endpointId), MIDI_CLOCK_TRACE_ENDPOINT_DEVICE_ID_FIELD)          \
    )

#define MIDI_CLOCK_LOG_HRESULT_EXCEPTION(ex, messageText)                                        \
    LOG_IF_FAILED(static_cast<HRESULT>((ex).code()));                                            \
    TraceLoggingWrite(                                                                           \
        MidiClockTelemetryProvider::Provider(),                                                  \
        MIDI_CLOCK_TRACE_EVENT_ERROR,                                                            \
        TraceLoggingString(__FUNCTION__, MIDI_CLOCK_TRACE_LOCATION_FIELD),                       \
        TraceLoggingLevel(WINEVENT_LEVEL_ERROR),                                                 \
        TraceLoggingWideString((messageText), MIDI_CLOCK_TRACE_MESSAGE_FIELD),                   \
        TraceLoggingHResult(static_cast<HRESULT>((ex).code()), MIDI_CLOCK_TRACE_HRESULT_FIELD),  \
        TraceLoggingWideString((ex).message().c_str(), MIDI_CLOCK_TRACE_ERROR_FIELD)             \
    )

#define MIDI_CLOCK_LOG_GENERAL_EXCEPTION(messageText)                                            \
    LOG_IF_FAILED(E_FAIL);                                                                       \
    TraceLoggingWrite(                                                                           \
        MidiClockTelemetryProvider::Provider(),                                                  \
        MIDI_CLOCK_TRACE_EVENT_ERROR,                                                            \
        TraceLoggingString(__FUNCTION__, MIDI_CLOCK_TRACE_LOCATION_FIELD),                       \
        TraceLoggingLevel(WINEVENT_LEVEL_ERROR),                                                 \
        TraceLoggingWideString((messageText), MIDI_CLOCK_TRACE_MESSAGE_FIELD)                    \
    )

// Every public entry point that can be reached from XAML, from a timer, or from the clock
// worker threads is wrapped so that an escaping exception can never terminate the process.
#define MIDI_CLOCK_CATCH_AND_LOG(messageText)                                                    \
    catch (winrt::hresult_error const& ex)                                                       \
    {                                                                                            \
        MIDI_CLOCK_LOG_HRESULT_EXCEPTION(ex, messageText);                                       \
    }                                                                                            \
    catch (...)                                                                                  \
    {                                                                                            \
        MIDI_CLOCK_LOG_GENERAL_EXCEPTION(messageText);                                           \
    }
