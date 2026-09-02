// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

class MidiKeyboardTelemetryProvider : public wil::TraceLoggingProvider
{
    IMPLEMENT_TRACELOGGING_CLASS_WITH_MICROSOFT_TELEMETRY(
        MidiKeyboardTelemetryProvider,
        "Microsoft.Windows.Midi2.VirtualKeyboard",
        //  PS> [System.Diagnostics.Tracing.EventSource]::new("Microsoft.Windows.Midi2.VirtualKeyboard").Guid
        // {4d757852-a6e6-5490-86d9-4ac9794015e1}
        (0x4d757852, 0xa6e6, 0x5490, 0x86, 0xd9, 0x4a, 0xc9, 0x79, 0x40, 0x15, 0xe1))
};

#define MIDI_KEYBOARD_TRACE_EVENT_ERROR                  "MidiKeyboard.Error"
#define MIDI_KEYBOARD_TRACE_EVENT_WARNING                "MidiKeyboard.Warning"
#define MIDI_KEYBOARD_TRACE_EVENT_INFO                   "MidiKeyboard.Info"

#define MIDI_KEYBOARD_TRACE_LOCATION_FIELD               "location"
#define MIDI_KEYBOARD_TRACE_MESSAGE_FIELD                "message"
#define MIDI_KEYBOARD_TRACE_HRESULT_FIELD                "hresult"
#define MIDI_KEYBOARD_TRACE_ERROR_FIELD                  "error"
#define MIDI_KEYBOARD_TRACE_ENDPOINT_DEVICE_ID_FIELD     "endpoint id"

#define MIDI_KEYBOARD_LOG_INFO(messageText)                                                      \
    TraceLoggingWrite(                                                                           \
        MidiKeyboardTelemetryProvider::Provider(),                                               \
        MIDI_KEYBOARD_TRACE_EVENT_INFO,                                                          \
        TraceLoggingString(__FUNCTION__, MIDI_KEYBOARD_TRACE_LOCATION_FIELD),                    \
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),                                                  \
        TraceLoggingWideString((messageText), MIDI_KEYBOARD_TRACE_MESSAGE_FIELD)                 \
    )

#define MIDI_KEYBOARD_LOG_INFO_WITH_ENDPOINT(messageText, endpointId)                            \
    TraceLoggingWrite(                                                                           \
        MidiKeyboardTelemetryProvider::Provider(),                                               \
        MIDI_KEYBOARD_TRACE_EVENT_INFO,                                                          \
        TraceLoggingString(__FUNCTION__, MIDI_KEYBOARD_TRACE_LOCATION_FIELD),                    \
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),                                                  \
        TraceLoggingWideString((messageText), MIDI_KEYBOARD_TRACE_MESSAGE_FIELD),                 \
        TraceLoggingWideString((endpointId), MIDI_KEYBOARD_TRACE_ENDPOINT_DEVICE_ID_FIELD)       \
    )

#define MIDI_KEYBOARD_LOG_HRESULT_EXCEPTION(ex, messageText)                                     \
    LOG_IF_FAILED(static_cast<HRESULT>((ex).code()));                                            \
    TraceLoggingWrite(                                                                           \
        MidiKeyboardTelemetryProvider::Provider(),                                               \
        MIDI_KEYBOARD_TRACE_EVENT_ERROR,                                                         \
        TraceLoggingString(__FUNCTION__, MIDI_KEYBOARD_TRACE_LOCATION_FIELD),                    \
        TraceLoggingLevel(WINEVENT_LEVEL_ERROR),                                                 \
        TraceLoggingWideString((messageText), MIDI_KEYBOARD_TRACE_MESSAGE_FIELD),                 \
        TraceLoggingHResult(static_cast<HRESULT>((ex).code()), MIDI_KEYBOARD_TRACE_HRESULT_FIELD),\
        TraceLoggingWideString((ex).message().c_str(), MIDI_KEYBOARD_TRACE_ERROR_FIELD)          \
    )

#define MIDI_KEYBOARD_LOG_GENERAL_EXCEPTION(messageText)                                         \
    LOG_IF_FAILED(E_FAIL);                                                                       \
    TraceLoggingWrite(                                                                           \
        MidiKeyboardTelemetryProvider::Provider(),                                               \
        MIDI_KEYBOARD_TRACE_EVENT_ERROR,                                                         \
        TraceLoggingString(__FUNCTION__, MIDI_KEYBOARD_TRACE_LOCATION_FIELD),                    \
        TraceLoggingLevel(WINEVENT_LEVEL_ERROR),                                                 \
        TraceLoggingWideString((messageText), MIDI_KEYBOARD_TRACE_MESSAGE_FIELD)                 \
    )

// Every public entry point that can be reached from XAML, from a pointer or keyboard event, or
// from a MIDI callback is wrapped so that an escaping exception can never terminate the process.
#define MIDI_KEYBOARD_CATCH_AND_LOG(messageText)                                                 \
    catch (winrt::hresult_error const& ex)                                                       \
    {                                                                                            \
        MIDI_KEYBOARD_LOG_HRESULT_EXCEPTION(ex, messageText);                                    \
    }                                                                                            \
    catch (...)                                                                                  \
    {                                                                                            \
        MIDI_KEYBOARD_LOG_GENERAL_EXCEPTION(messageText);                                        \
    }
