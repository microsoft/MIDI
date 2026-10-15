// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

class MidiSoundBankSynthTelemetryProvider : public wil::TraceLoggingProvider
{
    IMPLEMENT_TRACELOGGING_CLASS_WITH_MICROSOFT_TELEMETRY(
        MidiSoundBankSynthTelemetryProvider,
        "Microsoft.Windows.Midi2.SoundBankSynth",
        //  PS> [System.Diagnostics.Tracing.EventSource]::new("Microsoft.Windows.Midi2.SoundBankSynth").Guid
        // {0e10866d-f9dc-5200-ca0e-76ae0d231a1b}
        (0x0e10866d, 0xf9dc, 0x5200, 0xca, 0x0e, 0x76, 0xae, 0x0d, 0x23, 0x1a, 0x1b))
};

#define MIDI_SF2SYNTH_TRACE_EVENT_ERROR               "MidiSoundBankSynth.Error"
#define MIDI_SF2SYNTH_TRACE_EVENT_WARNING             "MidiSoundBankSynth.Warning"
#define MIDI_SF2SYNTH_TRACE_EVENT_INFO                "MidiSoundBankSynth.Info"

#define MIDI_SF2SYNTH_TRACE_LOCATION_FIELD            "location"
#define MIDI_SF2SYNTH_TRACE_MESSAGE_FIELD             "message"
#define MIDI_SF2SYNTH_TRACE_HRESULT_FIELD             "hresult"
#define MIDI_SF2SYNTH_TRACE_ERROR_FIELD               "error"

#define MIDI_SF2SYNTH_LOG_INFO(messageText)                                                      \
    TraceLoggingWrite(                                                                           \
        MidiSoundBankSynthTelemetryProvider::Provider(),                                         \
        MIDI_SF2SYNTH_TRACE_EVENT_INFO,                                                          \
        TraceLoggingString(__FUNCTION__, MIDI_SF2SYNTH_TRACE_LOCATION_FIELD),                    \
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),                                                  \
        TraceLoggingWideString((messageText), MIDI_SF2SYNTH_TRACE_MESSAGE_FIELD)                 \
    )

#define MIDI_SF2SYNTH_LOG_WARNING(messageText)                                                   \
    TraceLoggingWrite(                                                                           \
        MidiSoundBankSynthTelemetryProvider::Provider(),                                         \
        MIDI_SF2SYNTH_TRACE_EVENT_WARNING,                                                       \
        TraceLoggingString(__FUNCTION__, MIDI_SF2SYNTH_TRACE_LOCATION_FIELD),                    \
        TraceLoggingLevel(WINEVENT_LEVEL_WARNING),                                               \
        TraceLoggingWideString((messageText), MIDI_SF2SYNTH_TRACE_MESSAGE_FIELD)                 \
    )

#define MIDI_SF2SYNTH_LOG_HRESULT_EXCEPTION(ex, messageText)                                     \
    LOG_IF_FAILED(static_cast<HRESULT>((ex).code()));                                            \
    TraceLoggingWrite(                                                                           \
        MidiSoundBankSynthTelemetryProvider::Provider(),                                         \
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
        MidiSoundBankSynthTelemetryProvider::Provider(),                                         \
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
