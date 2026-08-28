// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App WinRT API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once
#include "Transports.Network.MidiNetworkConfiguredHost.g.h"

namespace winrt::Windows::Devices::Midi2::Transports::Network::implementation
{
    struct MidiNetworkConfiguredHost : MidiNetworkConfiguredHostT<MidiNetworkConfiguredHost>
    {
        MidiNetworkConfiguredHost() = default;

        winrt::guid HostId() const noexcept { return m_hostId; }

        bool IsEnabled() const noexcept { return m_isEnabled; }
        bool HasStarted() const noexcept { return m_hasStarted; }

        winrt::hstring ActualPort() const noexcept { return m_actualPort; }
        winrt::hstring ActualAddress() const noexcept { return m_actualAddress; }

        winrt::hstring ConfiguredPort() const noexcept { return m_configuredPort; }
        bool AllowPortFallback() const noexcept { return m_allowPortFallback; }
        bool UsedPortFallback() const noexcept { return m_usedPortFallback; }

        winrt::hstring UmpEndpointName() const noexcept { return m_umpEndpointName; }
        winrt::hstring ProductInstanceId() const noexcept { return m_productInstanceId; }

        winrt::hstring ServiceInstanceName() const noexcept { return m_serviceInstanceName; }

        bool CreateMidi1Ports() const noexcept { return m_createMidi1Ports; }

        network::MidiNetworkRemoteClientPolicy RemoteClientPolicy() const noexcept { return m_remoteClientPolicy; }

        winrt::Windows::Foundation::Collections::IVectorView<network::MidiNetworkHostConnection> Connections() const noexcept
        {
            return m_connections.GetView();
        }

        void InternalInitialize(
            _In_ bool const isEnabled,
            _In_ winrt::guid const& hostId,
            _In_ winrt::hstring const& umpEndpointName,
            _In_ winrt::hstring const& productInstanceId,
            _In_ winrt::hstring const& serviceInstanceName,
            _In_ bool const hasStarted,
            _In_ winrt::hstring const& actualAddress,
            _In_ winrt::hstring const& actualPort,
            _In_ winrt::hstring const& configuredPort,
            _In_ bool const allowPortFallback,
            _In_ bool const usedPortFallback,
            _In_ bool const createMidi1Ports,
            _In_ network::MidiNetworkRemoteClientPolicy const remoteClientPolicy) noexcept
        {
            m_isEnabled = isEnabled;
            m_hostId = hostId;
            m_umpEndpointName = umpEndpointName;
            m_productInstanceId = productInstanceId;
            m_serviceInstanceName = serviceInstanceName;
            m_hasStarted = hasStarted;
            m_actualAddress = actualAddress;
            m_actualPort = actualPort;
            m_configuredPort = configuredPort;
            m_allowPortFallback = allowPortFallback;
            m_usedPortFallback = usedPortFallback;
            m_createMidi1Ports = createMidi1Ports;
            m_remoteClientPolicy = remoteClientPolicy;
        }

        void InternalAddConnection(_In_ network::MidiNetworkHostConnection const& connection) noexcept
        {
            if (connection != nullptr)
            {
                m_connections.Append(connection);
            }
        }

    private:
        bool m_isEnabled{ false };
        winrt::guid m_hostId{};
        winrt::hstring m_umpEndpointName{};
        winrt::hstring m_productInstanceId{};
        winrt::hstring m_serviceInstanceName{};
        bool m_hasStarted{ false };
        winrt::hstring m_actualAddress{};
        winrt::hstring m_actualPort{};
        winrt::hstring m_configuredPort{};
        bool m_allowPortFallback{ true };
        bool m_usedPortFallback{ false };
        bool m_createMidi1Ports{ false };
        network::MidiNetworkRemoteClientPolicy m_remoteClientPolicy{ network::MidiNetworkRemoteClientPolicy::AllowAny };

        winrt::Windows::Foundation::Collections::IVector<network::MidiNetworkHostConnection> m_connections{
            winrt::single_threaded_vector<network::MidiNetworkHostConnection>() };
    };
}
