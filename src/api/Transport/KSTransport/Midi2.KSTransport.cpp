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
CMidi2KSTransport::Activate(
    REFIID riid,
    void **activatedInterface
)
{
    RETURN_HR_IF(E_INVALIDARG, nullptr == activatedInterface);

    if (__uuidof(IMidiIn) == riid)
    {
        TraceLoggingWrite(
            MidiKSTransportTelemetryProvider::Provider(),
            MIDI_TRACE_EVENT_INFO,
            TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
            TraceLoggingLevel(WINEVENT_LEVEL_INFO),
            TraceLoggingPointer(this, "this"),
            TraceLoggingWideString(L"IMidiIn", MIDI_TRACE_EVENT_INTERFACE_FIELD)
        );

        wil::com_ptr_nothrow<IMidiIn> midiIn;
        RETURN_IF_FAILED(Microsoft::WRL::MakeAndInitialize<CMidi2KSMidiIn>(&midiIn));
        *activatedInterface = midiIn.detach();
    }
    else if (__uuidof(IMidiOut) == riid)
    {
        TraceLoggingWrite(
            MidiKSTransportTelemetryProvider::Provider(),
            MIDI_TRACE_EVENT_INFO,
            TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
            TraceLoggingLevel(WINEVENT_LEVEL_INFO),
            TraceLoggingPointer(this, "this"),
            TraceLoggingWideString(L"IMidiOut", MIDI_TRACE_EVENT_INTERFACE_FIELD)
        );

        wil::com_ptr_nothrow<IMidiOut> midiOut;
        RETURN_IF_FAILED(Microsoft::WRL::MakeAndInitialize<CMidi2KSMidiOut>(&midiOut));
        *activatedInterface = midiOut.detach();
    }
    else if (__uuidof(IMidiBidirectional) == riid)
    {
        TraceLoggingWrite(
            MidiKSTransportTelemetryProvider::Provider(),
            MIDI_TRACE_EVENT_INFO,
            TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
            TraceLoggingLevel(WINEVENT_LEVEL_INFO),
            TraceLoggingPointer(this, "this"),
            TraceLoggingWideString(L"IMidiBidirectional", MIDI_TRACE_EVENT_INTERFACE_FIELD)
        );

        wil::com_ptr_nothrow<IMidiBidirectional> midiBidi;
        RETURN_IF_FAILED(Microsoft::WRL::MakeAndInitialize<CMidi2KSMidiBidi>(&midiBidi));
        *activatedInterface = midiBidi.detach();
    }
    else if (__uuidof(IMidiEndpointManager) == riid)
    {
        TraceLoggingWrite(
            MidiKSTransportTelemetryProvider::Provider(),
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

        RETURN_IF_FAILED(TransportState::Current().GetEndpointManager()->QueryInterface(riid, activatedInterface));
    }
    else if (__uuidof(IMidiTransportConfigurationManager) == riid)
    {
        TraceLoggingWrite(
            MidiKSTransportTelemetryProvider::Provider(),
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

        RETURN_IF_FAILED(TransportState::Current().GetConfigurationManager()->QueryInterface(riid, activatedInterface));
    }

    else if (__uuidof(IMidiServiceTransportPluginMetadataProvider) == riid)
    {
        TraceLoggingWrite(
            MidiKSTransportTelemetryProvider::Provider(),
            MIDI_TRACE_EVENT_INFO,
            TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
            TraceLoggingLevel(WINEVENT_LEVEL_INFO),
            TraceLoggingPointer(this, "this"),
            TraceLoggingWideString(L"IMidiServiceTransportPluginMetadataProvider", MIDI_TRACE_EVENT_INTERFACE_FIELD)
        );

        wil::com_ptr_nothrow<IMidiServiceTransportPluginMetadataProvider> metadataProvider;
        RETURN_IF_FAILED(Microsoft::WRL::MakeAndInitialize<CMidi2KSMidiPluginMetadataProvider>(&metadataProvider));
        *activatedInterface = metadataProvider.detach();
    }

    else
    {
        return E_NOINTERFACE;
    }

    return S_OK;
}

