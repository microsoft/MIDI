// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://github.com/microsoft/MIDI/
// ============================================================================

#pragma once

// One host's DNS-SD registration. Why it is made the way it is, and why the transport repeats the
// announcements the DNS client makes for it, is at the top of MidiNetworkAdvertiser.cpp.
class MidiNetworkAdvertiser
{
public:
    HRESULT Initialize();

    // An empty host name is this PC's .local name, and a zero interface index is every adapter.
    // S_FALSE when the DNS client was still working on it at the timeout: the host is advertised
    // once the DNS client finishes, and Shutdown withdraws it either way.
    HRESULT Advertise(
        _In_ winrt::hstring const& serviceInstanceNameWithoutSuffix,
        _In_ winrt::hstring const& hostName,
        _In_ uint16_t const port,
        _In_ winrt::hstring const& midiEndpointName,
        _In_ winrt::hstring const& midiProductInstanceId,
        _In_ uint32_t const interfaceIndex
    );

    // Withdraws the registration, which sends the goodbye
    HRESULT Shutdown();

    // A DNS-SD responder renames a colliding instance label instead of refusing to register it,
    // so what is on the network is not necessarily what was configured.
    bool InstanceNameWasChanged() const noexcept { return m_registration.WasRenamed(); }

    // The label actually on the network, without the service type suffix. The requested one until
    // the DNS client says what it chose, and empty if it cannot be copied.
    winrt::hstring ActualInstanceNameWithoutSuffix() const noexcept;

private:
    ::WindowsMidiServicesInternal::MidiDnssdAdvertiser m_registration;
};