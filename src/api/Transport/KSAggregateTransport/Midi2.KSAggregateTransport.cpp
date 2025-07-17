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
CMidi2KSAggregateTransport::Activate(
    REFIID iid,
    void **activatedInterface
)
{
    RETURN_HR_IF(E_INVALIDARG, nullptr == activatedInterface);

    if (__uuidof(IMidiBidirectional) == iid)
    {
        TraceLoggingWrite(
            MidiKSAggregateTransportTelemetryProvider::Provider(),
            MIDI_TRACE_EVENT_INFO,
            TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
            TraceLoggingLevel(WINEVENT_LEVEL_INFO),
            TraceLoggingPointer(this, "this"),
            TraceLoggingWideString(L"IMidiBidirectional", MIDI_TRACE_EVENT_INTERFACE_FIELD)
        );

        wil::com_ptr_nothrow<IMidiBidirectional> midiBidi;
        RETURN_IF_FAILED(Microsoft::WRL::MakeAndInitialize<CMidi2KSAggregateMidiBidi>(&midiBidi));
        *activatedInterface = midiBidi.detach();
    }
    else if (__uuidof(IMidiEndpointManager) == iid)
    {
        TraceLoggingWrite(
            MidiKSAggregateTransportTelemetryProvider::Provider(),
            MIDI_TRACE_EVENT_INFO,
            TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
            TraceLoggingLevel(WINEVENT_LEVEL_INFO),
            TraceLoggingPointer(this, "this"),
            TraceLoggingWideString(L"IMidiEndpointManager", MIDI_TRACE_EVENT_INTERFACE_FIELD)
        );


        // check to see if this is the first time we're creating the endpoint manager. If so, create it.
        if (TransportState::Current().GetEndpointManager() == nullptr)
        {
            TransportState::Current().ConstructEndpointManager();
        }

        RETURN_IF_FAILED(TransportState::Current().GetEndpointManager()->QueryInterface(iid, activatedInterface));
    }
    else if (__uuidof(IMidiTransportConfigurationManager) == iid)
    {
        TraceLoggingWrite(
            MidiKSAggregateTransportTelemetryProvider::Provider(),
            MIDI_TRACE_EVENT_INFO,
            TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
            TraceLoggingLevel(WINEVENT_LEVEL_INFO),
            TraceLoggingPointer(this, "this"),
            TraceLoggingWideString(L"IMidiTransportConfigurationManager", MIDI_TRACE_EVENT_INTERFACE_FIELD)
        );

        // check to see if this is the first time we're creating the endpoint manager. If so, create it.
        if (TransportState::Current().GetConfigurationManager() == nullptr)
        {
            TransportState::Current().ConstructConfigurationManager();
        }

        RETURN_IF_FAILED(TransportState::Current().GetConfigurationManager()->QueryInterface(iid, activatedInterface));
    }

    else if (__uuidof(IMidiServiceTransportPluginMetadataProvider) == iid)
    {
        TraceLoggingWrite(
            MidiKSAggregateTransportTelemetryProvider::Provider(),
            MIDI_TRACE_EVENT_INFO,
            TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
            TraceLoggingLevel(WINEVENT_LEVEL_INFO),
            TraceLoggingPointer(this, "this"),
            TraceLoggingWideString(L"IMidiServiceTransportPluginMetadataProvider", MIDI_TRACE_EVENT_INTERFACE_FIELD)
        );

        wil::com_ptr_nothrow<IMidiServiceTransportPluginMetadataProvider> metadataProvider;
        RETURN_IF_FAILED(Microsoft::WRL::MakeAndInitialize<CMidi2KSAggregateMidiPluginMetadataProvider>(&metadataProvider));
        *activatedInterface = metadataProvider.detach();
    }

    else
    {
        return E_NOINTERFACE;
    }

    return S_OK;
}

