// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://github.com/microsoft/MIDI/
// ============================================================================

#pragma once

using namespace winrt::Windows::Networking;
using namespace winrt::Windows::Networking::Sockets;
//using namespace winrt::Windows::Networking::ServiceDiscovery::Dnssd;



// One host's DNS-SD registration. Why it uses the WinRT registration, and why the transport
// repeats the announcements the DNS client makes for it, is at the top of MidiNetworkAdvertiser.cpp.
class MidiNetworkAdvertiser
{
public:
    HRESULT Initialize();

    // A null adapter advertises on every adapter
    HRESULT Advertise(
        _In_ winrt::hstring const& serviceInstanceNameWithoutSuffix,
        _In_ HostName const& hostName,
        _In_ DatagramSocket const& boundSocket,
        _In_ uint16_t const port,
        _In_ winrt::hstring const& midiEndpointName,
        _In_ winrt::hstring const& midiProductInstanceId,
        _In_ winrt::Windows::Networking::Connectivity::NetworkAdapter const& adapter
    );

    HRESULT Shutdown();

    // A DNS-SD responder renames a colliding instance label instead of refusing to register it,
    // so what is on the network is not necessarily what was configured.
    bool InstanceNameWasChanged() const { return m_instanceNameWasChanged; }

    // The label actually on the network, without the service type suffix. Empty if the platform
    // did not tell us what it chose.
    winrt::hstring ActualInstanceNameWithoutSuffix() const { return m_actualInstanceNameWithoutSuffix; }

private:
    winrt::Windows::Networking::ServiceDiscovery::Dnssd::DnssdServiceInstance m_serviceInstance{ nullptr };

    bool m_instanceNameWasChanged{ false };
    winrt::hstring m_actualInstanceNameWithoutSuffix{ };
};