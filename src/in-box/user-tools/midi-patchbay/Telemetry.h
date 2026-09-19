// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

class MidiPatchbayTelemetryProvider : public wil::TraceLoggingProvider
{
    IMPLEMENT_TRACELOGGING_CLASS_WITH_MICROSOFT_TELEMETRY(
        MidiPatchbayTelemetryProvider,
        "Microsoft.Windows.Midi2.Patchbay",
        //  PS> [System.Diagnostics.Tracing.EventSource]::new("Microsoft.Windows.Midi2.Patchbay").Guid
        // {63051d43-f3c0-5fc5-8e54-6ff2ad9480ce}
        (0x63051d43, 0xf3c0, 0x5fc5, 0x8e, 0x54, 0x6f, 0xf2, 0xad, 0x94, 0x80, 0xce))
};

#define MIDI_PATCHBAY_TRACE_EVENT_ERROR                  "MidiPatchbay.Error"
#define MIDI_PATCHBAY_TRACE_EVENT_WARNING                "MidiPatchbay.Warning"
#define MIDI_PATCHBAY_TRACE_EVENT_INFO                   "MidiPatchbay.Info"

#define MIDI_PATCHBAY_TRACE_LOCATION_FIELD               "location"
#define MIDI_PATCHBAY_TRACE_MESSAGE_FIELD                "message"
#define MIDI_PATCHBAY_TRACE_HRESULT_FIELD                "hresult"
#define MIDI_PATCHBAY_TRACE_ERROR_FIELD                  "error"
#define MIDI_PATCHBAY_TRACE_ENDPOINT_DEVICE_ID_FIELD     "endpoint id"

#define MIDI_PATCHBAY_LOG_INFO(messageText)                                                         \
    TraceLoggingWrite(                                                                              \
        MidiPatchbayTelemetryProvider::Provider(),                                                  \
        MIDI_PATCHBAY_TRACE_EVENT_INFO,                                                             \
        TraceLoggingString(__FUNCTION__, MIDI_PATCHBAY_TRACE_LOCATION_FIELD),                       \
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),                                                     \
        TraceLoggingWideString((messageText), MIDI_PATCHBAY_TRACE_MESSAGE_FIELD)                    \
    )

#define MIDI_PATCHBAY_LOG_INFO_WITH_ENDPOINT(messageText, endpointId)                               \
    TraceLoggingWrite(                                                                              \
        MidiPatchbayTelemetryProvider::Provider(),                                                  \
        MIDI_PATCHBAY_TRACE_EVENT_INFO,                                                             \
        TraceLoggingString(__FUNCTION__, MIDI_PATCHBAY_TRACE_LOCATION_FIELD),                       \
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),                                                     \
        TraceLoggingWideString((messageText), MIDI_PATCHBAY_TRACE_MESSAGE_FIELD),                   \
        TraceLoggingWideString((endpointId), MIDI_PATCHBAY_TRACE_ENDPOINT_DEVICE_ID_FIELD)          \
    )

#define MIDI_PATCHBAY_LOG_WARNING(messageText)                                                      \
    TraceLoggingWrite(                                                                              \
        MidiPatchbayTelemetryProvider::Provider(),                                                  \
        MIDI_PATCHBAY_TRACE_EVENT_WARNING,                                                          \
        TraceLoggingString(__FUNCTION__, MIDI_PATCHBAY_TRACE_LOCATION_FIELD),                       \
        TraceLoggingLevel(WINEVENT_LEVEL_WARNING),                                                  \
        TraceLoggingWideString((messageText), MIDI_PATCHBAY_TRACE_MESSAGE_FIELD)                    \
    )

#define MIDI_PATCHBAY_LOG_HRESULT_EXCEPTION(ex, messageText)                                        \
    LOG_IF_FAILED(static_cast<HRESULT>((ex).code()));                                               \
    TraceLoggingWrite(                                                                              \
        MidiPatchbayTelemetryProvider::Provider(),                                                  \
        MIDI_PATCHBAY_TRACE_EVENT_ERROR,                                                            \
        TraceLoggingString(__FUNCTION__, MIDI_PATCHBAY_TRACE_LOCATION_FIELD),                       \
        TraceLoggingLevel(WINEVENT_LEVEL_ERROR),                                                    \
        TraceLoggingWideString((messageText), MIDI_PATCHBAY_TRACE_MESSAGE_FIELD),                   \
        TraceLoggingHResult(static_cast<HRESULT>((ex).code()), MIDI_PATCHBAY_TRACE_HRESULT_FIELD),  \
        TraceLoggingWideString((ex).message().c_str(), MIDI_PATCHBAY_TRACE_ERROR_FIELD)             \
    )

#define MIDI_PATCHBAY_LOG_GENERAL_EXCEPTION(messageText)                                            \
    LOG_IF_FAILED(E_FAIL);                                                                          \
    TraceLoggingWrite(                                                                              \
        MidiPatchbayTelemetryProvider::Provider(),                                                  \
        MIDI_PATCHBAY_TRACE_EVENT_ERROR,                                                            \
        TraceLoggingString(__FUNCTION__, MIDI_PATCHBAY_TRACE_LOCATION_FIELD),                       \
        TraceLoggingLevel(WINEVENT_LEVEL_ERROR),                                                    \
        TraceLoggingWideString((messageText), MIDI_PATCHBAY_TRACE_MESSAGE_FIELD)                    \
    )

// Every public entry point reachable from XAML, from a timer, from a device watcher or from the
// service callback threads is wrapped so an escaping exception can never terminate the process.
#define MIDI_PATCHBAY_CATCH_AND_LOG(messageText)                                                    \
    catch (winrt::hresult_error const& ex)                                                          \
    {                                                                                               \
        MIDI_PATCHBAY_LOG_HRESULT_EXCEPTION(ex, messageText);                                       \
    }                                                                                               \
    catch (...)                                                                                     \
    {                                                                                               \
        MIDI_PATCHBAY_LOG_GENERAL_EXCEPTION(messageText);                                           \
    }
