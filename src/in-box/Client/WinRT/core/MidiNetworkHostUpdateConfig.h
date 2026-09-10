// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App WinRT API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once
#include "Transports.Network.MidiNetworkHostUpdateConfig.g.h"

#include "..\..\..\Transport\UdpNetworkMidi2Transport\net2udp_transport_defs.h"
#include "..\..\..\Transport\UdpNetworkMidi2Transport\network_json_defs.h"

namespace winrt::Windows::Devices::Midi2::Transports::Network::implementation
{
    struct MidiNetworkHostUpdateConfig : MidiNetworkHostUpdateConfigT<MidiNetworkHostUpdateConfig>
    {
        MidiNetworkHostUpdateConfig() = default;

        MidiNetworkHostUpdateConfig(_In_ winrt::guid const& hostId) { m_hostId = hostId; }

        winrt::guid TransportId() const noexcept { return internal::StringToGuid(MIDI_NETWORK_TRANSPORT_ID); }

        winrt::guid HostId() const noexcept { return m_hostId; }
        void HostId(_In_ winrt::guid const& value) noexcept { m_hostId = value; }

        bool CreateMidi1Ports() const noexcept { return m_createMidi1Ports; }
        void CreateMidi1Ports(_In_ bool const value) noexcept { m_createMidi1Ports = value; }

        uint8_t FallbackMidi1PortCount() const noexcept { return m_fallbackMidi1PortCount; }
        void FallbackMidi1PortCount(_In_ uint8_t const value) noexcept { m_fallbackMidi1PortCount = value; }

        json::JsonObject ConfigJson() const noexcept;

    private:
        winrt::guid m_hostId{};
        bool m_createMidi1Ports{ MIDI_NETWORK_MIDI_CREATE_MIDI1_PORTS_DEFAULT };
        uint8_t m_fallbackMidi1PortCount{ MIDI_NETWORK_MIDI_FALLBACK_MIDI1_PORT_COUNT_DEFAULT };
    };
}
namespace winrt::Windows::Devices::Midi2::Transports::Network::factory_implementation
{
    struct MidiNetworkHostUpdateConfig : MidiNetworkHostUpdateConfigT<MidiNetworkHostUpdateConfig, implementation::MidiNetworkHostUpdateConfig>
    {
    };
}
