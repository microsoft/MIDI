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
CMidi2NetworkMidiTransport::Activate(
    REFIID riid,
    void **requestedInterface
)
{
    RETURN_HR_IF(E_INVALIDARG, nullptr == requestedInterface);

    if (__uuidof(IMidiBidirectional) == riid)
    {
        TraceLoggingWrite(
            MidiNetworkMidiTransportTelemetryProvider::Provider(),
            MIDI_TRACE_EVENT_INFO,
            TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
            TraceLoggingLevel(WINEVENT_LEVEL_INFO),
            TraceLoggingPointer(this, "this"),
            TraceLoggingWideString(L"IMidiBidirectional", MIDI_TRACE_EVENT_INTERFACE_FIELD)
        );

        wil::com_ptr_nothrow<IMidiBidirectional> midiBidi;
        RETURN_IF_FAILED(Microsoft::WRL::MakeAndInitialize<CMidi2NetworkMidiBidi>(&midiBidi));
        *requestedInterface = midiBidi.detach();
    }


    else if (__uuidof(IMidiEndpointManager) == riid)
    {
        TraceLoggingWrite(
            MidiNetworkMidiTransportTelemetryProvider::Provider(),
            MIDI_TRACE_EVENT_INFO,
            TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
            TraceLoggingLevel(WINEVENT_LEVEL_INFO),
            TraceLoggingPointer(this, "this"),
            TraceLoggingWideString(L"IMidiEndpointManager", MIDI_TRACE_EVENT_INTERFACE_FIELD)
        );

        // check to see if this is the first time we're creating the endpoint manager. If so, create it.
        auto endpointManager = TransportState::Current().GetEndpointManager();

        if (endpointManager == nullptr)
        {
            RETURN_IF_FAILED(TransportState::Current().ConstructEndpointManager());

            endpointManager = TransportState::Current().GetEndpointManager();
        }

        RETURN_HR_IF_NULL(E_UNEXPECTED, endpointManager);
        RETURN_IF_FAILED(endpointManager->QueryInterface(riid, requestedInterface));
    }


    else if (__uuidof(IMidiTransportConfigurationManager) == riid)
    {
        TraceLoggingWrite(
            MidiNetworkMidiTransportTelemetryProvider::Provider(),
            MIDI_TRACE_EVENT_INFO,
            TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
            TraceLoggingLevel(WINEVENT_LEVEL_INFO),
            TraceLoggingPointer(this, "this"),
            TraceLoggingWideString(L"IMidiAbstractionConfigurationManager", MIDI_TRACE_EVENT_INTERFACE_FIELD)
        );

        // check to see if this is the first time we're creating the configuration manager. If so, create it.
        auto configurationManager = TransportState::Current().GetConfigurationManager();

        if (configurationManager == nullptr)
        {
            RETURN_IF_FAILED(TransportState::Current().ConstructConfigurationManager());

            configurationManager = TransportState::Current().GetConfigurationManager();
        }

        RETURN_HR_IF_NULL(E_UNEXPECTED, configurationManager);
        RETURN_IF_FAILED(configurationManager->QueryInterface(riid, requestedInterface));
    }

    else if (__uuidof(IMidiServiceTransportPluginMetadataProvider) == riid)
    {
        TraceLoggingWrite(
            MidiNetworkMidiTransportTelemetryProvider::Provider(),
            MIDI_TRACE_EVENT_INFO,
            TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
            TraceLoggingLevel(WINEVENT_LEVEL_INFO),
            TraceLoggingPointer(this, "this"),
            TraceLoggingWideString(L"IMidiServiceAbstractionPluginMetadataProvider", MIDI_TRACE_EVENT_INTERFACE_FIELD)
        );

        wil::com_ptr_nothrow<IMidiServiceTransportPluginMetadataProvider> metadataProvider;
        RETURN_IF_FAILED(Microsoft::WRL::MakeAndInitialize<CMidi2NetworkMidiPluginMetadataProvider>(&metadataProvider));
        *requestedInterface = metadataProvider.detach();
    }

    else
    {
        TraceLoggingWrite(
            MidiNetworkMidiTransportTelemetryProvider::Provider(),
            MIDI_TRACE_EVENT_INFO,
            TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
            TraceLoggingLevel(WINEVENT_LEVEL_INFO),
            TraceLoggingPointer(this, "this"),
            TraceLoggingWideString(L"Unknown Interface", MIDI_TRACE_EVENT_INTERFACE_FIELD)
        );

        return E_NOINTERFACE;
    }

    return S_OK;
}

