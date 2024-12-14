// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://github.com/microsoft/MIDI/
// ============================================================================

#include "pch.h"

HRESULT 
MidiNetworkAdvertiser::Initialize()
{
    TraceLoggingWrite(
        MidiNetworkMidiTransportTelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_INFO,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this"),
        TraceLoggingWideString(L"Enter", MIDI_TRACE_EVENT_MESSAGE_FIELD)
    );




    return S_OK;
}



inline const winrt::hstring BuildFullServiceInstanceName(_In_ winrt::hstring const& nameWithoutSuffix)
{
    return nameWithoutSuffix + L"." + DNS_PTR_SERVICE_TYPE;
}




_Use_decl_annotations_
HRESULT 
MidiNetworkAdvertiser::Advertise(
    winrt::hstring const& serviceInstanceNameWithoutSuffix,
    HostName const& hostName,
    DatagramSocket const& boundSocket,
    uint16_t const port,
    winrt::hstring const& midiEndpointName,
    winrt::hstring const& midiProductInstanceId

)
{
    TraceLoggingWrite(
        MidiNetworkMidiTransportTelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_INFO,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this"),
        TraceLoggingWideString(L"Enter", MIDI_TRACE_EVENT_MESSAGE_FIELD)
    );

    auto fullServiceName = BuildFullServiceInstanceName(serviceInstanceNameWithoutSuffix);


    auto service = DnssdServiceInstance(
        fullServiceName,
        hostName,
        port);

    // add the txt attributes per the spec
    service.TextAttributes().Insert(L"UMPEndpointName", midiEndpointName);
    service.TextAttributes().Insert(L"ProductInstanceId", midiProductInstanceId);

    // register with the socket that's bound to the port
    auto registration = service.RegisterDatagramSocketAsync(boundSocket).get();

    switch (registration.Status())
    {
    case DnssdRegistrationStatus::Success:
        return S_OK;

        // The service was not registered because security settings did not allow it.
    case DnssdRegistrationStatus::SecurityError:
        TraceLoggingWrite(
            MidiNetworkMidiTransportTelemetryProvider::Provider(),
            MIDI_TRACE_EVENT_ERROR,
            TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
            TraceLoggingLevel(WINEVENT_LEVEL_ERROR),
            TraceLoggingPointer(this, "this"),
            TraceLoggingWideString(L"Unable to register datagram socket. Security settings did not allow it", MIDI_TRACE_EVENT_MESSAGE_FIELD)
        );

        RETURN_IF_FAILED(E_ACCESSDENIED);
        break;

        // The service was not registered because the service name provided is not valid.
    case DnssdRegistrationStatus::InvalidServiceName:
        TraceLoggingWrite(
            MidiNetworkMidiTransportTelemetryProvider::Provider(),
            MIDI_TRACE_EVENT_ERROR,
            TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
            TraceLoggingLevel(WINEVENT_LEVEL_ERROR),
            TraceLoggingPointer(this, "this"),
            TraceLoggingWideString(L"Unable to register datagram socket. Invalid service name", MIDI_TRACE_EVENT_MESSAGE_FIELD)
        );

        RETURN_IF_FAILED(E_INVALIDARG);
        break;

        // The service was not registered because of an error on the DNS server.
    case DnssdRegistrationStatus::ServerError:
        TraceLoggingWrite(
            MidiNetworkMidiTransportTelemetryProvider::Provider(),
            MIDI_TRACE_EVENT_INFO,
            TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
            TraceLoggingPointer(this, "this"),
            TraceLoggingWideString(L"Unable to register datagram socket. Server error", MIDI_TRACE_EVENT_MESSAGE_FIELD)
        );

        RETURN_IF_FAILED(E_FAIL);
        break;

    default:
        TraceLoggingWrite(
            MidiNetworkMidiTransportTelemetryProvider::Provider(),
            MIDI_TRACE_EVENT_ERROR,
            TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
            TraceLoggingLevel(WINEVENT_LEVEL_ERROR),
            TraceLoggingPointer(this, "this"),
            TraceLoggingWideString(L"Unable to register datagram socket for unknown reason", MIDI_TRACE_EVENT_MESSAGE_FIELD)
        );


        RETURN_IF_FAILED(E_FAIL);
    }
}



HRESULT 
MidiNetworkAdvertiser::Shutdown()
{
    TraceLoggingWrite(
        MidiNetworkMidiTransportTelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_INFO,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this"),
        TraceLoggingWideString(L"Enter", MIDI_TRACE_EVENT_MESSAGE_FIELD)
    );




    return S_OK;
}
