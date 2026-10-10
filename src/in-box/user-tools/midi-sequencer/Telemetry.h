// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

class MidiSequencerTelemetryProvider : public wil::TraceLoggingProvider
{
    IMPLEMENT_TRACELOGGING_CLASS_WITH_MICROSOFT_TELEMETRY(
        MidiSequencerTelemetryProvider,
        "Microsoft.Windows.Midi2.Sequencer",
        //  PS> [System.Diagnostics.Tracing.EventSource]::new("Microsoft.Windows.Midi2.Sequencer").Guid
        // {088f6e81-015a-5c8c-88eb-d409294999bb}
        (0x088f6e81, 0x015a, 0x5c8c, 0x88, 0xeb, 0xd4, 0x09, 0x29, 0x49, 0x99, 0xbb))
};

#define MIDI_SEQUENCER_TRACE_EVENT_ERROR                  "MidiSequencer.Error"
#define MIDI_SEQUENCER_TRACE_EVENT_WARNING                "MidiSequencer.Warning"
#define MIDI_SEQUENCER_TRACE_EVENT_INFO                   "MidiSequencer.Info"

#define MIDI_SEQUENCER_TRACE_LOCATION_FIELD               "location"
#define MIDI_SEQUENCER_TRACE_MESSAGE_FIELD                "message"
#define MIDI_SEQUENCER_TRACE_HRESULT_FIELD                "hresult"
#define MIDI_SEQUENCER_TRACE_ERROR_FIELD                  "error"

#define MIDI_SEQUENCER_LOG_INFO(messageText)                                                      \
    TraceLoggingWrite(                                                                            \
        MidiSequencerTelemetryProvider::Provider(),                                               \
        MIDI_SEQUENCER_TRACE_EVENT_INFO,                                                          \
        TraceLoggingString(__FUNCTION__, MIDI_SEQUENCER_TRACE_LOCATION_FIELD),                    \
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),                                                   \
        TraceLoggingWideString((messageText), MIDI_SEQUENCER_TRACE_MESSAGE_FIELD)                 \
    )

#define MIDI_SEQUENCER_LOG_HRESULT_EXCEPTION(ex, messageText)                                     \
    LOG_IF_FAILED(static_cast<HRESULT>((ex).code()));                                             \
    TraceLoggingWrite(                                                                            \
        MidiSequencerTelemetryProvider::Provider(),                                               \
        MIDI_SEQUENCER_TRACE_EVENT_ERROR,                                                         \
        TraceLoggingString(__FUNCTION__, MIDI_SEQUENCER_TRACE_LOCATION_FIELD),                    \
        TraceLoggingLevel(WINEVENT_LEVEL_ERROR),                                                  \
        TraceLoggingWideString((messageText), MIDI_SEQUENCER_TRACE_MESSAGE_FIELD),                \
        TraceLoggingHResult(static_cast<HRESULT>((ex).code()), MIDI_SEQUENCER_TRACE_HRESULT_FIELD), \
        TraceLoggingWideString((ex).message().c_str(), MIDI_SEQUENCER_TRACE_ERROR_FIELD)          \
    )

#define MIDI_SEQUENCER_LOG_GENERAL_EXCEPTION(messageText)                                         \
    LOG_IF_FAILED(E_FAIL);                                                                        \
    TraceLoggingWrite(                                                                            \
        MidiSequencerTelemetryProvider::Provider(),                                               \
        MIDI_SEQUENCER_TRACE_EVENT_ERROR,                                                         \
        TraceLoggingString(__FUNCTION__, MIDI_SEQUENCER_TRACE_LOCATION_FIELD),                    \
        TraceLoggingLevel(WINEVENT_LEVEL_ERROR),                                                  \
        TraceLoggingWideString((messageText), MIDI_SEQUENCER_TRACE_MESSAGE_FIELD)                 \
    )

// Every entry point reachable from XAML, a timer, the engine or a MIDI callback is wrapped, so an
// escaping exception can never end the process with the customer's sequence unsaved.
#define MIDI_SEQUENCER_CATCH_AND_LOG(messageText)                                                 \
    catch (winrt::hresult_error const& ex)                                                        \
    {                                                                                             \
        MIDI_SEQUENCER_LOG_HRESULT_EXCEPTION(ex, messageText);                                    \
    }                                                                                             \
    catch (...)                                                                                   \
    {                                                                                             \
        MIDI_SEQUENCER_LOG_GENERAL_EXCEPTION(messageText);                                        \
    }
