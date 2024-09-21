// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://github.com/microsoft/MIDI/
// ============================================================================


#include "pch.h"

_Use_decl_annotations_
HRESULT
CMidi2NetworkMidiAbstraction::Activate(
    REFIID Riid,
    void **Interface
)
{
    RETURN_HR_IF(E_INVALIDARG, nullptr == Interface);

    if (__uuidof(IMidiBiDi) == Riid)
    {
        TraceLoggingWrite(
            MidiNetworkMidiAbstractionTelemetryProvider::Provider(),
            MIDI_TRACE_EVENT_INFO,
            TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
            TraceLoggingPointer(this, "this"),
            TraceLoggingWideString(L"IMidiBiDi", MIDI_TRACE_EVENT_INTERFACE_FIELD)
        );

        wil::com_ptr_nothrow<IMidiBiDi> midiBiDi;
        RETURN_IF_FAILED(Microsoft::WRL::MakeAndInitialize<CMidi2NetworkMidiBiDi>(&midiBiDi));
        *Interface = midiBiDi.detach();
    }


    else if (__uuidof(IMidiEndpointManager) == Riid)
    {
        TraceLoggingWrite(
            MidiNetworkMidiAbstractionTelemetryProvider::Provider(),
            MIDI_TRACE_EVENT_INFO,
            TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
            TraceLoggingPointer(this, "this"),
            TraceLoggingWideString(L"IMidiEndpointManager", MIDI_TRACE_EVENT_INTERFACE_FIELD)
        );

        // check to see if this is the first time we're creating the endpoint manager. If so, create it.
        if (AbstractionState::Current().GetEndpointManager() == nullptr)
        {
            AbstractionState::Current().ConstructEndpointManager();
        }

        RETURN_IF_FAILED(AbstractionState::Current().GetEndpointManager()->QueryInterface(Riid, Interface));
    }


    else if (__uuidof(IMidiAbstractionConfigurationManager) == Riid)
    {
        TraceLoggingWrite(
            MidiNetworkMidiAbstractionTelemetryProvider::Provider(),
            MIDI_TRACE_EVENT_INFO,
            TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
            TraceLoggingPointer(this, "this"),
            TraceLoggingWideString(L"IMidiAbstractionConfigurationManager", MIDI_TRACE_EVENT_INTERFACE_FIELD)
        );

        // check to see if this is the first time we're creating the configuration manager. If so, create it.
        if (AbstractionState::Current().GetConfigurationManager() == nullptr)
        {
            AbstractionState::Current().ConstructConfigurationManager();
        }

        RETURN_IF_FAILED(AbstractionState::Current().GetConfigurationManager()->QueryInterface(Riid, Interface));
    }

    else if (__uuidof(IMidiServiceAbstractionPluginMetadataProvider) == Riid)
    {
        TraceLoggingWrite(
            MidiNetworkMidiAbstractionTelemetryProvider::Provider(),
            MIDI_TRACE_EVENT_INFO,
            TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
            TraceLoggingPointer(this, "this"),
            TraceLoggingWideString(L"IMidiServiceAbstractionPluginMetadataProvider", MIDI_TRACE_EVENT_INTERFACE_FIELD)
        );

        wil::com_ptr_nothrow<IMidiServiceAbstractionPluginMetadataProvider> metadataProvider;
        RETURN_IF_FAILED(Microsoft::WRL::MakeAndInitialize<CMidi2NetworkMidiPluginMetadataProvider>(&metadataProvider));
        *Interface = metadataProvider.detach();
    }

    else
    {
        TraceLoggingWrite(
            MidiNetworkMidiAbstractionTelemetryProvider::Provider(),
            MIDI_TRACE_EVENT_INFO,
            TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
            TraceLoggingPointer(this, "this"),
            TraceLoggingWideString(L"Unknown Interface", MIDI_TRACE_EVENT_INTERFACE_FIELD)
        );

        return E_NOINTERFACE;
    }

    return S_OK;
}

