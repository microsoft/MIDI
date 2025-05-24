// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App SDK and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once
#include "Endpoints.Network.MidiNetworkClientEndpointCreationConfig.g.h"

namespace winrt::Microsoft::Windows::Devices::Midi2::Endpoints::Network::implementation
{
    struct MidiNetworkClientEndpointCreationConfig : MidiNetworkClientEndpointCreationConfigT<MidiNetworkClientEndpointCreationConfig>
    {
        MidiNetworkClientEndpointCreationConfig() = default;

        winrt::guid TransportId() const noexcept { return internal::StringToGuid(MIDI_NETWORK_TRANSPORT_ID); }
        winrt::hstring GetConfigJson() const noexcept;

        winrt::hstring Id() const noexcept { return m_id; }
        void Id(_In_ winrt::hstring const& value) { m_id = value; }

        bool UmpOnly() const noexcept { return m_umpOnly; }
        void UmpOnly(_In_ bool const value) noexcept { m_umpOnly = value; }

        bool AutoReconnect() const noexcept{ return m_autoReconnect; }
        void AutoReconnect(_In_ bool const value) noexcept { m_autoReconnect = value; }

        midi2::Endpoints::Network::MidiNetworkClientMatchCriteria MatchCriteria() const noexcept { return m_matchCriteria; }
        void MatchCriteria(_In_ network::MidiNetworkClientMatchCriteria const& value) noexcept { m_matchCriteria = value; }

    private:
        winrt::hstring m_id{};
        bool m_umpOnly{ false };
        bool m_autoReconnect{ true };

        network::MidiNetworkClientMatchCriteria m_matchCriteria{};

    };
}
namespace winrt::Microsoft::Windows::Devices::Midi2::Endpoints::Network::factory_implementation
{
    struct MidiNetworkClientEndpointCreationConfig : MidiNetworkClientEndpointCreationConfigT<MidiNetworkClientEndpointCreationConfig, implementation::MidiNetworkClientEndpointCreationConfig>
    {
    };
}
