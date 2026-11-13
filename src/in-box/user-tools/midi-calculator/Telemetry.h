// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

class MidiCalculatorTelemetryProvider : public wil::TraceLoggingProvider
{
    IMPLEMENT_TRACELOGGING_CLASS_WITH_MICROSOFT_TELEMETRY(
        MidiCalculatorTelemetryProvider,
        "Microsoft.Windows.Midi2.Calculator",
        //  PS> [System.Diagnostics.Tracing.EventSource]::new("Microsoft.Windows.Midi2.Calculator").Guid
        // {5830f992-086f-5a38-221c-d25c08be6f61}
        (0x5830f992, 0x086f, 0x5a38, 0x22, 0x1c, 0xd2, 0x5c, 0x08, 0xbe, 0x6f, 0x61))
};

#define MIDI_CALCULATOR_TRACE_EVENT_ERROR                  "MidiCalculator.Error"
#define MIDI_CALCULATOR_TRACE_EVENT_WARNING                "MidiCalculator.Warning"
#define MIDI_CALCULATOR_TRACE_EVENT_INFO                   "MidiCalculator.Info"

#define MIDI_CALCULATOR_TRACE_LOCATION_FIELD               "location"
#define MIDI_CALCULATOR_TRACE_MESSAGE_FIELD                "message"
#define MIDI_CALCULATOR_TRACE_HRESULT_FIELD                "hresult"
#define MIDI_CALCULATOR_TRACE_ERROR_FIELD                  "error"
#define MIDI_CALCULATOR_TRACE_ENDPOINT_DEVICE_ID_FIELD     "endpoint id"

#define MIDI_CALCULATOR_LOG_INFO(messageText)                                                       \
    TraceLoggingWrite(                                                                           \
        MidiCalculatorTelemetryProvider::Provider(),                                               \
        MIDI_CALCULATOR_TRACE_EVENT_INFO,                                                           \
        TraceLoggingString(__FUNCTION__, MIDI_CALCULATOR_TRACE_LOCATION_FIELD),                     \
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),                                                  \
        TraceLoggingWideString((messageText), MIDI_CALCULATOR_TRACE_MESSAGE_FIELD)                  \
    )

#define MIDI_CALCULATOR_LOG_INFO_WITH_ENDPOINT(messageText, endpointId)                             \
    TraceLoggingWrite(                                                                           \
        MidiCalculatorTelemetryProvider::Provider(),                                               \
        MIDI_CALCULATOR_TRACE_EVENT_INFO,                                                           \
        TraceLoggingString(__FUNCTION__, MIDI_CALCULATOR_TRACE_LOCATION_FIELD),                     \
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),                                                  \
        TraceLoggingWideString((messageText), MIDI_CALCULATOR_TRACE_MESSAGE_FIELD),                 \
        TraceLoggingWideString((endpointId), MIDI_CALCULATOR_TRACE_ENDPOINT_DEVICE_ID_FIELD)        \
    )

#define MIDI_CALCULATOR_LOG_HRESULT_EXCEPTION(ex, messageText)                                      \
    LOG_IF_FAILED(static_cast<HRESULT>((ex).code()));                                            \
    TraceLoggingWrite(                                                                           \
        MidiCalculatorTelemetryProvider::Provider(),                                               \
        MIDI_CALCULATOR_TRACE_EVENT_ERROR,                                                          \
        TraceLoggingString(__FUNCTION__, MIDI_CALCULATOR_TRACE_LOCATION_FIELD),                     \
        TraceLoggingLevel(WINEVENT_LEVEL_ERROR),                                                 \
        TraceLoggingWideString((messageText), MIDI_CALCULATOR_TRACE_MESSAGE_FIELD),                 \
        TraceLoggingHResult(static_cast<HRESULT>((ex).code()), MIDI_CALCULATOR_TRACE_HRESULT_FIELD),\
        TraceLoggingWideString((ex).message().c_str(), MIDI_CALCULATOR_TRACE_ERROR_FIELD)           \
    )

#define MIDI_CALCULATOR_LOG_GENERAL_EXCEPTION(messageText)                                          \
    LOG_IF_FAILED(E_FAIL);                                                                       \
    TraceLoggingWrite(                                                                           \
        MidiCalculatorTelemetryProvider::Provider(),                                               \
        MIDI_CALCULATOR_TRACE_EVENT_ERROR,                                                          \
        TraceLoggingString(__FUNCTION__, MIDI_CALCULATOR_TRACE_LOCATION_FIELD),                     \
        TraceLoggingLevel(WINEVENT_LEVEL_ERROR),                                                 \
        TraceLoggingWideString((messageText), MIDI_CALCULATOR_TRACE_MESSAGE_FIELD)                  \
    )

// Every public entry point that can be reached from XAML or from a MIDI callback is wrapped
// so that an escaping exception can never terminate the process.
#define MIDI_CALCULATOR_CATCH_AND_LOG(messageText)                                                  \
    catch (winrt::hresult_error const& ex)                                                       \
    {                                                                                            \
        MIDI_CALCULATOR_LOG_HRESULT_EXCEPTION(ex, messageText);                                     \
    }                                                                                            \
    catch (...)                                                                                  \
    {                                                                                            \
        MIDI_CALCULATOR_LOG_GENERAL_EXCEPTION(messageText);                                         \
    }
