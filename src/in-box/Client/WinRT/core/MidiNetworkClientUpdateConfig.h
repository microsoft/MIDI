// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App WinRT API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once
#include "Transports.Network.MidiNetworkClientUpdateConfig.g.h"

#include "..\..\..\Transport\UdpNetworkMidi2Transport\net2udp_transport_defs.h"
#include "..\..\..\Transport\UdpNetworkMidi2Transport\network_json_defs.h"

namespace winrt::Windows::Devices::Midi2::Transports::Network::implementation
{
    struct MidiNetworkClientUpdateConfig : MidiNetworkClientUpdateConfigT<MidiNetworkClientUpdateConfig>
    {
        MidiNetworkClientUpdateConfig() = default;

        MidiNetworkClientUpdateConfig(_In_ winrt::guid const& clientId) { m_clientId = clientId; }

        winrt::guid TransportId() const noexcept { return internal::StringToGuid(MIDI_NETWORK_TRANSPORT_ID); }

        winrt::guid ClientId() const noexcept { return m_clientId; }
        void ClientId(_In_ winrt::guid const& value) noexcept { m_clientId = value; }

        bool CreateMidi1Ports() const noexcept { return m_createMidi1Ports; }
        void CreateMidi1Ports(_In_ bool const value) noexcept { m_createMidi1Ports = value; }

        uint8_t FallbackMidi1PortCount() const noexcept { return m_fallbackMidi1PortCount; }
        void FallbackMidi1PortCount(_In_ uint8_t const value) noexcept { m_fallbackMidi1PortCount = value; }

        network::MidiNetworkSendSpeedLimit SendSpeedLimit() const noexcept { return m_sendSpeedLimit.value_or(network::MidiNetworkSendSpeedLimit::Unlimited); }
        void SendSpeedLimit(_In_ network::MidiNetworkSendSpeedLimit const& value) noexcept { m_sendSpeedLimit = value; }

        bool ReduceSendSpeedAutomatically() const noexcept { return m_reduceSendSpeedAutomatically.value_or(false); }
        void ReduceSendSpeedAutomatically(_In_ bool const value) noexcept { m_reduceSendSpeedAutomatically = value; }

        json::JsonObject ConfigJson() const noexcept;

    private:
        winrt::guid m_clientId{};
        bool m_createMidi1Ports{ MIDI_NETWORK_MIDI_CREATE_MIDI1_PORTS_DEFAULT };
        uint8_t m_fallbackMidi1PortCount{ MIDI_NETWORK_MIDI_FALLBACK_MIDI1_PORT_COUNT_DEFAULT };

        // Empty until set. The merge into the configuration file cannot delete a key, and these
        // came later than the two above, so a caller which never sets them keeps the saved ones.
        std::optional<network::MidiNetworkSendSpeedLimit> m_sendSpeedLimit{};
        std::optional<bool> m_reduceSendSpeedAutomatically{};
    };
}
namespace winrt::Windows::Devices::Midi2::Transports::Network::factory_implementation
{
    struct MidiNetworkClientUpdateConfig : MidiNetworkClientUpdateConfigT<MidiNetworkClientUpdateConfig, implementation::MidiNetworkClientUpdateConfig>
    {
    };
}
