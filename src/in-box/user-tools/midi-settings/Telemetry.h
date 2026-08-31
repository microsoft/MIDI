// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

class MidiSettingsTelemetryProvider : public wil::TraceLoggingProvider
{
    IMPLEMENT_TRACELOGGING_CLASS_WITH_MICROSOFT_TELEMETRY(
        MidiSettingsTelemetryProvider,
        "Microsoft.Windows.Midi2.Settings",
        //  PS> [System.Diagnostics.Tracing.EventSource]::new("Microsoft.Windows.Midi2.Settings").Guid
        // {a0754983-bad3-5c5f-29f3-2949ed06ae4d}
        (0xa0754983, 0xbad3, 0x5c5f, 0x29, 0xf3, 0x29, 0x49, 0xed, 0x06, 0xae, 0x4d))
};

#define MIDI_SETTINGS_TRACE_EVENT_ERROR                 "MidiSettings.Error"
#define MIDI_SETTINGS_TRACE_EVENT_WARNING               "MidiSettings.Warning"
#define MIDI_SETTINGS_TRACE_EVENT_INFO                  "MidiSettings.Info"

#define MIDI_SETTINGS_TRACE_LOCATION_FIELD              "location"
#define MIDI_SETTINGS_TRACE_MESSAGE_FIELD               "message"
#define MIDI_SETTINGS_TRACE_HRESULT_FIELD               "hresult"
#define MIDI_SETTINGS_TRACE_ERROR_FIELD                 "error"
#define MIDI_SETTINGS_TRACE_DETAIL_FIELD                "detail"

#define MIDI_SETTINGS_LOG_INFO(messageText)                                                        \
    TraceLoggingWrite(                                                                             \
        MidiSettingsTelemetryProvider::Provider(),                                                 \
        MIDI_SETTINGS_TRACE_EVENT_INFO,                                                            \
        TraceLoggingString(__FUNCTION__, MIDI_SETTINGS_TRACE_LOCATION_FIELD),                      \
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),                                                    \
        TraceLoggingWideString((messageText), MIDI_SETTINGS_TRACE_MESSAGE_FIELD)                   \
    )

#define MIDI_SETTINGS_LOG_INFO_WITH_DETAIL(messageText, detailText)                                \
    TraceLoggingWrite(                                                                             \
        MidiSettingsTelemetryProvider::Provider(),                                                 \
        MIDI_SETTINGS_TRACE_EVENT_INFO,                                                            \
        TraceLoggingString(__FUNCTION__, MIDI_SETTINGS_TRACE_LOCATION_FIELD),                      \
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),                                                    \
        TraceLoggingWideString((messageText), MIDI_SETTINGS_TRACE_MESSAGE_FIELD),                  \
        TraceLoggingWideString((detailText), MIDI_SETTINGS_TRACE_DETAIL_FIELD)                     \
    )

#define MIDI_SETTINGS_LOG_WARNING(messageText)                                                     \
    TraceLoggingWrite(                                                                             \
        MidiSettingsTelemetryProvider::Provider(),                                                 \
        MIDI_SETTINGS_TRACE_EVENT_WARNING,                                                         \
        TraceLoggingString(__FUNCTION__, MIDI_SETTINGS_TRACE_LOCATION_FIELD),                      \
        TraceLoggingLevel(WINEVENT_LEVEL_WARNING),                                                 \
        TraceLoggingWideString((messageText), MIDI_SETTINGS_TRACE_MESSAGE_FIELD)                   \
    )

#define MIDI_SETTINGS_LOG_HRESULT_EXCEPTION(ex, messageText)                                       \
    LOG_IF_FAILED(static_cast<HRESULT>((ex).code()));                                              \
    TraceLoggingWrite(                                                                             \
        MidiSettingsTelemetryProvider::Provider(),                                                 \
        MIDI_SETTINGS_TRACE_EVENT_ERROR,                                                           \
        TraceLoggingString(__FUNCTION__, MIDI_SETTINGS_TRACE_LOCATION_FIELD),                      \
        TraceLoggingLevel(WINEVENT_LEVEL_ERROR),                                                   \
        TraceLoggingWideString((messageText), MIDI_SETTINGS_TRACE_MESSAGE_FIELD),                  \
        TraceLoggingHResult(static_cast<HRESULT>((ex).code()), MIDI_SETTINGS_TRACE_HRESULT_FIELD), \
        TraceLoggingWideString((ex).message().c_str(), MIDI_SETTINGS_TRACE_ERROR_FIELD)            \
    )

#define MIDI_SETTINGS_LOG_GENERAL_EXCEPTION(messageText)                                           \
    LOG_IF_FAILED(E_FAIL);                                                                         \
    TraceLoggingWrite(                                                                             \
        MidiSettingsTelemetryProvider::Provider(),                                                 \
        MIDI_SETTINGS_TRACE_EVENT_ERROR,                                                           \
        TraceLoggingString(__FUNCTION__, MIDI_SETTINGS_TRACE_LOCATION_FIELD),                      \
        TraceLoggingLevel(WINEVENT_LEVEL_ERROR),                                                   \
        TraceLoggingWideString((messageText), MIDI_SETTINGS_TRACE_MESSAGE_FIELD)                   \
    )

// Every public entry point that can be reached from XAML or from a background operation is
// wrapped so that an escaping exception can never terminate the process.
#define MIDI_SETTINGS_CATCH_AND_LOG(messageText)                                                   \
    catch (winrt::hresult_error const& ex)                                                         \
    {                                                                                              \
        MIDI_SETTINGS_LOG_HRESULT_EXCEPTION(ex, messageText);                                      \
    }                                                                                              \
    catch (...)                                                                                    \
    {                                                                                              \
        MIDI_SETTINGS_LOG_GENERAL_EXCEPTION(messageText);                                          \
    }
