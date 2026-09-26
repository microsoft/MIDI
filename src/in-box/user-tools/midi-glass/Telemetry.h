// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

class MidiGlassTelemetryProvider : public wil::TraceLoggingProvider
{
    IMPLEMENT_TRACELOGGING_CLASS_WITH_MICROSOFT_TELEMETRY(
        MidiGlassTelemetryProvider,
        "Microsoft.Windows.Midi2.Glass",
        //  PS> [System.Diagnostics.Tracing.EventSource]::new("Microsoft.Windows.Midi2.Glass").Guid
        // {89ed34c8-8095-561e-2b3e-233b39f39631}
        (0x89ed34c8, 0x8095, 0x561e, 0x2b, 0x3e, 0x23, 0x3b, 0x39, 0xf3, 0x96, 0x31))
};

#define MIDI_GLASS_TRACE_EVENT_ERROR                  "MidiGlass.Error"
#define MIDI_GLASS_TRACE_EVENT_WARNING                "MidiGlass.Warning"
#define MIDI_GLASS_TRACE_EVENT_INFO                   "MidiGlass.Info"

#define MIDI_GLASS_TRACE_LOCATION_FIELD               "location"
#define MIDI_GLASS_TRACE_MESSAGE_FIELD                "message"
#define MIDI_GLASS_TRACE_HRESULT_FIELD                "hresult"
#define MIDI_GLASS_TRACE_ERROR_FIELD                  "error"
#define MIDI_GLASS_TRACE_ENDPOINT_DEVICE_ID_FIELD     "endpoint id"

#define MIDI_GLASS_LOG_INFO(messageText)                                                       \
    TraceLoggingWrite(                                                                           \
        MidiGlassTelemetryProvider::Provider(),                                               \
        MIDI_GLASS_TRACE_EVENT_INFO,                                                           \
        TraceLoggingString(__FUNCTION__, MIDI_GLASS_TRACE_LOCATION_FIELD),                     \
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),                                                  \
        TraceLoggingWideString((messageText), MIDI_GLASS_TRACE_MESSAGE_FIELD)                  \
    )

#define MIDI_GLASS_LOG_INFO_WITH_ENDPOINT(messageText, endpointId)                             \
    TraceLoggingWrite(                                                                           \
        MidiGlassTelemetryProvider::Provider(),                                               \
        MIDI_GLASS_TRACE_EVENT_INFO,                                                           \
        TraceLoggingString(__FUNCTION__, MIDI_GLASS_TRACE_LOCATION_FIELD),                     \
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),                                                  \
        TraceLoggingWideString((messageText), MIDI_GLASS_TRACE_MESSAGE_FIELD),                 \
        TraceLoggingWideString((endpointId), MIDI_GLASS_TRACE_ENDPOINT_DEVICE_ID_FIELD)        \
    )

#define MIDI_GLASS_LOG_HRESULT_EXCEPTION(ex, messageText)                                      \
    LOG_IF_FAILED(static_cast<HRESULT>((ex).code()));                                            \
    TraceLoggingWrite(                                                                           \
        MidiGlassTelemetryProvider::Provider(),                                               \
        MIDI_GLASS_TRACE_EVENT_ERROR,                                                          \
        TraceLoggingString(__FUNCTION__, MIDI_GLASS_TRACE_LOCATION_FIELD),                     \
        TraceLoggingLevel(WINEVENT_LEVEL_ERROR),                                                 \
        TraceLoggingWideString((messageText), MIDI_GLASS_TRACE_MESSAGE_FIELD),                 \
        TraceLoggingHResult(static_cast<HRESULT>((ex).code()), MIDI_GLASS_TRACE_HRESULT_FIELD),\
        TraceLoggingWideString((ex).message().c_str(), MIDI_GLASS_TRACE_ERROR_FIELD)           \
    )

#define MIDI_GLASS_LOG_GENERAL_EXCEPTION(messageText)                                          \
    LOG_IF_FAILED(E_FAIL);                                                                       \
    TraceLoggingWrite(                                                                           \
        MidiGlassTelemetryProvider::Provider(),                                               \
        MIDI_GLASS_TRACE_EVENT_ERROR,                                                          \
        TraceLoggingString(__FUNCTION__, MIDI_GLASS_TRACE_LOCATION_FIELD),                     \
        TraceLoggingLevel(WINEVENT_LEVEL_ERROR),                                                 \
        TraceLoggingWideString((messageText), MIDI_GLASS_TRACE_MESSAGE_FIELD)                  \
    )

// Every public entry point that can be reached from XAML or from a MIDI callback is wrapped
// so that an escaping exception can never terminate the process.
#define MIDI_GLASS_CATCH_AND_LOG(messageText)                                                  \
    catch (winrt::hresult_error const& ex)                                                       \
    {                                                                                            \
        MIDI_GLASS_LOG_HRESULT_EXCEPTION(ex, messageText);                                     \
    }                                                                                            \
    catch (...)                                                                                  \
    {                                                                                            \
        MIDI_GLASS_LOG_GENERAL_EXCEPTION(messageText);                                         \
    }
