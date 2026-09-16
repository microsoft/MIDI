// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

class MidiPlayerTelemetryProvider : public wil::TraceLoggingProvider
{
    IMPLEMENT_TRACELOGGING_CLASS_WITH_MICROSOFT_TELEMETRY(
        MidiPlayerTelemetryProvider,
        "Microsoft.Windows.Midi2.Player",
        //  PS> [System.Diagnostics.Tracing.EventSource]::new("Microsoft.Windows.Midi2.Player").Guid
        // {d73d6253-7040-57b5-91da-fff865d35aed}
        (0xd73d6253, 0x7040, 0x57b5, 0x91, 0xda, 0xff, 0xf8, 0x65, 0xd3, 0x5a, 0xed))
};

#define MIDI_PLAYER_TRACE_EVENT_ERROR                  "MidiPlayer.Error"
#define MIDI_PLAYER_TRACE_EVENT_WARNING                "MidiPlayer.Warning"
#define MIDI_PLAYER_TRACE_EVENT_INFO                   "MidiPlayer.Info"

#define MIDI_PLAYER_TRACE_LOCATION_FIELD               "location"
#define MIDI_PLAYER_TRACE_MESSAGE_FIELD                "message"
#define MIDI_PLAYER_TRACE_HRESULT_FIELD                "hresult"
#define MIDI_PLAYER_TRACE_ERROR_FIELD                  "error"
#define MIDI_PLAYER_TRACE_ENDPOINT_DEVICE_ID_FIELD     "endpoint id"

#define MIDI_PLAYER_LOG_INFO(messageText)                                                         \
    TraceLoggingWrite(                                                                            \
        MidiPlayerTelemetryProvider::Provider(),                                                  \
        MIDI_PLAYER_TRACE_EVENT_INFO,                                                             \
        TraceLoggingString(__FUNCTION__, MIDI_PLAYER_TRACE_LOCATION_FIELD),                       \
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),                                                   \
        TraceLoggingWideString((messageText), MIDI_PLAYER_TRACE_MESSAGE_FIELD)                    \
    )

#define MIDI_PLAYER_LOG_INFO_WITH_ENDPOINT(messageText, endpointId)                               \
    TraceLoggingWrite(                                                                            \
        MidiPlayerTelemetryProvider::Provider(),                                                  \
        MIDI_PLAYER_TRACE_EVENT_INFO,                                                             \
        TraceLoggingString(__FUNCTION__, MIDI_PLAYER_TRACE_LOCATION_FIELD),                       \
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),                                                   \
        TraceLoggingWideString((messageText), MIDI_PLAYER_TRACE_MESSAGE_FIELD),                   \
        TraceLoggingWideString((endpointId), MIDI_PLAYER_TRACE_ENDPOINT_DEVICE_ID_FIELD)          \
    )

#define MIDI_PLAYER_LOG_HRESULT_EXCEPTION(ex, messageText)                                        \
    LOG_IF_FAILED(static_cast<HRESULT>((ex).code()));                                             \
    TraceLoggingWrite(                                                                            \
        MidiPlayerTelemetryProvider::Provider(),                                                  \
        MIDI_PLAYER_TRACE_EVENT_ERROR,                                                            \
        TraceLoggingString(__FUNCTION__, MIDI_PLAYER_TRACE_LOCATION_FIELD),                       \
        TraceLoggingLevel(WINEVENT_LEVEL_ERROR),                                                  \
        TraceLoggingWideString((messageText), MIDI_PLAYER_TRACE_MESSAGE_FIELD),                   \
        TraceLoggingHResult(static_cast<HRESULT>((ex).code()), MIDI_PLAYER_TRACE_HRESULT_FIELD),  \
        TraceLoggingWideString((ex).message().c_str(), MIDI_PLAYER_TRACE_ERROR_FIELD)             \
    )

#define MIDI_PLAYER_LOG_GENERAL_EXCEPTION(messageText)                                            \
    LOG_IF_FAILED(E_FAIL);                                                                        \
    TraceLoggingWrite(                                                                            \
        MidiPlayerTelemetryProvider::Provider(),                                                  \
        MIDI_PLAYER_TRACE_EVENT_ERROR,                                                            \
        TraceLoggingString(__FUNCTION__, MIDI_PLAYER_TRACE_LOCATION_FIELD),                       \
        TraceLoggingLevel(WINEVENT_LEVEL_ERROR),                                                  \
        TraceLoggingWideString((messageText), MIDI_PLAYER_TRACE_MESSAGE_FIELD)                    \
    )

// Every public entry point reachable from XAML, from a timer, or from the playback worker is
// wrapped, so an escaping exception can never terminate the process.
#define MIDI_PLAYER_CATCH_AND_LOG(messageText)                                                    \
    catch (winrt::hresult_error const& ex)                                                        \
    {                                                                                             \
        MIDI_PLAYER_LOG_HRESULT_EXCEPTION(ex, messageText);                                       \
    }                                                                                             \
    catch (...)                                                                                   \
    {                                                                                             \
        MIDI_PLAYER_LOG_GENERAL_EXCEPTION(messageText);                                           \
    }
