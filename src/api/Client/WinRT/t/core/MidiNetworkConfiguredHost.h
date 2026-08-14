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

        winrt::hstring UmpEndpointName() const noexcept { return m_umpEndpointName; }
        winrt::hstring ProductInstanceId() const noexcept { return m_productInstanceId; }

        winrt::hstring ServiceInstanceName() const noexcept { return m_serviceInstanceName; }

        bool CreateMidi1Ports() const noexcept { return m_createMidi1Ports; }

        void InternalInitialize(
            bool const isEnabled,
            winrt::guid const& hostId,
            winrt::hstring const& umpEndpointName,
            winrt::hstring const& productInstanceId,
            winrt::hstring const& serviceInstanceName,
            bool const hasStarted,
            winrt::hstring const& actualAddress,
            winrt::hstring const& actualPort,
            bool const createMidi1Ports) noexcept
        {
            m_isEnabled = isEnabled;
            m_hostId = hostId;
            m_umpEndpointName = umpEndpointName;
            m_productInstanceId = productInstanceId;
            m_serviceInstanceName = serviceInstanceName;
            m_hasStarted = hasStarted;
            m_actualAddress = actualAddress;
            m_actualPort = actualPort;
            m_createMidi1Ports = createMidi1Ports;
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
        bool m_createMidi1Ports{ false };


    };
}
