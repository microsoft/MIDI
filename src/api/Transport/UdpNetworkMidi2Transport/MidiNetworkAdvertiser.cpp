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


// TODO: Change this method to "Start" and have the parameters passed into Initialize instead of this.

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

    m_instanceNameWasChanged = false;
    m_actualInstanceNameWithoutSuffix = serviceInstanceNameWithoutSuffix;

    m_serviceInstance = DnssdServiceInstance(
        fullServiceName,
        hostName,
        port);

    // add the txt attributes per the spec
    m_serviceInstance.TextAttributes().Insert(L"UMPEndpointName", midiEndpointName);
    m_serviceInstance.TextAttributes().Insert(L"ProductInstanceId", midiProductInstanceId);

    // register with the socket that's bound to the port
    auto registration = m_serviceInstance.RegisterDatagramSocketAsync(boundSocket).get();

    switch (registration.Status())
    {
    case DnssdRegistrationStatus::Success:
        // The responder renames a colliding instance label rather than refusing it, so the name
        // on the wire is not necessarily the one we asked for. Recorded because everything else
        // reports the configured name, and the two disagreeing is otherwise invisible.
        if (registration.HasInstanceNameChanged())
        {
            m_instanceNameWasChanged = true;

            // The platform updates the instance in place with whatever it settled on. Read back
            // rather than assumed, and the service type suffix trimmed off again so this is the
            // same shape as the configured name.
            std::wstring registered{ };

            try
            {
                registered = m_serviceInstance.DnssdServiceInstanceName();
            }
            CATCH_LOG();

            std::wstring const suffix{ L"." DNS_PTR_SERVICE_TYPE };

            if (registered.length() > suffix.length() &&
                _wcsicmp(registered.c_str() + (registered.length() - suffix.length()), suffix.c_str()) == 0)
            {
                registered.resize(registered.length() - suffix.length());
            }

            if (!registered.empty())
            {
                m_actualInstanceNameWithoutSuffix = winrt::hstring{ registered };
            }

            TraceLoggingWrite(
                MidiNetworkMidiTransportTelemetryProvider::Provider(),
                MIDI_TRACE_EVENT_WARNING,
                TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
                TraceLoggingLevel(WINEVENT_LEVEL_WARNING),
                TraceLoggingPointer(this, "this"),
                TraceLoggingWideString(L"DNS-SD renamed this host because its service instance name collided on the network", MIDI_TRACE_EVENT_MESSAGE_FIELD),
                TraceLoggingWideString(fullServiceName.c_str(), "requested name"),
                TraceLoggingWideString(m_actualInstanceNameWithoutSuffix.c_str(), "actual name")
            );
        }

        TraceLoggingWrite(
            MidiNetworkMidiTransportTelemetryProvider::Provider(),
            MIDI_TRACE_EVENT_INFO,
            TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
            TraceLoggingLevel(WINEVENT_LEVEL_INFO),
            TraceLoggingPointer(this, "this"),
            TraceLoggingWideString(L"Registered socket successfully", MIDI_TRACE_EVENT_MESSAGE_FIELD)

        );
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

    TraceLoggingWrite(
        MidiNetworkMidiTransportTelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_INFO,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this"),
        TraceLoggingWideString(L"Exit", MIDI_TRACE_EVENT_MESSAGE_FIELD)
    );
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

    m_serviceInstance = nullptr;


    return S_OK;
}
