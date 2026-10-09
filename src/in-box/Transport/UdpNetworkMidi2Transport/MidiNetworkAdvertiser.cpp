// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://github.com/microsoft/MIDI/
// ============================================================================

#include "pch.h"

// DNS-SD registration, and the workaround that goes with it. Measured on the wire, Sep 2026.
//
// Registration goes through DnsServiceRegister (midi_dnssd_advertiser.h, shared with the RTP-MIDI
// transport), not the WinRT DnssdServiceInstance used before. A WinRT registration lasts exactly as
// long as its socket, so it could only be made again by closing the socket and ending every session.
// And on a customer's PC (issue #1249), Windows twice stopped answering correctly for the WinRT
// registration while the host kept running: once by not answering at all, and once by listing the
// PC's other services under _midi2._udp. The RTP-MIDI host's DnsServiceRegister registration beside
// it stayed right. Both APIs go through the same DNS client, which probes, renames a colliding
// label, answers queries and sends a goodbye the same way for either.
//
// What the DNS client gets wrong is the announcement of a new registration. It sends one, where
// RFC 6762 section 8.3 asks for at least two, and it sets the cache-flush bit on the shared PTR
// record, which section 10.2 forbids. A device which misses that one packet doesn't list the host
// until it asks again, and a Mac can go a long time without asking. The flush bit also makes
// every device which hears it drop the other hosts it knew of, including this PC's own.
//
// So after each registration the endpoint manager repeats every host's PTR record without the
// flush bit, at 1.5 and 4.5 seconds (midi_dnssd_announcer.h). A host is withdrawn from those
// repeats before its registration ends, because a repeat sent after the goodbye would put it
// back in other devices' lists for the record's 75 minute lifetime.
//
// Remove the repeats once the DNS client is fixed.

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



_Use_decl_annotations_
HRESULT 
MidiNetworkAdvertiser::Advertise(
    winrt::hstring const& serviceInstanceNameWithoutSuffix,
    winrt::hstring const& hostName,
    uint16_t const port,
    winrt::hstring const& midiEndpointName,
    winrt::hstring const& midiProductInstanceId,
    uint32_t const interfaceIndex
)
{
    // Declared HRESULT, so it must not throw: callers use RETURN_IF_FAILED and an
    // escaping exception would unwind past them into a worker thread.
    try
    {
        TraceLoggingWrite(
            MidiNetworkMidiTransportTelemetryProvider::Provider(),
            MIDI_TRACE_EVENT_INFO,
            TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
            TraceLoggingLevel(WINEVENT_LEVEL_INFO),
            TraceLoggingPointer(this, "this"),
            TraceLoggingWideString(L"Enter", MIDI_TRACE_EVENT_MESSAGE_FIELD)
        );

        auto const hr = m_registration.Register(
            std::wstring{ serviceInstanceNameWithoutSuffix },
            DNS_PTR_SERVICE_TYPE,
            std::wstring{ hostName },
            port,
            {
                { L"UMPEndpointName", std::wstring{ midiEndpointName } },
                { L"ProductInstanceId", std::wstring{ midiProductInstanceId } }
            },
            ::WindowsMidiServicesInternal::MidiDnssdRegistrationTimeoutMilliseconds,
            std::stop_token{ },
            interfaceIndex);

        if (hr == HRESULT_FROM_WIN32(ERROR_TIMEOUT))
        {
            TraceLoggingWrite(
                MidiNetworkMidiTransportTelemetryProvider::Provider(),
                MIDI_TRACE_EVENT_WARNING,
                TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
                TraceLoggingLevel(WINEVENT_LEVEL_WARNING),
                TraceLoggingPointer(this, "this"),
                TraceLoggingWideString(L"The DNS client has not finished registering this host. It is advertised once it does.", MIDI_TRACE_EVENT_MESSAGE_FIELD),
                TraceLoggingWideString(serviceInstanceNameWithoutSuffix.c_str(), "requested name")
            );

            return S_FALSE;
        }

        if (FAILED(hr))
        {
            TraceLoggingWrite(
                MidiNetworkMidiTransportTelemetryProvider::Provider(),
                MIDI_TRACE_EVENT_ERROR,
                TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
                TraceLoggingLevel(WINEVENT_LEVEL_ERROR),
                TraceLoggingPointer(this, "this"),
                TraceLoggingWideString(L"Unable to register this host with DNS-SD", MIDI_TRACE_EVENT_MESSAGE_FIELD),
                TraceLoggingWideString(serviceInstanceNameWithoutSuffix.c_str(), "requested name"),
                TraceLoggingHResult(hr, MIDI_TRACE_EVENT_HRESULT_FIELD)
            );

            return hr;
        }

        auto const registeredLabel = m_registration.RegisteredLabel();

        // The responder renames a colliding instance label rather than refusing it, so the name
        // on the wire is not necessarily the one we asked for. Recorded because everything else
        // reports the configured name, and the two disagreeing is otherwise invisible.
        if (m_registration.WasRenamed())
        {
            TraceLoggingWrite(
                MidiNetworkMidiTransportTelemetryProvider::Provider(),
                MIDI_TRACE_EVENT_WARNING,
                TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
                TraceLoggingLevel(WINEVENT_LEVEL_WARNING),
                TraceLoggingPointer(this, "this"),
                TraceLoggingWideString(L"DNS-SD renamed this host because its service instance name collided on the network", MIDI_TRACE_EVENT_MESSAGE_FIELD),
                TraceLoggingWideString(serviceInstanceNameWithoutSuffix.c_str(), "requested name"),
                TraceLoggingWideString(registeredLabel.c_str(), "actual name")
            );
        }

        TraceLoggingWrite(
            MidiNetworkMidiTransportTelemetryProvider::Provider(),
            MIDI_TRACE_EVENT_INFO,
            TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
            TraceLoggingLevel(WINEVENT_LEVEL_INFO),
            TraceLoggingPointer(this, "this"),
            TraceLoggingWideString(L"Registered with DNS-SD", MIDI_TRACE_EVENT_MESSAGE_FIELD),
            TraceLoggingWideString(registeredLabel.c_str(), "name"),
            TraceLoggingUInt16(port, "port"),
            TraceLoggingUInt32(interfaceIndex, "interface index")
        );

        return S_OK;
    }
    CATCH_RETURN()
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

    m_registration.Unregister();

    return S_OK;
}

winrt::hstring
MidiNetworkAdvertiser::ActualInstanceNameWithoutSuffix() const noexcept
{
    try
    {
        return winrt::hstring{ m_registration.RegisteredLabel() };
    }
    catch (...)
    {
        return winrt::hstring{ };
    }
}
