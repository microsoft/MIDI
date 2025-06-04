// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App SDK and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once
#include "Endpoints.Network.MidiNetworkAdvertisedHost.g.h"


namespace winrt::Microsoft::Windows::Devices::Midi2::Endpoints::Network::implementation
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

        collections::IVectorView<winrt::hstring> IPAddresses() { return m_ipAddresses.GetView(); }

        void InternalInitialize(
            _In_ winrt::hstring deviceId,
            _In_ winrt::hstring deviceName,
            _In_ winrt::hstring fullName,
            _In_ winrt::hstring serviceInstanceName,
            _In_ winrt::hstring serviceType,
            _In_ winrt::hstring hostName,
            _In_ uint16_t port,
            _In_ winrt::hstring domain,
            _In_ winrt::hstring umpEndpointName,
            _In_ winrt::hstring productInstanceId,
            _In_ collections::IVector<winrt::hstring> ipAddresses
            ) noexcept;

        void InternalUpdateFromDeviceInformation(
            _In_ enumeration::DeviceInformation device
            ) noexcept;

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

        collections::IVector<winrt::hstring> m_ipAddresses = winrt::multi_threaded_vector<winrt::hstring>();

    };
}
