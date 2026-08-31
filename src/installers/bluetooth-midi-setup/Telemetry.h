// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

class MidiBluetoothSetupTelemetryProvider : public wil::TraceLoggingProvider
{
    IMPLEMENT_TRACELOGGING_CLASS_WITH_MICROSOFT_TELEMETRY(
        MidiBluetoothSetupTelemetryProvider,
        "Microsoft.Windows.Midi2.BluetoothSetup",
        //  PS> [System.Diagnostics.Tracing.EventSource]::new("Microsoft.Windows.Midi2.BluetoothSetup").Guid
        // {95d87ae2-f81b-51a8-a4a2-bdc459065b9f}
        (0x95d87ae2, 0xf81b, 0x51a8, 0xa4, 0xa2, 0xbd, 0xc4, 0x59, 0x06, 0x5b, 0x9f))
};

#define MIDI_BTSETUP_TRACE_EVENT_ERROR                  "MidiBluetoothSetup.Error"
#define MIDI_BTSETUP_TRACE_EVENT_WARNING                "MidiBluetoothSetup.Warning"
#define MIDI_BTSETUP_TRACE_EVENT_INFO                   "MidiBluetoothSetup.Info"

#define MIDI_BTSETUP_TRACE_LOCATION_FIELD               "location"
#define MIDI_BTSETUP_TRACE_MESSAGE_FIELD                "message"
#define MIDI_BTSETUP_TRACE_HRESULT_FIELD                "hresult"
#define MIDI_BTSETUP_TRACE_ERROR_FIELD                  "error"
#define MIDI_BTSETUP_TRACE_ENDPOINT_DEVICE_ID_FIELD     "endpoint id"

#define MIDI_BTSETUP_LOG_INFO(messageText)                                                       \
    TraceLoggingWrite(                                                                           \
        MidiBluetoothSetupTelemetryProvider::Provider(),                                               \
        MIDI_BTSETUP_TRACE_EVENT_INFO,                                                           \
        TraceLoggingString(__FUNCTION__, MIDI_BTSETUP_TRACE_LOCATION_FIELD),                     \
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),                                                  \
        TraceLoggingWideString((messageText), MIDI_BTSETUP_TRACE_MESSAGE_FIELD)                  \
    )

#define MIDI_BTSETUP_LOG_INFO_WITH_ENDPOINT(messageText, endpointId)                             \
    TraceLoggingWrite(                                                                           \
        MidiBluetoothSetupTelemetryProvider::Provider(),                                               \
        MIDI_BTSETUP_TRACE_EVENT_INFO,                                                           \
        TraceLoggingString(__FUNCTION__, MIDI_BTSETUP_TRACE_LOCATION_FIELD),                     \
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),                                                  \
        TraceLoggingWideString((messageText), MIDI_BTSETUP_TRACE_MESSAGE_FIELD),                 \
        TraceLoggingWideString((endpointId), MIDI_BTSETUP_TRACE_ENDPOINT_DEVICE_ID_FIELD)        \
    )

#define MIDI_BTSETUP_LOG_HRESULT_EXCEPTION(ex, messageText)                                      \
    LOG_IF_FAILED(static_cast<HRESULT>((ex).code()));                                            \
    TraceLoggingWrite(                                                                           \
        MidiBluetoothSetupTelemetryProvider::Provider(),                                               \
        MIDI_BTSETUP_TRACE_EVENT_ERROR,                                                          \
        TraceLoggingString(__FUNCTION__, MIDI_BTSETUP_TRACE_LOCATION_FIELD),                     \
        TraceLoggingLevel(WINEVENT_LEVEL_ERROR),                                                 \
        TraceLoggingWideString((messageText), MIDI_BTSETUP_TRACE_MESSAGE_FIELD),                 \
        TraceLoggingHResult(static_cast<HRESULT>((ex).code()), MIDI_BTSETUP_TRACE_HRESULT_FIELD),\
        TraceLoggingWideString((ex).message().c_str(), MIDI_BTSETUP_TRACE_ERROR_FIELD)           \
    )

#define MIDI_BTSETUP_LOG_GENERAL_EXCEPTION(messageText)                                          \
    LOG_IF_FAILED(E_FAIL);                                                                       \
    TraceLoggingWrite(                                                                           \
        MidiBluetoothSetupTelemetryProvider::Provider(),                                               \
        MIDI_BTSETUP_TRACE_EVENT_ERROR,                                                          \
        TraceLoggingString(__FUNCTION__, MIDI_BTSETUP_TRACE_LOCATION_FIELD),                     \
        TraceLoggingLevel(WINEVENT_LEVEL_ERROR),                                                 \
        TraceLoggingWideString((messageText), MIDI_BTSETUP_TRACE_MESSAGE_FIELD)                  \
    )

// Every public entry point that can be reached from XAML or from a MIDI callback is wrapped
// so that an escaping exception can never terminate the process.
#define MIDI_BTSETUP_CATCH_AND_LOG(messageText)                                                  \
    catch (winrt::hresult_error const& ex)                                                       \
    {                                                                                            \
        MIDI_BTSETUP_LOG_HRESULT_EXCEPTION(ex, messageText);                                     \
    }                                                                                            \
    catch (...)                                                                                  \
    {                                                                                            \
        MIDI_BTSETUP_LOG_GENERAL_EXCEPTION(messageText);                                         \
    }
