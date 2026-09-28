// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services WinRT API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once
#include "Transports.Rtp.MidiRtpAdvertisedHost.g.h"

#include "MidiRtpSdkJson.h"

namespace winrt::Windows::Devices::Midi2::Transports::Rtp::implementation
{
    struct MidiRtpAdvertisedHost : MidiRtpAdvertisedHostT<MidiRtpAdvertisedHost>
    {
        MidiRtpAdvertisedHost() = default;

        winrt::hstring ServiceInstanceName() const noexcept { return m_serviceInstanceName; }
        winrt::hstring HostName() const noexcept { return m_hostName; }
        uint16_t Port() const noexcept { return m_port; }
        collections::IVectorView<winrt::hstring> IPAddresses() const noexcept { return m_ipAddresses.GetView(); }
        collections::IVectorView<winrt::hstring> IPv4Addresses() const noexcept { return m_ipv4Addresses.GetView(); }
        collections::IVectorView<winrt::hstring> IPv6Addresses() const noexcept { return m_ipv6Addresses.GetView(); }
        bool IsThisPc() const noexcept { return m_isThisPc; }

        void InternalInitialize(_In_ json::JsonObject const& source) noexcept;

    private:
        winrt::hstring m_serviceInstanceName{};
        winrt::hstring m_hostName{};
        uint16_t m_port{ 0 };
        collections::IVector<winrt::hstring> m_ipAddresses{ winrt::single_threaded_vector<winrt::hstring>() };
        collections::IVector<winrt::hstring> m_ipv4Addresses{ winrt::single_threaded_vector<winrt::hstring>() };
        collections::IVector<winrt::hstring> m_ipv6Addresses{ winrt::single_threaded_vector<winrt::hstring>() };
        bool m_isThisPc{ false };
    };
}
