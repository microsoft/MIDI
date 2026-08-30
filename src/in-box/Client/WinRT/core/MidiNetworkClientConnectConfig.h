// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App WinRT API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once
#include "Transports.Network.MidiNetworkClientConnectConfig.g.h"

#include "..\..\..\Transport\UdpNetworkMidi2Transport\net2udp_transport_defs.h"

namespace winrt::Windows::Devices::Midi2::Transports::Network::implementation
{
    struct MidiNetworkClientConnectConfig : MidiNetworkClientConnectConfigT<MidiNetworkClientConnectConfig>
    {
        MidiNetworkClientConnectConfig() = default;

        winrt::guid TransportId() const noexcept { return internal::StringToGuid(MIDI_NETWORK_TRANSPORT_ID); }
        json::JsonObject ConfigJson() const noexcept;

        winrt::hstring Comment() const noexcept { return m_comment; }
        void Comment(_In_ winrt::hstring const& value) noexcept { m_comment = value; }

        winrt::guid ClientId() const noexcept { return m_id; }
        void ClientId(_In_ winrt::guid const& value) noexcept { m_id = value; }

        winrt::hstring UmpEndpointName() const noexcept { return m_umpEndpointName; }
        void UmpEndpointName(_In_ winrt::hstring const& value) noexcept { m_umpEndpointName = internal::TruncateToUtf8ByteCount(value.c_str(), MIDI_MAX_UMP_ENDPOINT_NAME_BYTE_COUNT); }

        winrt::hstring CustomEndpointName() const noexcept { return m_customEndpointName; }
        void CustomEndpointName(_In_ winrt::hstring const& value) noexcept { m_customEndpointName = internal::TruncateToUtf8ByteCount(value.c_str(), MIDI_MAX_UMP_ENDPOINT_NAME_BYTE_COUNT); }

        bool CreateOnlyUmpEndpoints() const noexcept { return m_umpOnly; }
        void CreateOnlyUmpEndpoints(_In_ bool const value) noexcept { m_umpOnly = value; }

        bool AutoReconnect() const noexcept{ return m_autoReconnect; }
        void AutoReconnect(_In_ bool const value) noexcept { m_autoReconnect = value; }

        network::MidiNetworkClientMatchCriteria MatchCriteria() const noexcept { return m_matchCriteria; }
        void MatchCriteria(_In_ network::MidiNetworkClientMatchCriteria const& value) noexcept { m_matchCriteria = value; }

    private:
        winrt::hstring m_umpEndpointName{};
        winrt::hstring m_customEndpointName{};
        winrt::guid m_id{};
        bool m_umpOnly{ false };
        bool m_autoReconnect{ true };
        winrt::hstring m_comment{};

        network::MidiNetworkClientMatchCriteria m_matchCriteria{};

    };
}
namespace winrt::Windows::Devices::Midi2::Transports::Network::factory_implementation
{
    struct MidiNetworkClientConnectConfig : MidiNetworkClientConnectConfigT<MidiNetworkClientConnectConfig, implementation::MidiNetworkClientConnectConfig>
    {
    };
}
