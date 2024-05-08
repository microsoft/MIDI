// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://github.com/microsoft/MIDI/
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
            MidiSrvAbstractionTelemetryProvider::Provider(),
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
            MidiSrvAbstractionTelemetryProvider::Provider(),
            MIDI_TRACE_EVENT_ERROR,
            TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
            TraceLoggingLevel(WINEVENT_LEVEL_ERROR),
            TraceLoggingWideString(L"static", "this"),
            TraceLoggingWideString(L"MidiSrv is NOT installed", MIDI_TRACE_EVENT_MESSAGE_FIELD)
        );

        return false;
    }
}

_Use_decl_annotations_
bool MidiSrvManager::AttemptToStartService()
{
    TraceLoggingWrite(
        MidiSrvAbstractionTelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_INFO,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingWideString(L"static", "this"),
        TraceLoggingWideString(L"Attempting to trigger service start", MIDI_TRACE_EVENT_MESSAGE_FIELD)
    );

    // Service start trigger in this case is the dedicated ETW event provider
    // This is set up in the service install code

    TraceLoggingWrite(
        MidiSrvDemandStartEtwTrigger::Provider(),
        MIDI_TRACE_EVENT_INFO,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingWideString(L"static", "this"),
        TraceLoggingWideString(L"Event to trigger start of MidiSrv if it is not already running.")
    );

    wil::unique_schandle serviceControlManager;
    serviceControlManager.reset(OpenSCManager(NULL, NULL, GENERIC_READ));

    wil::unique_schandle service;
    service.reset(
        OpenService(serviceControlManager.get(), MIDI_SERVICE_NAME, GENERIC_READ)
    );

    if (!service)
    {
        TraceLoggingWrite(
            MidiSrvAbstractionTelemetryProvider::Provider(),
            MIDI_TRACE_EVENT_ERROR,
            TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
            TraceLoggingLevel(WINEVENT_LEVEL_ERROR),
            TraceLoggingWideString(L"static", "this"),
            TraceLoggingWideString(L"Couldn't get service handle", MIDI_TRACE_EVENT_MESSAGE_FIELD)
        );

        return false;
    }

    // TODO: Need to wait here until the service is started

    uint32_t attempts = 0;              // usually only needs 1-2 attempts to start service.
    const uint32_t maxAttempts = 40;    // apx 10 seconds max wait. 

    while (attempts < maxAttempts)
    {
        SERVICE_STATUS status;
        if (QueryServiceStatus(service.get(), &status))
        {
            if (status.dwCurrentState == SERVICE_RUNNING)
            {
                TraceLoggingWrite(
                    MidiSrvAbstractionTelemetryProvider::Provider(),
                    MIDI_TRACE_EVENT_INFO,
                    TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
                    TraceLoggingLevel(WINEVENT_LEVEL_INFO),
                    TraceLoggingWideString(L"static", "this"),
                    TraceLoggingWideString(L"Service status is SERVICE_RUNNING", MIDI_TRACE_EVENT_MESSAGE_FIELD),
                    TraceLoggingUInt32(attempts, "number of attempts")
                );

                return true;
            }
            else
            {
                // sleep for a quarter of a second and try again
                Sleep(250);
            }
        }
        else
        {
            // QueryServiceStatus failed

            auto err = GetLastError();

            TraceLoggingWrite(
                MidiSrvAbstractionTelemetryProvider::Provider(),
                MIDI_TRACE_EVENT_ERROR,
                TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
                TraceLoggingLevel(WINEVENT_LEVEL_ERROR),
                TraceLoggingWideString(L"static", "this"),
                TraceLoggingWideString(L"QueryServiceStatus failed", MIDI_TRACE_EVENT_MESSAGE_FIELD),
                TraceLoggingUInt32(err, "GetLastError"),
                TraceLoggingUInt32(attempts, "number of attempts")
            );

            return false;
        }

        attempts++;
    }

    TraceLoggingWrite(
        MidiSrvAbstractionTelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_ERROR,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_ERROR),
        TraceLoggingWideString(L"static", "this"),
        TraceLoggingWideString(L"Exhausted attempts waiting for service to start", MIDI_TRACE_EVENT_MESSAGE_FIELD)
    );

    return false;
}
