// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// The transport's COM class.
// ============================================================================

#include "pch.h"

_Use_decl_annotations_
HRESULT
CMidi2RtpMidiTransport::Activate(
    REFIID riid,
    void** requestedInterface)
{
    RETURN_HR_IF_NULL(E_INVALIDARG, requestedInterface);
    *requestedInterface = nullptr;

    TraceLoggingWrite(
        MidiRtpMidiTransportTelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_INFO,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this"),
        TraceLoggingGuid(riid, MIDI_TRACE_EVENT_INTERFACE_FIELD)
    );

    try
    {
        if (__uuidof(IMidiBidirectional) == riid)
        {
            wil::com_ptr_nothrow<IMidiBidirectional> bidi;
            RETURN_IF_FAILED(Microsoft::WRL::MakeAndInitialize<CMidi2RtpMidiBidi>(&bidi));
            *requestedInterface = bidi.detach();
        }
        else if (__uuidof(IMidiEndpointManager) == riid)
        {
            RETURN_IF_FAILED(TransportState::Current().ConstructEndpointManager());

            auto const endpointManager = TransportState::Current().GetEndpointManager();
            RETURN_HR_IF_NULL(E_UNEXPECTED, endpointManager);
            RETURN_IF_FAILED(endpointManager->QueryInterface(riid, requestedInterface));
        }
        else if (__uuidof(IMidiTransportConfigurationManager) == riid)
        {
            RETURN_IF_FAILED(TransportState::Current().ConstructConfigurationManager());

            auto const configurationManager = TransportState::Current().GetConfigurationManager();
            RETURN_HR_IF_NULL(E_UNEXPECTED, configurationManager);
            RETURN_IF_FAILED(configurationManager->QueryInterface(riid, requestedInterface));
        }
        else if (__uuidof(IMidiServiceTransportPluginMetadataProvider) == riid)
        {
            wil::com_ptr_nothrow<IMidiServiceTransportPluginMetadataProvider> metadataProvider;
            RETURN_IF_FAILED(Microsoft::WRL::MakeAndInitialize<CMidi2RtpMidiPluginMetadataProvider>(&metadataProvider));
            *requestedInterface = metadataProvider.detach();
        }
        else
        {
            return E_NOINTERFACE;
        }

        return S_OK;
    }
    CATCH_RETURN();
}
