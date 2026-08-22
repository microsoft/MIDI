// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

class MidiSysExToolTelemetryProvider : public wil::TraceLoggingProvider
{
    IMPLEMENT_TRACELOGGING_CLASS_WITH_MICROSOFT_TELEMETRY(
        MidiSysExToolTelemetryProvider,
        "Microsoft.Windows.Midi2.SysExTool",
        //  PS> [System.Diagnostics.Tracing.EventSource]::new("Microsoft.Windows.Midi2.SysExTool").Guid
        // {cebee286-ab08-5aee-b8a4-5183ee4ed779}
        (0xcebee286, 0xab08, 0x5aee, 0xb8, 0xa4, 0x51, 0x83, 0xee, 0x4e, 0xd7, 0x79))
};

#define MIDI_SYSEXTOOL_TRACE_EVENT_ERROR                  "MidiSysExTool.Error"
#define MIDI_SYSEXTOOL_TRACE_EVENT_WARNING                "MidiSysExTool.Warning"
#define MIDI_SYSEXTOOL_TRACE_EVENT_INFO                   "MidiSysExTool.Info"

#define MIDI_SYSEXTOOL_TRACE_LOCATION_FIELD               "location"
#define MIDI_SYSEXTOOL_TRACE_MESSAGE_FIELD                "message"
#define MIDI_SYSEXTOOL_TRACE_HRESULT_FIELD                "hresult"
#define MIDI_SYSEXTOOL_TRACE_ERROR_FIELD                  "error"
#define MIDI_SYSEXTOOL_TRACE_ENDPOINT_DEVICE_ID_FIELD     "endpoint id"

#define MIDI_SYSEXTOOL_LOG_INFO(messageText)                                                       \
    TraceLoggingWrite(                                                                           \
        MidiSysExToolTelemetryProvider::Provider(),                                               \
        MIDI_SYSEXTOOL_TRACE_EVENT_INFO,                                                           \
        TraceLoggingString(__FUNCTION__, MIDI_SYSEXTOOL_TRACE_LOCATION_FIELD),                     \
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),                                                  \
        TraceLoggingWideString((messageText), MIDI_SYSEXTOOL_TRACE_MESSAGE_FIELD)                  \
    )

#define MIDI_SYSEXTOOL_LOG_INFO_WITH_ENDPOINT(messageText, endpointId)                             \
    TraceLoggingWrite(                                                                           \
        MidiSysExToolTelemetryProvider::Provider(),                                               \
        MIDI_SYSEXTOOL_TRACE_EVENT_INFO,                                                           \
        TraceLoggingString(__FUNCTION__, MIDI_SYSEXTOOL_TRACE_LOCATION_FIELD),                     \
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),                                                  \
        TraceLoggingWideString((messageText), MIDI_SYSEXTOOL_TRACE_MESSAGE_FIELD),                 \
        TraceLoggingWideString((endpointId), MIDI_SYSEXTOOL_TRACE_ENDPOINT_DEVICE_ID_FIELD)        \
    )

#define MIDI_SYSEXTOOL_LOG_HRESULT_EXCEPTION(ex, messageText)                                      \
    LOG_IF_FAILED(static_cast<HRESULT>((ex).code()));                                            \
    TraceLoggingWrite(                                                                           \
        MidiSysExToolTelemetryProvider::Provider(),                                               \
        MIDI_SYSEXTOOL_TRACE_EVENT_ERROR,                                                          \
        TraceLoggingString(__FUNCTION__, MIDI_SYSEXTOOL_TRACE_LOCATION_FIELD),                     \
        TraceLoggingLevel(WINEVENT_LEVEL_ERROR),                                                 \
        TraceLoggingWideString((messageText), MIDI_SYSEXTOOL_TRACE_MESSAGE_FIELD),                 \
        TraceLoggingHResult(static_cast<HRESULT>((ex).code()), MIDI_SYSEXTOOL_TRACE_HRESULT_FIELD),\
        TraceLoggingWideString((ex).message().c_str(), MIDI_SYSEXTOOL_TRACE_ERROR_FIELD)           \
    )

#define MIDI_SYSEXTOOL_LOG_GENERAL_EXCEPTION(messageText)                                          \
    LOG_IF_FAILED(E_FAIL);                                                                       \
    TraceLoggingWrite(                                                                           \
        MidiSysExToolTelemetryProvider::Provider(),                                               \
        MIDI_SYSEXTOOL_TRACE_EVENT_ERROR,                                                          \
        TraceLoggingString(__FUNCTION__, MIDI_SYSEXTOOL_TRACE_LOCATION_FIELD),                     \
        TraceLoggingLevel(WINEVENT_LEVEL_ERROR),                                                 \
        TraceLoggingWideString((messageText), MIDI_SYSEXTOOL_TRACE_MESSAGE_FIELD)                  \
    )

// Every public entry point that can be reached from XAML or from a MIDI callback is wrapped
// so that an escaping exception can never terminate the process.
#define MIDI_SYSEXTOOL_CATCH_AND_LOG(messageText)                                                  \
    catch (winrt::hresult_error const& ex)                                                       \
    {                                                                                            \
        MIDI_SYSEXTOOL_LOG_HRESULT_EXCEPTION(ex, messageText);                                     \
    }                                                                                            \
    catch (...)                                                                                  \
    {                                                                                            \
        MIDI_SYSEXTOOL_LOG_GENERAL_EXCEPTION(messageText);                                         \
    }
