// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

#include "pch.h"


_Use_decl_annotations_
bool MidiSrvManager::IsServiceInstalled()
{
    wil::unique_schandle serviceControlManager;
    serviceControlManager.reset(OpenSCManager(NULL, NULL, GENERIC_READ));

    wil::unique_schandle service;
    service.reset(
        OpenService(serviceControlManager.get(), MIDI_SERVICE_NAME, GENERIC_READ)
    );

    if (service)
    {
        TraceLoggingWrite(
            MidiSrvTransportTelemetryProvider::Provider(),
            MIDI_TRACE_EVENT_INFO,
            TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
            TraceLoggingLevel(WINEVENT_LEVEL_INFO),
            TraceLoggingWideString(L"static", "this"),
            TraceLoggingWideString(L"MidiSrv is installed", MIDI_TRACE_EVENT_MESSAGE_FIELD)
        );

        return true;
    }
    else
    {
        TraceLoggingWrite(
            MidiSrvTransportTelemetryProvider::Provider(),
            MIDI_TRACE_EVENT_ERROR,
            TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
            TraceLoggingLevel(WINEVENT_LEVEL_ERROR),
            TraceLoggingWideString(L"static", "this"),
            TraceLoggingWideString(L"MidiSrv is NOT installed", MIDI_TRACE_EVENT_MESSAGE_FIELD)
        );

        return false;
    }
}

