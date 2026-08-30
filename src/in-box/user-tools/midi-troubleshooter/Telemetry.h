// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

class MidiTroubleshooterTelemetryProvider : public wil::TraceLoggingProvider
{
    IMPLEMENT_TRACELOGGING_CLASS_WITH_MICROSOFT_TELEMETRY(
        MidiTroubleshooterTelemetryProvider,
        "Microsoft.Windows.Midi2.Troubleshooter",
        //  PS> [System.Diagnostics.Tracing.EventSource]::new("Microsoft.Windows.Midi2.Troubleshooter").Guid
        // {3b9d5955-7963-50ee-a951-d1752f852cad}
        (0x3b9d5955, 0x7963, 0x50ee, 0xa9, 0x51, 0xd1, 0x75, 0x2f, 0x85, 0x2c, 0xad))
};

#define MIDI_TSHOOT_TRACE_EVENT_ERROR                   "MidiTroubleshooter.Error"
#define MIDI_TSHOOT_TRACE_EVENT_WARNING                 "MidiTroubleshooter.Warning"
#define MIDI_TSHOOT_TRACE_EVENT_INFO                    "MidiTroubleshooter.Info"

#define MIDI_TSHOOT_TRACE_LOCATION_FIELD                "location"
#define MIDI_TSHOOT_TRACE_MESSAGE_FIELD                 "message"
#define MIDI_TSHOOT_TRACE_HRESULT_FIELD                 "hresult"
#define MIDI_TSHOOT_TRACE_ERROR_FIELD                   "error"
#define MIDI_TSHOOT_TRACE_DETAIL_FIELD                  "detail"

#define MIDI_TSHOOT_LOG_INFO(messageText)                                                        \
    TraceLoggingWrite(                                                                           \
        MidiTroubleshooterTelemetryProvider::Provider(),                                         \
        MIDI_TSHOOT_TRACE_EVENT_INFO,                                                            \
        TraceLoggingString(__FUNCTION__, MIDI_TSHOOT_TRACE_LOCATION_FIELD),                      \
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),                                                  \
        TraceLoggingWideString((messageText), MIDI_TSHOOT_TRACE_MESSAGE_FIELD)                   \
    )

#define MIDI_TSHOOT_LOG_INFO_WITH_DETAIL(messageText, detailText)                                \
    TraceLoggingWrite(                                                                           \
        MidiTroubleshooterTelemetryProvider::Provider(),                                         \
        MIDI_TSHOOT_TRACE_EVENT_INFO,                                                            \
        TraceLoggingString(__FUNCTION__, MIDI_TSHOOT_TRACE_LOCATION_FIELD),                      \
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),                                                  \
        TraceLoggingWideString((messageText), MIDI_TSHOOT_TRACE_MESSAGE_FIELD),                  \
        TraceLoggingWideString((detailText), MIDI_TSHOOT_TRACE_DETAIL_FIELD)                     \
    )

#define MIDI_TSHOOT_LOG_WARNING(messageText)                                                     \
    TraceLoggingWrite(                                                                           \
        MidiTroubleshooterTelemetryProvider::Provider(),                                         \
        MIDI_TSHOOT_TRACE_EVENT_WARNING,                                                         \
        TraceLoggingString(__FUNCTION__, MIDI_TSHOOT_TRACE_LOCATION_FIELD),                      \
        TraceLoggingLevel(WINEVENT_LEVEL_WARNING),                                               \
        TraceLoggingWideString((messageText), MIDI_TSHOOT_TRACE_MESSAGE_FIELD)                   \
    )

#define MIDI_TSHOOT_LOG_HRESULT_EXCEPTION(ex, messageText)                                       \
    LOG_IF_FAILED(static_cast<HRESULT>((ex).code()));                                            \
    TraceLoggingWrite(                                                                           \
        MidiTroubleshooterTelemetryProvider::Provider(),                                         \
        MIDI_TSHOOT_TRACE_EVENT_ERROR,                                                           \
        TraceLoggingString(__FUNCTION__, MIDI_TSHOOT_TRACE_LOCATION_FIELD),                      \
        TraceLoggingLevel(WINEVENT_LEVEL_ERROR),                                                 \
        TraceLoggingWideString((messageText), MIDI_TSHOOT_TRACE_MESSAGE_FIELD),                  \
        TraceLoggingHResult(static_cast<HRESULT>((ex).code()), MIDI_TSHOOT_TRACE_HRESULT_FIELD), \
        TraceLoggingWideString((ex).message().c_str(), MIDI_TSHOOT_TRACE_ERROR_FIELD)            \
    )

#define MIDI_TSHOOT_LOG_GENERAL_EXCEPTION(messageText)                                           \
    LOG_IF_FAILED(E_FAIL);                                                                       \
    TraceLoggingWrite(                                                                           \
        MidiTroubleshooterTelemetryProvider::Provider(),                                         \
        MIDI_TSHOOT_TRACE_EVENT_ERROR,                                                           \
        TraceLoggingString(__FUNCTION__, MIDI_TSHOOT_TRACE_LOCATION_FIELD),                      \
        TraceLoggingLevel(WINEVENT_LEVEL_ERROR),                                                 \
        TraceLoggingWideString((messageText), MIDI_TSHOOT_TRACE_MESSAGE_FIELD)                   \
    )

// Every public entry point that can be reached from XAML or from a background operation is
// wrapped so that an escaping exception can never terminate the process.
#define MIDI_TSHOOT_CATCH_AND_LOG(messageText)                                                   \
    catch (winrt::hresult_error const& ex)                                                       \
    {                                                                                            \
        MIDI_TSHOOT_LOG_HRESULT_EXCEPTION(ex, messageText);                                      \
    }                                                                                            \
    catch (...)                                                                                  \
    {                                                                                            \
        MIDI_TSHOOT_LOG_GENERAL_EXCEPTION(messageText);                                          \
    }
