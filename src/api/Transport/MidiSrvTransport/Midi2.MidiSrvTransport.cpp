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
CMidi2MidiSrvTransport::Activate(
    REFIID iid,
    void **activatedInterface
)
{
    RETURN_HR_IF(E_INVALIDARG, nullptr == activatedInterface);

    if (__uuidof(IMidiIn) == iid)
    {
        TraceLoggingWrite(
            MidiSrvTransportTelemetryProvider::Provider(),
            MIDI_TRACE_EVENT_INFO,
            TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
            TraceLoggingLevel(WINEVENT_LEVEL_INFO),
            TraceLoggingPointer(this, "this"),
            TraceLoggingWideString(L"IMidiIn", MIDI_TRACE_EVENT_INTERFACE_FIELD)
            );

        wil::com_ptr_nothrow<IMidiIn> midiIn;
        RETURN_IF_FAILED(Microsoft::WRL::MakeAndInitialize<CMidi2MidiSrvIn>(&midiIn));
        *activatedInterface = midiIn.detach();
    }
    else if (__uuidof(IMidiOut) == iid)
    {
        TraceLoggingWrite(
            MidiSrvTransportTelemetryProvider::Provider(),
            MIDI_TRACE_EVENT_INFO,
            TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
            TraceLoggingLevel(WINEVENT_LEVEL_INFO),
            TraceLoggingPointer(this, "this"),
            TraceLoggingWideString(L"IMidiOut", MIDI_TRACE_EVENT_INTERFACE_FIELD)
            );

        wil::com_ptr_nothrow<IMidiOut> midiOut;
        RETURN_IF_FAILED(Microsoft::WRL::MakeAndInitialize<CMidi2MidiSrvOut>(&midiOut));
        *activatedInterface = midiOut.detach();
    }
    else if (__uuidof(IMidiBidirectional) == iid)
    {
        TraceLoggingWrite(
            MidiSrvTransportTelemetryProvider::Provider(),
            MIDI_TRACE_EVENT_INFO,
            TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
            TraceLoggingLevel(WINEVENT_LEVEL_INFO),
            TraceLoggingPointer(this, "this"),
            TraceLoggingWideString(L"IMidiBidirectional", MIDI_TRACE_EVENT_INTERFACE_FIELD)
            );

        wil::com_ptr_nothrow<IMidiBidirectional> midiBidi;
        RETURN_IF_FAILED(Microsoft::WRL::MakeAndInitialize<CMidi2MidiSrvBidi>(&midiBidi));
        *activatedInterface = midiBidi.detach();
    }
    else if (__uuidof(IMidiTransportConfigurationManager) == iid)
    {
        TraceLoggingWrite(
            MidiSrvTransportTelemetryProvider::Provider(),
            MIDI_TRACE_EVENT_INFO,
            TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
            TraceLoggingLevel(WINEVENT_LEVEL_INFO),
            TraceLoggingPointer(this, "this"),
            TraceLoggingWideString(L"IMidiTransportConfigurationManager", MIDI_TRACE_EVENT_INTERFACE_FIELD)
        );

        wil::com_ptr_nothrow<IMidiTransportConfigurationManager> config;
        RETURN_IF_FAILED(Microsoft::WRL::MakeAndInitialize<CMidi2MidiSrvConfigurationManager>(&config));
        *activatedInterface = config.detach();
    }
    
    else if (__uuidof(IMidiServicePluginMetadataReporter) == iid)
    {
        TraceLoggingWrite(
            MidiSrvTransportTelemetryProvider::Provider(),
            MIDI_TRACE_EVENT_INFO,
            TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
            TraceLoggingLevel(WINEVENT_LEVEL_INFO),
            TraceLoggingPointer(this, "this"),
            TraceLoggingWideString(L"IMidiServicePluginMetadataReporter", MIDI_TRACE_EVENT_INTERFACE_FIELD)
        );

        wil::com_ptr_nothrow<IMidiServicePluginMetadataReporter> metadataReporter;
        RETURN_IF_FAILED(Microsoft::WRL::MakeAndInitialize<CMidi2MidiSrvConfigurationManager>(&metadataReporter));  // config manager implements this interface
        *activatedInterface = metadataReporter.detach();
    }


    else if (__uuidof(IMidiSessionTracker) == iid)
    {
        TraceLoggingWrite(
            MidiSrvTransportTelemetryProvider::Provider(),
            MIDI_TRACE_EVENT_INFO,
            TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
            TraceLoggingLevel(WINEVENT_LEVEL_INFO),
            TraceLoggingPointer(this, "this"),
            TraceLoggingWideString(L"IMidiSessionTracker", MIDI_TRACE_EVENT_INTERFACE_FIELD)
        );

        wil::com_ptr_nothrow<IMidiSessionTracker> config;
        RETURN_IF_FAILED(Microsoft::WRL::MakeAndInitialize<CMidi2MidiSrvSessionTracker>(&config));
        *activatedInterface = config.detach();
    }
    else
    {
        TraceLoggingWrite(
            MidiSrvTransportTelemetryProvider::Provider(),
            MIDI_TRACE_EVENT_ERROR,
            TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
            TraceLoggingLevel(WINEVENT_LEVEL_INFO),
            TraceLoggingPointer(this, "this"),
            TraceLoggingWideString(L"Unknown interface requested", MIDI_TRACE_EVENT_MESSAGE_FIELD)
        );
            return E_NOINTERFACE;
    }

    return S_OK;
}

