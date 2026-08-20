// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

class Midi2MonitorTelemetryProvider : public wil::TraceLoggingProvider
{
    IMPLEMENT_TRACELOGGING_CLASS_WITH_MICROSOFT_TELEMETRY(
        Midi2MonitorTelemetryProvider,
        "Microsoft.Windows.Midi2.Monitor",
        //  PS> [System.Diagnostics.Tracing.EventSource]::new("Microsoft.Windows.Midi2.Monitor").Guid
        // {fd60bb52-ed89-5529-9203-76f87e574739}
        (0xfd60bb52, 0xed89, 0x5529, 0x92, 0x03, 0x76, 0xf8, 0x7e, 0x57, 0x47, 0x39))
};

#define MIDI_MONITOR_TRACE_EVENT_ERROR                  "MidiMonitor.Error"
#define MIDI_MONITOR_TRACE_EVENT_WARNING                "MidiMonitor.Warning"
#define MIDI_MONITOR_TRACE_EVENT_INFO                   "MidiMonitor.Info"

#define MIDI_MONITOR_TRACE_LOCATION_FIELD               "location"
#define MIDI_MONITOR_TRACE_MESSAGE_FIELD                "message"
#define MIDI_MONITOR_TRACE_HRESULT_FIELD                "hresult"
#define MIDI_MONITOR_TRACE_ERROR_FIELD                  "error"
#define MIDI_MONITOR_TRACE_ENDPOINT_DEVICE_ID_FIELD     "endpoint id"

#define MIDI_MONITOR_LOG_INFO(messageText)                                                       \
    TraceLoggingWrite(                                                                           \
        Midi2MonitorTelemetryProvider::Provider(),                                               \
        MIDI_MONITOR_TRACE_EVENT_INFO,                                                           \
        TraceLoggingString(__FUNCTION__, MIDI_MONITOR_TRACE_LOCATION_FIELD),                     \
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),                                                  \
        TraceLoggingWideString((messageText), MIDI_MONITOR_TRACE_MESSAGE_FIELD)                  \
    )

#define MIDI_MONITOR_LOG_INFO_WITH_ENDPOINT(messageText, endpointId)                             \
    TraceLoggingWrite(                                                                           \
        Midi2MonitorTelemetryProvider::Provider(),                                               \
        MIDI_MONITOR_TRACE_EVENT_INFO,                                                           \
        TraceLoggingString(__FUNCTION__, MIDI_MONITOR_TRACE_LOCATION_FIELD),                     \
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),                                                  \
        TraceLoggingWideString((messageText), MIDI_MONITOR_TRACE_MESSAGE_FIELD),                 \
        TraceLoggingWideString((endpointId), MIDI_MONITOR_TRACE_ENDPOINT_DEVICE_ID_FIELD)        \
    )

#define MIDI_MONITOR_LOG_HRESULT_EXCEPTION(ex, messageText)                                      \
    LOG_IF_FAILED(static_cast<HRESULT>((ex).code()));                                            \
    TraceLoggingWrite(                                                                           \
        Midi2MonitorTelemetryProvider::Provider(),                                               \
        MIDI_MONITOR_TRACE_EVENT_ERROR,                                                          \
        TraceLoggingString(__FUNCTION__, MIDI_MONITOR_TRACE_LOCATION_FIELD),                     \
        TraceLoggingLevel(WINEVENT_LEVEL_ERROR),                                                 \
        TraceLoggingWideString((messageText), MIDI_MONITOR_TRACE_MESSAGE_FIELD),                 \
        TraceLoggingHResult(static_cast<HRESULT>((ex).code()), MIDI_MONITOR_TRACE_HRESULT_FIELD),\
        TraceLoggingWideString((ex).message().c_str(), MIDI_MONITOR_TRACE_ERROR_FIELD)           \
    )

#define MIDI_MONITOR_LOG_GENERAL_EXCEPTION(messageText)                                          \
    LOG_IF_FAILED(E_FAIL);                                                                       \
    TraceLoggingWrite(                                                                           \
        Midi2MonitorTelemetryProvider::Provider(),                                               \
        MIDI_MONITOR_TRACE_EVENT_ERROR,                                                          \
        TraceLoggingString(__FUNCTION__, MIDI_MONITOR_TRACE_LOCATION_FIELD),                     \
        TraceLoggingLevel(WINEVENT_LEVEL_ERROR),                                                 \
        TraceLoggingWideString((messageText), MIDI_MONITOR_TRACE_MESSAGE_FIELD)                  \
    )

// Every public entry point that can be reached from XAML or from a MIDI callback is wrapped
// so that an escaping exception can never terminate the process.
#define MIDI_MONITOR_CATCH_AND_LOG(messageText)                                                  \
    catch (winrt::hresult_error const& ex)                                                       \
    {                                                                                            \
        MIDI_MONITOR_LOG_HRESULT_EXCEPTION(ex, messageText);                                     \
    }                                                                                            \
    catch (...)                                                                                  \
    {                                                                                            \
        MIDI_MONITOR_LOG_GENERAL_EXCEPTION(messageText);                                         \
    }
