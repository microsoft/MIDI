// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

class MidiNetworkSetupTelemetryProvider : public wil::TraceLoggingProvider
{
    IMPLEMENT_TRACELOGGING_CLASS_WITH_MICROSOFT_TELEMETRY(
        MidiNetworkSetupTelemetryProvider,
        "Microsoft.Windows.Midi2.NetworkSetup",
        //  PS> [System.Diagnostics.Tracing.EventSource]::new("Microsoft.Windows.Midi2.NetworkSetup").Guid
        // {8d258c25-e7b4-5d73-ec89-d1532de50566}
        (0x8d258c25, 0xe7b4, 0x5d73, 0xec, 0x89, 0xd1, 0x53, 0x2d, 0xe5, 0x05, 0x66))
};

#define MIDI_NETSETUP_TRACE_EVENT_ERROR                  "MidiNetworkSetup.Error"
#define MIDI_NETSETUP_TRACE_EVENT_WARNING                "MidiNetworkSetup.Warning"
#define MIDI_NETSETUP_TRACE_EVENT_INFO                   "MidiNetworkSetup.Info"

#define MIDI_NETSETUP_TRACE_LOCATION_FIELD               "location"
#define MIDI_NETSETUP_TRACE_MESSAGE_FIELD                "message"
#define MIDI_NETSETUP_TRACE_HRESULT_FIELD                "hresult"
#define MIDI_NETSETUP_TRACE_ERROR_FIELD                  "error"
#define MIDI_NETSETUP_TRACE_ENDPOINT_DEVICE_ID_FIELD     "endpoint id"

#define MIDI_NETSETUP_LOG_INFO(messageText)                                                       \
    TraceLoggingWrite(                                                                           \
        MidiNetworkSetupTelemetryProvider::Provider(),                                               \
        MIDI_NETSETUP_TRACE_EVENT_INFO,                                                           \
        TraceLoggingString(__FUNCTION__, MIDI_NETSETUP_TRACE_LOCATION_FIELD),                     \
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),                                                  \
        TraceLoggingWideString((messageText), MIDI_NETSETUP_TRACE_MESSAGE_FIELD)                  \
    )

#define MIDI_NETSETUP_LOG_INFO_WITH_ENDPOINT(messageText, endpointId)                             \
    TraceLoggingWrite(                                                                           \
        MidiNetworkSetupTelemetryProvider::Provider(),                                               \
        MIDI_NETSETUP_TRACE_EVENT_INFO,                                                           \
        TraceLoggingString(__FUNCTION__, MIDI_NETSETUP_TRACE_LOCATION_FIELD),                     \
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),                                                  \
        TraceLoggingWideString((messageText), MIDI_NETSETUP_TRACE_MESSAGE_FIELD),                 \
        TraceLoggingWideString((endpointId), MIDI_NETSETUP_TRACE_ENDPOINT_DEVICE_ID_FIELD)        \
    )

#define MIDI_NETSETUP_LOG_HRESULT_EXCEPTION(ex, messageText)                                      \
    LOG_IF_FAILED(static_cast<HRESULT>((ex).code()));                                            \
    TraceLoggingWrite(                                                                           \
        MidiNetworkSetupTelemetryProvider::Provider(),                                               \
        MIDI_NETSETUP_TRACE_EVENT_ERROR,                                                          \
        TraceLoggingString(__FUNCTION__, MIDI_NETSETUP_TRACE_LOCATION_FIELD),                     \
        TraceLoggingLevel(WINEVENT_LEVEL_ERROR),                                                 \
        TraceLoggingWideString((messageText), MIDI_NETSETUP_TRACE_MESSAGE_FIELD),                 \
        TraceLoggingHResult(static_cast<HRESULT>((ex).code()), MIDI_NETSETUP_TRACE_HRESULT_FIELD),\
        TraceLoggingWideString((ex).message().c_str(), MIDI_NETSETUP_TRACE_ERROR_FIELD)           \
    )

#define MIDI_NETSETUP_LOG_GENERAL_EXCEPTION(messageText)                                          \
    LOG_IF_FAILED(E_FAIL);                                                                       \
    TraceLoggingWrite(                                                                           \
        MidiNetworkSetupTelemetryProvider::Provider(),                                               \
        MIDI_NETSETUP_TRACE_EVENT_ERROR,                                                          \
        TraceLoggingString(__FUNCTION__, MIDI_NETSETUP_TRACE_LOCATION_FIELD),                     \
        TraceLoggingLevel(WINEVENT_LEVEL_ERROR),                                                 \
        TraceLoggingWideString((messageText), MIDI_NETSETUP_TRACE_MESSAGE_FIELD)                  \
    )

// Every public entry point that can be reached from XAML or from a MIDI callback is wrapped
// so that an escaping exception can never terminate the process.
#define MIDI_NETSETUP_CATCH_AND_LOG(messageText)                                                  \
    catch (winrt::hresult_error const& ex)                                                       \
    {                                                                                            \
        MIDI_NETSETUP_LOG_HRESULT_EXCEPTION(ex, messageText);                                     \
    }                                                                                            \
    catch (...)                                                                                  \
    {                                                                                            \
        MIDI_NETSETUP_LOG_GENERAL_EXCEPTION(messageText);                                         \
    }
