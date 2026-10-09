// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

class MidiSoundFontSynthTelemetryProvider : public wil::TraceLoggingProvider
{
    IMPLEMENT_TRACELOGGING_CLASS_WITH_MICROSOFT_TELEMETRY(
        MidiSoundFontSynthTelemetryProvider,
        "Microsoft.Windows.Midi2.SoundFontSynth",
        //  PS> [System.Diagnostics.Tracing.EventSource]::new("Microsoft.Windows.Midi2.SoundFontSynth").Guid
        // {07f3890d-f790-583b-09c3-41b7eccd8cb9}
        (0x07f3890d, 0xf790, 0x583b, 0x09, 0xc3, 0x41, 0xb7, 0xec, 0xcd, 0x8c, 0xb9))
};

#define MIDI_SF2SYNTH_TRACE_EVENT_ERROR               "MidiSoundFontSynth.Error"
#define MIDI_SF2SYNTH_TRACE_EVENT_WARNING             "MidiSoundFontSynth.Warning"
#define MIDI_SF2SYNTH_TRACE_EVENT_INFO                "MidiSoundFontSynth.Info"

#define MIDI_SF2SYNTH_TRACE_LOCATION_FIELD            "location"
#define MIDI_SF2SYNTH_TRACE_MESSAGE_FIELD             "message"
#define MIDI_SF2SYNTH_TRACE_HRESULT_FIELD             "hresult"
#define MIDI_SF2SYNTH_TRACE_ERROR_FIELD               "error"

#define MIDI_SF2SYNTH_LOG_INFO(messageText)                                                      \
    TraceLoggingWrite(                                                                           \
        MidiSoundFontSynthTelemetryProvider::Provider(),                                         \
        MIDI_SF2SYNTH_TRACE_EVENT_INFO,                                                          \
        TraceLoggingString(__FUNCTION__, MIDI_SF2SYNTH_TRACE_LOCATION_FIELD),                    \
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),                                                  \
        TraceLoggingWideString((messageText), MIDI_SF2SYNTH_TRACE_MESSAGE_FIELD)                 \
    )

#define MIDI_SF2SYNTH_LOG_WARNING(messageText)                                                   \
    TraceLoggingWrite(                                                                           \
        MidiSoundFontSynthTelemetryProvider::Provider(),                                         \
        MIDI_SF2SYNTH_TRACE_EVENT_WARNING,                                                       \
        TraceLoggingString(__FUNCTION__, MIDI_SF2SYNTH_TRACE_LOCATION_FIELD),                    \
        TraceLoggingLevel(WINEVENT_LEVEL_WARNING),                                               \
        TraceLoggingWideString((messageText), MIDI_SF2SYNTH_TRACE_MESSAGE_FIELD)                 \
    )

#define MIDI_SF2SYNTH_LOG_HRESULT_EXCEPTION(ex, messageText)                                     \
    LOG_IF_FAILED(static_cast<HRESULT>((ex).code()));                                            \
    TraceLoggingWrite(                                                                           \
        MidiSoundFontSynthTelemetryProvider::Provider(),                                         \
        MIDI_SF2SYNTH_TRACE_EVENT_ERROR,                                                         \
        TraceLoggingString(__FUNCTION__, MIDI_SF2SYNTH_TRACE_LOCATION_FIELD),                    \
        TraceLoggingLevel(WINEVENT_LEVEL_ERROR),                                                 \
        TraceLoggingWideString((messageText), MIDI_SF2SYNTH_TRACE_MESSAGE_FIELD),                \
        TraceLoggingHResult(static_cast<HRESULT>((ex).code()), MIDI_SF2SYNTH_TRACE_HRESULT_FIELD), \
        TraceLoggingWideString((ex).message().c_str(), MIDI_SF2SYNTH_TRACE_ERROR_FIELD)          \
    )

#define MIDI_SF2SYNTH_LOG_GENERAL_EXCEPTION(messageText)                                         \
    LOG_IF_FAILED(E_FAIL);                                                                       \
    TraceLoggingWrite(                                                                           \
        MidiSoundFontSynthTelemetryProvider::Provider(),                                         \
        MIDI_SF2SYNTH_TRACE_EVENT_ERROR,                                                         \
        TraceLoggingString(__FUNCTION__, MIDI_SF2SYNTH_TRACE_LOCATION_FIELD),                    \
        TraceLoggingLevel(WINEVENT_LEVEL_ERROR),                                                 \
        TraceLoggingWideString((messageText), MIDI_SF2SYNTH_TRACE_MESSAGE_FIELD)                 \
    )

// Every entry point that can be reached from XAML, a timer, an SDK callback or a worker thread is
// wrapped, so an escaping exception can never take the process and every synth with it.
#define MIDI_SF2SYNTH_CATCH_AND_LOG(messageText)                                                 \
    catch (winrt::hresult_error const& ex)                                                       \
    {                                                                                            \
        MIDI_SF2SYNTH_LOG_HRESULT_EXCEPTION(ex, messageText);                                    \
    }                                                                                            \
    catch (...)                                                                                  \
    {                                                                                            \
        MIDI_SF2SYNTH_LOG_GENERAL_EXCEPTION(messageText);                                        \
    }
