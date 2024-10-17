// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

#include "pch.h"

_Use_decl_annotations_
HRESULT
CMidi2DiagnosticsMidiConfigurationManager::Initialize(
    GUID transportId,
    IMidiDeviceManagerInterface* midiDeviceManager,
    IMidiServiceConfigurationManagerInterface* midiServiceConfigurationManagerInterface
)
{
    UNREFERENCED_PARAMETER(midiServiceConfigurationManagerInterface);
    UNREFERENCED_PARAMETER(transportId);


    TraceLoggingWrite(
        MidiDiagnosticsAbstractionTelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_INFO,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this")
    );

    RETURN_HR_IF_NULL(E_INVALIDARG, midiDeviceManager);
    RETURN_IF_FAILED(midiDeviceManager->QueryInterface(__uuidof(IMidiDeviceManagerInterface), (void**)&m_midiDeviceManager));

    return S_OK;
}

_Use_decl_annotations_
HRESULT
CMidi2DiagnosticsMidiConfigurationManager::UpdateConfiguration(
    LPCWSTR configurationJsonSection,
    LPWSTR* response)
{
    TraceLoggingWrite(
        MidiDiagnosticsAbstractionTelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_INFO,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this")
    );


    UNREFERENCED_PARAMETER(configurationJsonSection);
    UNREFERENCED_PARAMETER(response);

    return E_NOTIMPL;
}

HRESULT
CMidi2DiagnosticsMidiConfigurationManager::Shutdown()
{
    TraceLoggingWrite(
        MidiDiagnosticsAbstractionTelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_INFO,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this")
    );

    m_midiDeviceManager.reset();

    return S_OK;
}