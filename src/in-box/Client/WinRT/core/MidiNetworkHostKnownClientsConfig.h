// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App WinRT API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once
#include "Transports.Network.MidiNetworkHostKnownClientsConfig.g.h"

#include "..\..\..\Transport\UdpNetworkMidi2Transport\net2udp_transport_defs.h"

namespace winrt::Windows::Devices::Midi2::Transports::Network::implementation
{
    struct MidiNetworkHostKnownClientsConfig : MidiNetworkHostKnownClientsConfigT<MidiNetworkHostKnownClientsConfig>
    {
        MidiNetworkHostKnownClientsConfig() = default;

        MidiNetworkHostKnownClientsConfig(_In_ winrt::guid const& hostId) noexcept { m_hostId = hostId; }

        winrt::guid TransportId() const noexcept { return internal::StringToGuid(MIDI_NETWORK_TRANSPORT_ID); }

        winrt::guid HostId() const noexcept { return m_hostId; }
        void HostId(_In_ winrt::guid const& value) noexcept { m_hostId = value; }

        foundation::Collections::IVector<network::MidiNetworkKnownRemoteClient> KnownClients() const noexcept { return m_knownClients; }

        json::JsonObject ConfigJson() const noexcept;

    private:
        winrt::guid m_hostId{};

        foundation::Collections::IVector<network::MidiNetworkKnownRemoteClient> m_knownClients{
            winrt::single_threaded_vector<network::MidiNetworkKnownRemoteClient>() };
    };
}

namespace winrt::Windows::Devices::Midi2::Transports::Network::factory_implementation
{
    struct MidiNetworkHostKnownClientsConfig : MidiNetworkHostKnownClientsConfigT<MidiNetworkHostKnownClientsConfig, implementation::MidiNetworkHostKnownClientsConfig>
    {
    };
}
