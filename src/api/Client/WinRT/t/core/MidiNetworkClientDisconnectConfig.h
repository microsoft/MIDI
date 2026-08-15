// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App WinRT API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once
#include "Transports.Network.MidiNetworkClientDisconnectConfig.g.h"

#include "..\..\..\..\Transport\UdpNetworkMidi2Transport\net2udp_transport_defs.h"

namespace winrt::Windows::Devices::Midi2::Transports::Network::implementation
{
    struct MidiNetworkClientDisconnectConfig : MidiNetworkClientDisconnectConfigT<MidiNetworkClientDisconnectConfig>
    {
        MidiNetworkClientDisconnectConfig() = default;

        MidiNetworkClientDisconnectConfig(_In_ winrt::guid const& clientId) { m_id = clientId; }


        winrt::guid TransportId() const noexcept { return internal::StringToGuid(MIDI_NETWORK_TRANSPORT_ID); }
        json::JsonObject ConfigJson() const noexcept;

        winrt::guid ClientId() const noexcept { return m_id; }
        void ClientId(_In_ winrt::guid const& value) noexcept { m_id = value; }

    private:
        winrt::guid m_id{};

    };
}
namespace winrt::Windows::Devices::Midi2::Transports::Network::factory_implementation
{
    struct MidiNetworkClientDisconnectConfig : MidiNetworkClientDisconnectConfigT<MidiNetworkClientDisconnectConfig, implementation::MidiNetworkClientDisconnectConfig>
    {
    };
}
