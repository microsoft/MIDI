// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================


#include "pch.h"

_Use_decl_annotations_
HRESULT
CMidi2DiagnosticsAbstraction::Activate(
    REFIID Riid,
    void **Interface
)
{
    RETURN_HR_IF(E_INVALIDARG, nullptr == Interface);

    if (__uuidof(IMidiBiDi) == Riid)
    {
        TraceLoggingWrite(
            MidiDiagnosticsAbstractionTelemetryProvider::Provider(),
            MIDI_TRACE_EVENT_INFO,
            TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
            TraceLoggingLevel(WINEVENT_LEVEL_INFO),
            TraceLoggingPointer(this, "this"),
            TraceLoggingWideString(L"IMidiBiDi", MIDI_TRACE_EVENT_INTERFACE_FIELD)
        );

        wil::com_ptr_nothrow<IMidiBiDi> midiBiDi;
        RETURN_IF_FAILED(Microsoft::WRL::MakeAndInitialize<CMidi2LoopbackMidiBiDi>(&midiBiDi));
        *Interface = midiBiDi.detach();
    }
    else if (__uuidof(IMidiEndpointManager) == Riid)
    {
        TraceLoggingWrite(
            MidiDiagnosticsAbstractionTelemetryProvider::Provider(),
            MIDI_TRACE_EVENT_INFO,
            TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
            TraceLoggingLevel(WINEVENT_LEVEL_INFO),
            TraceLoggingPointer(this, "this"),
            TraceLoggingWideString(L"IMidiEndpointManager", MIDI_TRACE_EVENT_INTERFACE_FIELD)

        );

        wil::com_ptr_nothrow<IMidiEndpointManager> midiEndpointManager;
        RETURN_IF_FAILED(Microsoft::WRL::MakeAndInitialize<CMidi2DiagnosticsEndpointManager>(&midiEndpointManager));
        *Interface = midiEndpointManager.detach();
    }
    else if (__uuidof(IMidiAbstractionConfigurationManager) == Riid)
    {
        TraceLoggingWrite(
            MidiDiagnosticsAbstractionTelemetryProvider::Provider(),
            MIDI_TRACE_EVENT_INFO,
            TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
            TraceLoggingLevel(WINEVENT_LEVEL_INFO),
            TraceLoggingPointer(this, "this"),
            TraceLoggingWideString(L"IMidiAbstractionConfigurationManager", MIDI_TRACE_EVENT_INTERFACE_FIELD)

        );

        wil::com_ptr_nothrow<IMidiAbstractionConfigurationManager> midiConfigurationManager;
        RETURN_IF_FAILED(Microsoft::WRL::MakeAndInitialize<CMidi2DiagnosticsMidiConfigurationManager>(&midiConfigurationManager));
        *Interface = midiConfigurationManager.detach();
    }

    else if (__uuidof(IMidiServiceAbstractionPluginMetadataProvider) == Riid)
    {
        TraceLoggingWrite(
            MidiDiagnosticsAbstractionTelemetryProvider::Provider(),
            MIDI_TRACE_EVENT_INFO,
            TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
            TraceLoggingLevel(WINEVENT_LEVEL_INFO),
            TraceLoggingPointer(this, "this"),
            TraceLoggingWideString(L"IMidiServiceAbstractionPluginMetadataProvider", MIDI_TRACE_EVENT_INTERFACE_FIELD)

        );

        wil::com_ptr_nothrow<IMidiServiceAbstractionPluginMetadataProvider> metadataProvider;
        RETURN_IF_FAILED(Microsoft::WRL::MakeAndInitialize<CMidi2DiagnosticsPluginMetadataProvider>(&metadataProvider));
        *Interface = metadataProvider.detach();
    }

    else
    {
        TraceLoggingWrite(
            MidiDiagnosticsAbstractionTelemetryProvider::Provider(),
            MIDI_TRACE_EVENT_ERROR,
            TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
            TraceLoggingLevel(WINEVENT_LEVEL_INFO),
            TraceLoggingPointer(this, "this")
        );

        return E_NOINTERFACE;
    }

    return S_OK;
}

