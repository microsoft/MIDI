// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

class MidiSoundModuleTelemetryProvider : public wil::TraceLoggingProvider
{
    IMPLEMENT_TRACELOGGING_CLASS_WITH_MICROSOFT_TELEMETRY(
        MidiSoundModuleTelemetryProvider,
        "Microsoft.Windows.Midi2.SoundModule",
        //  PS> [System.Diagnostics.Tracing.EventSource]::new("Microsoft.Windows.Midi2.SoundModule").Guid
        // {6aa4a412-960c-5542-93f5-9875357f8f52}
        (0x6aa4a412, 0x960c, 0x5542, 0x93, 0xf5, 0x98, 0x75, 0x35, 0x7f, 0x8f, 0x52))
};

#define MIDI_SF2SYNTH_TRACE_EVENT_ERROR               "MidiSoundModule.Error"
#define MIDI_SF2SYNTH_TRACE_EVENT_WARNING             "MidiSoundModule.Warning"
#define MIDI_SF2SYNTH_TRACE_EVENT_INFO                "MidiSoundModule.Info"

#define MIDI_SF2SYNTH_TRACE_LOCATION_FIELD            "location"
#define MIDI_SF2SYNTH_TRACE_MESSAGE_FIELD             "message"
#define MIDI_SF2SYNTH_TRACE_HRESULT_FIELD             "hresult"
#define MIDI_SF2SYNTH_TRACE_ERROR_FIELD               "error"

#define MIDI_SF2SYNTH_LOG_INFO(messageText)                                                      \
    TraceLoggingWrite(                                                                           \
        MidiSoundModuleTelemetryProvider::Provider(),                                            \
        MIDI_SF2SYNTH_TRACE_EVENT_INFO,                                                          \
        TraceLoggingString(__FUNCTION__, MIDI_SF2SYNTH_TRACE_LOCATION_FIELD),                    \
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),                                                  \
        TraceLoggingWideString((messageText), MIDI_SF2SYNTH_TRACE_MESSAGE_FIELD)                 \
    )

#define MIDI_SF2SYNTH_LOG_WARNING(messageText)                                                   \
    TraceLoggingWrite(                                                                           \
        MidiSoundModuleTelemetryProvider::Provider(),                                            \
        MIDI_SF2SYNTH_TRACE_EVENT_WARNING,                                                       \
        TraceLoggingString(__FUNCTION__, MIDI_SF2SYNTH_TRACE_LOCATION_FIELD),                    \
        TraceLoggingLevel(WINEVENT_LEVEL_WARNING),                                               \
        TraceLoggingWideString((messageText), MIDI_SF2SYNTH_TRACE_MESSAGE_FIELD)                 \
    )

#define MIDI_SF2SYNTH_LOG_HRESULT_EXCEPTION(ex, messageText)                                     \
    LOG_IF_FAILED(static_cast<HRESULT>((ex).code()));                                            \
    TraceLoggingWrite(                                                                           \
        MidiSoundModuleTelemetryProvider::Provider(),                                            \
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
        MidiSoundModuleTelemetryProvider::Provider(),                                            \
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
