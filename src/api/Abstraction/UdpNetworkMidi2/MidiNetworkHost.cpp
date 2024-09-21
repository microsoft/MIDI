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
MidiNetworkHost::Initialize(
    MidiNetworkUdpHostDefinition& hostDefinition
)
{
    TraceLoggingWrite(
        MidiNetworkMidiAbstractionTelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_INFO,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingPointer(this, "this"),
        TraceLoggingWideString(L"Enter", MIDI_TRACE_EVENT_MESSAGE_FIELD)
    );

    RETURN_HR_IF(E_INVALIDARG, hostDefinition.HostName.empty());
    RETURN_HR_IF(E_INVALIDARG, hostDefinition.ProductInstanceId.empty());
    RETURN_HR_IF(E_INVALIDARG, hostDefinition.ServiceInstanceName.empty());
    RETURN_HR_IF(E_INVALIDARG, hostDefinition.UmpEndpointName.empty());

    RETURN_HR_IF(E_INVALIDARG, hostDefinition.UmpEndpointName.size() > 98);
    RETURN_HR_IF(E_INVALIDARG, hostDefinition.ProductInstanceId.size() > 42);


    HostName hostName(hostDefinition.HostName);

    DnssdServiceInstance instance(
        hostDefinition.ServiceInstanceName,
        hostName,
        hostDefinition.Port);

    // add the txt attributes per the spec
    instance.TextAttributes().Insert(L"UMPEndpointName", hostDefinition.UmpEndpointName);
    instance.TextAttributes().Insert(L"ProductInstanceId", hostDefinition.ProductInstanceId);


    // TODO: Should we allow binding to a specific network adapter?
    auto result = instance.RegisterDatagramSocketAsync(m_socket).get();

    switch (result.Status())
    {
        case DnssdRegistrationStatus::Success:
            return S_OK;

        // The service was not registered because security settings did not allow it.
        case DnssdRegistrationStatus::SecurityError:
            RETURN_IF_FAILED(E_ACCESSDENIED);
            break;

        // The service was not registered because the service name provided is not valid.
        case DnssdRegistrationStatus::InvalidServiceName:
            RETURN_IF_FAILED(E_INVALIDARG);
            break;

        // The service was not registered because of an error on the DNS server.
        case DnssdRegistrationStatus::ServerError:
            RETURN_IF_FAILED(E_FAIL);
            break;

        default:
            RETURN_IF_FAILED(E_FAIL);
    }


}


HRESULT 
MidiNetworkHost::ProcessIncomingPacket()
{


    return S_OK;
}

HRESULT 
MidiNetworkHost::EstablishNewSession()
{
    TraceLoggingWrite(
        MidiNetworkMidiAbstractionTelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_INFO,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingPointer(this, "this"),
        TraceLoggingWideString(L"Enter", MIDI_TRACE_EVENT_MESSAGE_FIELD)
    );

    return S_OK;
}


HRESULT 
MidiNetworkHost::Cleanup()
{
    TraceLoggingWrite(
        MidiNetworkMidiAbstractionTelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_INFO,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingPointer(this, "this"),
        TraceLoggingWideString(L"Enter", MIDI_TRACE_EVENT_MESSAGE_FIELD)
    );

    return S_OK;
}