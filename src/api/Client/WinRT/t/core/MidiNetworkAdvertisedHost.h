// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App WinRT API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once
#include "Transports.Network.MidiNetworkAdvertisedHost.g.h"

#include "midi_dnssd_browser.h"


namespace winrt::Windows::Devices::Midi2::Transports::Network::implementation
{
    struct MidiNetworkAdvertisedHost : MidiNetworkAdvertisedHostT<MidiNetworkAdvertisedHost>
    {
        MidiNetworkAdvertisedHost() = default;

        winrt::hstring DeviceId() const noexcept { return m_deviceId; }
        winrt::hstring DeviceName() const noexcept { return m_deviceName; }
        winrt::hstring FullName() const noexcept { return m_fullName; }
        winrt::hstring ServiceInstanceName() const noexcept { return m_serviceInstanceName; }
        winrt::hstring ServiceType() const noexcept { return m_serviceType; }
        winrt::hstring HostName() const noexcept { return m_hostName; }
        uint16_t Port() const noexcept { return m_port; }
        winrt::hstring Domain() const noexcept { return m_domain; }
        winrt::hstring UmpEndpointName() const noexcept { return m_umpEndpointName; }
        winrt::hstring ProductInstanceId() const noexcept { return m_productInstanceId; }

        foundation::DateTime LastSeenTime() const noexcept { return m_lastSeenTime; }

        collections::IMapView<winrt::hstring, winrt::hstring> TextAttributes() noexcept { return m_textAttributes.GetView(); }

        collections::IVectorView<winrt::hstring> IPAddresses() noexcept { return m_ipAddresses.GetView(); }
        collections::IVectorView<winrt::hstring> IPv4Addresses() noexcept { return m_ipv4Addresses.GetView(); }
        collections::IVectorView<winrt::hstring> IPv6Addresses() noexcept { return m_ipv6Addresses.GetView(); }

        void InternalInitializeFromDnssdService(
            _In_ ::WindowsMidiServicesInternal::MidiDnssdService const& service) noexcept;

    private:
        winrt::hstring m_deviceId{};
        winrt::hstring m_deviceName{};
        winrt::hstring m_fullName{};
        winrt::hstring m_serviceInstanceName{};
        winrt::hstring m_serviceType{};
        winrt::hstring m_hostName{};
        uint16_t m_port{};
        winrt::hstring m_domain{};
        winrt::hstring m_umpEndpointName{};
        winrt::hstring m_productInstanceId{};

        foundation::DateTime m_lastSeenTime{};

        collections::IMap<winrt::hstring, winrt::hstring> m_textAttributes = winrt::multi_threaded_map<winrt::hstring, winrt::hstring>();

        collections::IVector<winrt::hstring> m_ipAddresses = winrt::multi_threaded_vector<winrt::hstring>();
        collections::IVector<winrt::hstring> m_ipv4Addresses = winrt::multi_threaded_vector<winrt::hstring>();
        collections::IVector<winrt::hstring> m_ipv6Addresses = winrt::multi_threaded_vector<winrt::hstring>();
    };
}
