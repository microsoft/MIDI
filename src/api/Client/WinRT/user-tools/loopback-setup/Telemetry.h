// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

class MidiLoopbackSetupTelemetryProvider : public wil::TraceLoggingProvider
{
    IMPLEMENT_TRACELOGGING_CLASS_WITH_MICROSOFT_TELEMETRY(
        MidiLoopbackSetupTelemetryProvider,
        "Microsoft.Windows.Midi2.LoopbackSetup",
        //  PS> [System.Diagnostics.Tracing.EventSource]::new("Microsoft.Windows.Midi2.LoopbackSetup").Guid
        // {779e7e5e-1ce4-54bb-17f0-b57b95f9e41f}
        (0x779e7e5e, 0x1ce4, 0x54bb, 0x17, 0xf0, 0xb5, 0x7b, 0x95, 0xf9, 0xe4, 0x1f))
};

#define MIDI_LOOPSETUP_TRACE_EVENT_ERROR                  "MidiLoopbackSetup.Error"
#define MIDI_LOOPSETUP_TRACE_EVENT_WARNING                "MidiLoopbackSetup.Warning"
#define MIDI_LOOPSETUP_TRACE_EVENT_INFO                   "MidiLoopbackSetup.Info"

#define MIDI_LOOPSETUP_TRACE_LOCATION_FIELD               "location"
#define MIDI_LOOPSETUP_TRACE_MESSAGE_FIELD                "message"
#define MIDI_LOOPSETUP_TRACE_HRESULT_FIELD                "hresult"
#define MIDI_LOOPSETUP_TRACE_ERROR_FIELD                  "error"
#define MIDI_LOOPSETUP_TRACE_ENDPOINT_DEVICE_ID_FIELD     "endpoint id"

#define MIDI_LOOPSETUP_LOG_INFO(messageText)                                                      \
    TraceLoggingWrite(                                                                            \
        MidiLoopbackSetupTelemetryProvider::Provider(),                                           \
        MIDI_LOOPSETUP_TRACE_EVENT_INFO,                                                          \
        TraceLoggingString(__FUNCTION__, MIDI_LOOPSETUP_TRACE_LOCATION_FIELD),                    \
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),                                                   \
        TraceLoggingWideString((messageText), MIDI_LOOPSETUP_TRACE_MESSAGE_FIELD)                 \
    )

#define MIDI_LOOPSETUP_LOG_INFO_WITH_ENDPOINT(messageText, endpointId)                            \
    TraceLoggingWrite(                                                                            \
        MidiLoopbackSetupTelemetryProvider::Provider(),                                           \
        MIDI_LOOPSETUP_TRACE_EVENT_INFO,                                                          \
        TraceLoggingString(__FUNCTION__, MIDI_LOOPSETUP_TRACE_LOCATION_FIELD),                    \
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),                                                   \
        TraceLoggingWideString((messageText), MIDI_LOOPSETUP_TRACE_MESSAGE_FIELD),                \
        TraceLoggingWideString((endpointId), MIDI_LOOPSETUP_TRACE_ENDPOINT_DEVICE_ID_FIELD)       \
    )

#define MIDI_LOOPSETUP_LOG_HRESULT_EXCEPTION(ex, messageText)                                     \
    LOG_IF_FAILED(static_cast<HRESULT>((ex).code()));                                             \
    TraceLoggingWrite(                                                                            \
        MidiLoopbackSetupTelemetryProvider::Provider(),                                           \
        MIDI_LOOPSETUP_TRACE_EVENT_ERROR,                                                         \
        TraceLoggingString(__FUNCTION__, MIDI_LOOPSETUP_TRACE_LOCATION_FIELD),                    \
        TraceLoggingLevel(WINEVENT_LEVEL_ERROR),                                                  \
        TraceLoggingWideString((messageText), MIDI_LOOPSETUP_TRACE_MESSAGE_FIELD),                \
        TraceLoggingHResult(static_cast<HRESULT>((ex).code()), MIDI_LOOPSETUP_TRACE_HRESULT_FIELD),\
        TraceLoggingWideString((ex).message().c_str(), MIDI_LOOPSETUP_TRACE_ERROR_FIELD)          \
    )

#define MIDI_LOOPSETUP_LOG_GENERAL_EXCEPTION(messageText)                                         \
    LOG_IF_FAILED(E_FAIL);                                                                        \
    TraceLoggingWrite(                                                                            \
        MidiLoopbackSetupTelemetryProvider::Provider(),                                           \
        MIDI_LOOPSETUP_TRACE_EVENT_ERROR,                                                         \
        TraceLoggingString(__FUNCTION__, MIDI_LOOPSETUP_TRACE_LOCATION_FIELD),                    \
        TraceLoggingLevel(WINEVENT_LEVEL_ERROR),                                                  \
        TraceLoggingWideString((messageText), MIDI_LOOPSETUP_TRACE_MESSAGE_FIELD)                 \
    )

// Every public entry point that can be reached from XAML or from a MIDI callback is wrapped
// so that an escaping exception can never terminate the process.
#define MIDI_LOOPSETUP_CATCH_AND_LOG(messageText)                                                 \
    catch (winrt::hresult_error const& ex)                                                        \
    {                                                                                             \
        MIDI_LOOPSETUP_LOG_HRESULT_EXCEPTION(ex, messageText);                                    \
    }                                                                                             \
    catch (...)                                                                                   \
    {                                                                                             \
        MIDI_LOOPSETUP_LOG_GENERAL_EXCEPTION(messageText);                                        \
    }
