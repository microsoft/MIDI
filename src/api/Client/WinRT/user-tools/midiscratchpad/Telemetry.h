// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

class MidiScratchPadTelemetryProvider : public wil::TraceLoggingProvider
{
    IMPLEMENT_TRACELOGGING_CLASS_WITH_MICROSOFT_TELEMETRY(
        MidiScratchPadTelemetryProvider,
        "Microsoft.Windows.Midi2.ScratchPad",
        //  PS> [System.Diagnostics.Tracing.EventSource]::new("Microsoft.Windows.Midi2.ScratchPad").Guid
        // {870ce634-cad0-541e-2eba-206739a2b310}
        (0x870ce634, 0xcad0, 0x541e, 0x2e, 0xba, 0x20, 0x67, 0x39, 0xa2, 0xb3, 0x10))
};

#define MIDI_SCRATCHPAD_TRACE_EVENT_ERROR                  "MidiScratchPad.Error"
#define MIDI_SCRATCHPAD_TRACE_EVENT_WARNING                "MidiScratchPad.Warning"
#define MIDI_SCRATCHPAD_TRACE_EVENT_INFO                   "MidiScratchPad.Info"

#define MIDI_SCRATCHPAD_TRACE_LOCATION_FIELD               "location"
#define MIDI_SCRATCHPAD_TRACE_MESSAGE_FIELD                "message"
#define MIDI_SCRATCHPAD_TRACE_HRESULT_FIELD                "hresult"
#define MIDI_SCRATCHPAD_TRACE_ERROR_FIELD                  "error"
#define MIDI_SCRATCHPAD_TRACE_ENDPOINT_DEVICE_ID_FIELD     "endpoint id"

#define MIDI_SCRATCHPAD_LOG_INFO(messageText)                                                       \
    TraceLoggingWrite(                                                                           \
        MidiScratchPadTelemetryProvider::Provider(),                                               \
        MIDI_SCRATCHPAD_TRACE_EVENT_INFO,                                                           \
        TraceLoggingString(__FUNCTION__, MIDI_SCRATCHPAD_TRACE_LOCATION_FIELD),                     \
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),                                                  \
        TraceLoggingWideString((messageText), MIDI_SCRATCHPAD_TRACE_MESSAGE_FIELD)                  \
    )

#define MIDI_SCRATCHPAD_LOG_INFO_WITH_ENDPOINT(messageText, endpointId)                             \
    TraceLoggingWrite(                                                                           \
        MidiScratchPadTelemetryProvider::Provider(),                                               \
        MIDI_SCRATCHPAD_TRACE_EVENT_INFO,                                                           \
        TraceLoggingString(__FUNCTION__, MIDI_SCRATCHPAD_TRACE_LOCATION_FIELD),                     \
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),                                                  \
        TraceLoggingWideString((messageText), MIDI_SCRATCHPAD_TRACE_MESSAGE_FIELD),                 \
        TraceLoggingWideString((endpointId), MIDI_SCRATCHPAD_TRACE_ENDPOINT_DEVICE_ID_FIELD)        \
    )

#define MIDI_SCRATCHPAD_LOG_HRESULT_EXCEPTION(ex, messageText)                                      \
    LOG_IF_FAILED(static_cast<HRESULT>((ex).code()));                                            \
    TraceLoggingWrite(                                                                           \
        MidiScratchPadTelemetryProvider::Provider(),                                               \
        MIDI_SCRATCHPAD_TRACE_EVENT_ERROR,                                                          \
        TraceLoggingString(__FUNCTION__, MIDI_SCRATCHPAD_TRACE_LOCATION_FIELD),                     \
        TraceLoggingLevel(WINEVENT_LEVEL_ERROR),                                                 \
        TraceLoggingWideString((messageText), MIDI_SCRATCHPAD_TRACE_MESSAGE_FIELD),                 \
        TraceLoggingHResult(static_cast<HRESULT>((ex).code()), MIDI_SCRATCHPAD_TRACE_HRESULT_FIELD),\
        TraceLoggingWideString((ex).message().c_str(), MIDI_SCRATCHPAD_TRACE_ERROR_FIELD)           \
    )

#define MIDI_SCRATCHPAD_LOG_GENERAL_EXCEPTION(messageText)                                          \
    LOG_IF_FAILED(E_FAIL);                                                                       \
    TraceLoggingWrite(                                                                           \
        MidiScratchPadTelemetryProvider::Provider(),                                               \
        MIDI_SCRATCHPAD_TRACE_EVENT_ERROR,                                                          \
        TraceLoggingString(__FUNCTION__, MIDI_SCRATCHPAD_TRACE_LOCATION_FIELD),                     \
        TraceLoggingLevel(WINEVENT_LEVEL_ERROR),                                                 \
        TraceLoggingWideString((messageText), MIDI_SCRATCHPAD_TRACE_MESSAGE_FIELD)                  \
    )

// Every public entry point that can be reached from XAML or from a MIDI callback is wrapped
// so that an escaping exception can never terminate the process.
#define MIDI_SCRATCHPAD_CATCH_AND_LOG(messageText)                                                  \
    catch (winrt::hresult_error const& ex)                                                       \
    {                                                                                            \
        MIDI_SCRATCHPAD_LOG_HRESULT_EXCEPTION(ex, messageText);                                     \
    }                                                                                            \
    catch (...)                                                                                  \
    {                                                                                            \
        MIDI_SCRATCHPAD_LOG_GENERAL_EXCEPTION(messageText);                                         \
    }
