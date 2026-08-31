// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App WinRT API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once
#include "Transports.Network.MidiNetworkHostRemovalConfig.g.h"

#include "..\..\..\Transport\UdpNetworkMidi2Transport\net2udp_transport_defs.h"

namespace winrt::Windows::Devices::Midi2::Transports::Network::implementation
{
    struct MidiNetworkHostRemovalConfig : MidiNetworkHostRemovalConfigT<MidiNetworkHostRemovalConfig>
    {
        MidiNetworkHostRemovalConfig() = default;

        MidiNetworkHostRemovalConfig(_In_ winrt::guid const& hostId) { m_hostId = hostId; }

        winrt::guid TransportId() const noexcept { return internal::StringToGuid(MIDI_NETWORK_TRANSPORT_ID); }
        
        winrt::guid HostId() const noexcept { return m_hostId; }
        void HostId(_In_ winrt::guid const& value) noexcept { m_hostId = value; }

        json::JsonObject ConfigJson() const noexcept;

    private:
        winrt::guid m_hostId{};
    };
}
namespace winrt::Windows::Devices::Midi2::Transports::Network::factory_implementation
{
    struct MidiNetworkHostRemovalConfig : MidiNetworkHostRemovalConfigT<MidiNetworkHostRemovalConfig, implementation::MidiNetworkHostRemovalConfig>
    {
    };
}
