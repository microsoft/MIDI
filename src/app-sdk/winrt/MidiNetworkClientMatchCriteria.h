// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App SDK and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once
#include "Endpoints.Network.MidiNetworkClientMatchCriteria.g.h"

namespace winrt::Microsoft::Windows::Devices::Midi2::Endpoints::Network::implementation
{
    struct MidiNetworkClientMatchCriteria : MidiNetworkClientMatchCriteriaT<MidiNetworkClientMatchCriteria>
    {
        MidiNetworkClientMatchCriteria() = default;

        winrt::hstring DeviceId() const noexcept { return m_deviceId; }
        void DeviceId(_In_ winrt::hstring const& value) noexcept { m_deviceId = value; }

        winrt::Windows::Networking::HostName HostName() const noexcept { return m_hostName; }
        void HostName(_In_ winrt::Windows::Networking::HostName const& value) { m_hostName = value; }

        uint16_t Port() const noexcept { return m_port; }
        void Port(_In_ uint16_t value) noexcept { m_port = value; }

    private:
        winrt::hstring m_deviceId{};

        winrt::Windows::Networking::HostName m_hostName{ nullptr };
        uint16_t m_port{ 0 };
    };
}
namespace winrt::Microsoft::Windows::Devices::Midi2::Endpoints::Network::factory_implementation
{
    struct MidiNetworkClientMatchCriteria : MidiNetworkClientMatchCriteriaT<MidiNetworkClientMatchCriteria, implementation::MidiNetworkClientMatchCriteria>
    {
    };
}
