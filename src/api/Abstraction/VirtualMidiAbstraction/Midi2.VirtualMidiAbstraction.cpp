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
CMidi2VirtualMidiAbstraction::Activate(
    REFIID iid,
    void **activatedInterface
)
{
    RETURN_HR_IF(E_INVALIDARG, nullptr == activatedInterface);

    if (__uuidof(IMidiBiDi) == iid)
    {
        TraceLoggingWrite(
            MidiVirtualMidiAbstractionTelemetryProvider::Provider(),
            MIDI_TRACE_EVENT_INFO,
            TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
            TraceLoggingLevel(WINEVENT_LEVEL_INFO),
            TraceLoggingPointer(this, "this"),
            TraceLoggingWideString(L"IMidiBiDi", MIDI_TRACE_EVENT_INTERFACE_FIELD)
            );


        wil::com_ptr_nothrow<IMidiBiDi> midiBiDi;
        RETURN_IF_FAILED(Microsoft::WRL::MakeAndInitialize<CMidi2VirtualMidiBiDi>(&midiBiDi));
        *activatedInterface = midiBiDi.detach();
    }

    else if (__uuidof(IMidiEndpointManager) == iid)
    {
        TraceLoggingWrite(
            MidiVirtualMidiAbstractionTelemetryProvider::Provider(),
            MIDI_TRACE_EVENT_INFO,
            TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
            TraceLoggingLevel(WINEVENT_LEVEL_INFO),
            TraceLoggingPointer(this, "this"),
            TraceLoggingWideString(L"IMidiEndpointManager", MIDI_TRACE_EVENT_INTERFACE_FIELD)

        );

        // check to see if this is the first time we're creating the endpoint manager. If so, create it.
        if (AbstractionState::Current().GetEndpointManager() == nullptr)
        {
            AbstractionState::Current().ConstructEndpointManager();
        }

        RETURN_IF_FAILED(AbstractionState::Current().GetEndpointManager()->QueryInterface(iid, activatedInterface));
    }

    else if (__uuidof(IMidiTransportConfigurationManager) == iid)
    {
        TraceLoggingWrite(
            MidiVirtualMidiAbstractionTelemetryProvider::Provider(),
            MIDI_TRACE_EVENT_INFO,
            TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
            TraceLoggingLevel(WINEVENT_LEVEL_INFO),
            TraceLoggingPointer(this, "this"),
            TraceLoggingWideString(L"IMidiAbstractionConfigurationManager", MIDI_TRACE_EVENT_INTERFACE_FIELD)

        );

        // check to see if this is the first time we're creating the endpoint manager. If so, create it.
        if (AbstractionState::Current().GetConfigurationManager() == nullptr)
        {
            AbstractionState::Current().ConstructConfigurationManager();
        }

        RETURN_IF_FAILED(AbstractionState::Current().GetConfigurationManager()->QueryInterface(iid, activatedInterface));
    }

    else if (__uuidof(IMidiServiceTransportPluginMetadataProvider) == iid)
    {
        TraceLoggingWrite(
            MidiVirtualMidiAbstractionTelemetryProvider::Provider(),
            MIDI_TRACE_EVENT_INFO,
            TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
            TraceLoggingLevel(WINEVENT_LEVEL_INFO),
            TraceLoggingPointer(this, "this"),
            TraceLoggingWideString(L"IMidiServiceAbstractionPluginMetadataProvider", MIDI_TRACE_EVENT_INTERFACE_FIELD)

        );

        wil::com_ptr_nothrow<IMidiServiceTransportPluginMetadataProvider> metadataProvider;
        RETURN_IF_FAILED(Microsoft::WRL::MakeAndInitialize<CMidi2VirtualMidiPluginMetadataProvider>(&metadataProvider));
        *activatedInterface = metadataProvider.detach();
    }

    else
    {
        TraceLoggingWrite(
            MidiVirtualMidiAbstractionTelemetryProvider::Provider(),
            MIDI_TRACE_EVENT_ERROR,
            TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
            TraceLoggingLevel(WINEVENT_LEVEL_ERROR),
            TraceLoggingPointer(this, "this"),
            TraceLoggingWideString(L"(Unknown)", MIDI_TRACE_EVENT_INTERFACE_FIELD)

        );

        RETURN_IF_FAILED(E_NOINTERFACE);
    }

    return S_OK;
}

