// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://github.com/microsoft/MIDI/
// ============================================================================

#pragma once

#include "pch.h"

_Use_decl_annotations_
HRESULT
CMidi2DiagnosticsMidiConfigurationManager::Initialize(
    GUID AbstractionId,
    IUnknown* MidiDeviceManager,
    IUnknown* MidiServiceConfigurationManagerInterface
)
{
    UNREFERENCED_PARAMETER(MidiServiceConfigurationManagerInterface);
    UNREFERENCED_PARAMETER(AbstractionId);


    TraceLoggingWrite(
        MidiDiagnosticsAbstractionTelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_INFO,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this")
    );

    RETURN_HR_IF_NULL(E_INVALIDARG, MidiDeviceManager);
    RETURN_IF_FAILED(MidiDeviceManager->QueryInterface(__uuidof(IMidiDeviceManagerInterface), (void**)&m_MidiDeviceManager));

    return S_OK;
}

_Use_decl_annotations_
HRESULT
CMidi2DiagnosticsMidiConfigurationManager::UpdateConfiguration(
    LPCWSTR ConfigurationJsonSection,
    BOOL IsFromConfigurationFile,
    BSTR* Response)
{
    TraceLoggingWrite(
        MidiDiagnosticsAbstractionTelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_INFO,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this")
    );


    UNREFERENCED_PARAMETER(ConfigurationJsonSection);
    UNREFERENCED_PARAMETER(IsFromConfigurationFile);
    UNREFERENCED_PARAMETER(Response);

    return E_NOTIMPL;
}

HRESULT
CMidi2DiagnosticsMidiConfigurationManager::Cleanup()
{
    TraceLoggingWrite(
        MidiDiagnosticsAbstractionTelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_INFO,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this")
    );


    return S_OK;
}