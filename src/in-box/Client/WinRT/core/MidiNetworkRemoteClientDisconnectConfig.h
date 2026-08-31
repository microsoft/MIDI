// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App WinRT API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once
#include "Transports.Network.MidiNetworkRemoteClientDisconnectConfig.g.h"

namespace winrt::Windows::Devices::Midi2::Transports::Network::implementation
{
    struct MidiNetworkRemoteClientDisconnectConfig : MidiNetworkRemoteClientDisconnectConfigT<MidiNetworkRemoteClientDisconnectConfig>
    {
        MidiNetworkRemoteClientDisconnectConfig() = default;

        MidiNetworkRemoteClientDisconnectConfig(
            _In_ winrt::guid const& hostId,
            _In_ winrt::hstring const& remoteClientName,
            _In_ winrt::hstring const& remoteClientProductInstanceId) noexcept;

        winrt::guid TransportId() const noexcept { return network::MidiNetworkTransportManager::TransportId(); }

        winrt::guid HostId() const noexcept { return m_hostId; }
        void HostId(_In_ winrt::guid const& value) noexcept { m_hostId = value; }

        hstring RemoteClientName() const noexcept { return m_remoteClientName; }
        void RemoteClientName(_In_ winrt::hstring const& value) noexcept { m_remoteClientName = value; }

        hstring RemoteClientProductInstanceId() const noexcept { return m_remoteClientProductInstanceId; }
        void RemoteClientProductInstanceId(_In_ winrt::hstring const& value) noexcept { m_remoteClientProductInstanceId = value; }

        json::JsonObject ConfigJson() noexcept;

    private:
        winrt::guid m_hostId{};
        hstring m_remoteClientName{};
        hstring m_remoteClientProductInstanceId{};
    };
}
namespace winrt::Windows::Devices::Midi2::Transports::Network::factory_implementation
{
    struct MidiNetworkRemoteClientDisconnectConfig : MidiNetworkRemoteClientDisconnectConfigT<MidiNetworkRemoteClientDisconnectConfig, implementation::MidiNetworkRemoteClientDisconnectConfig>
    {
    };
}
