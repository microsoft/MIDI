// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services WinRT API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once
#include "..\..\..\..\Transport\UdpNetworkMidi2Transport\net2udp_transport_defs.h"
#include "Transports.Network.MidiNetworkTransportSettings.g.h"

namespace winrt::Windows::Devices::Midi2::Transports::Network::implementation
{
    struct MidiNetworkTransportSettings : MidiNetworkTransportSettingsT<MidiNetworkTransportSettings>
    {
        MidiNetworkTransportSettings() = default;

        winrt::guid TransportId() const noexcept { return network::MidiNetworkTransportManager::TransportId(); }

        json::JsonObject ConfigJson();

        uint32_t MaxForwardErrorCorrectionCommandPackets() const noexcept { return m_maxForwardErrorCorrectionCommandPackets; }
        void MaxForwardErrorCorrectionCommandPackets(_In_ uint32_t const value) noexcept { m_maxForwardErrorCorrectionCommandPackets = value; }

        uint32_t MaxRetransmitBufferCommandPackets() const noexcept { return m_maxRetransmitBufferCommandPackets; }
        void MaxRetransmitBufferCommandPackets(_In_ uint32_t const value) noexcept { m_maxRetransmitBufferCommandPackets = value; }

        uint32_t OutboundPingIntervalMilliseconds() const noexcept { return m_outboundPingIntervalMilliseconds; }
        void OutboundPingIntervalMilliseconds(_In_ uint32_t const value) noexcept { m_outboundPingIntervalMilliseconds = value; }

        uint32_t InvitationPendingTimeoutMilliseconds() const noexcept { return m_invitationPendingTimeoutMilliseconds; }
        void InvitationPendingTimeoutMilliseconds(_In_ uint32_t const value) noexcept { m_invitationPendingTimeoutMilliseconds = value; }

        uint32_t MaxHostConnections() const noexcept { return m_maxHostConnections; }
        void MaxHostConnections(_In_ uint32_t const value) noexcept { m_maxHostConnections = value; }

        uint32_t DirectConnectionScanIntervalMilliseconds() const noexcept { return m_directConnectionScanIntervalMilliseconds; }
        void DirectConnectionScanIntervalMilliseconds(_In_ uint32_t const value) noexcept { m_directConnectionScanIntervalMilliseconds = value; }

        static uint32_t MinMaxForwardErrorCorrectionCommandPackets() noexcept { return MIDI_NETWORK_FEC_PACKET_COUNT_LOWER_BOUND; }
        static uint32_t MaxMaxForwardErrorCorrectionCommandPackets() noexcept { return MIDI_NETWORK_FEC_PACKET_COUNT_UPPER_BOUND; }
        static uint32_t MinMaxRetransmitBufferCommandPackets() noexcept { return MIDI_NETWORK_RETRANSMIT_BUFFER_PACKET_COUNT_LOWER_BOUND; }
        static uint32_t MaxMaxRetransmitBufferCommandPackets() noexcept { return MIDI_NETWORK_RETRANSMIT_BUFFER_PACKET_COUNT_UPPER_BOUND; }
        static uint32_t MinOutboundPingIntervalMilliseconds() noexcept { return MIDI_NETWORK_OUTBOUND_PING_INTERVAL_LOWER_BOUND; }
        static uint32_t MaxOutboundPingIntervalMilliseconds() noexcept { return MIDI_NETWORK_OUTBOUND_PING_INTERVAL_UPPER_BOUND; }
        static uint32_t MinInvitationPendingTimeoutMilliseconds() noexcept { return MIDI_NETWORK_INVITATION_PENDING_TIMEOUT_LOWER_BOUND; }
        static uint32_t MaxInvitationPendingTimeoutMilliseconds() noexcept { return MIDI_NETWORK_INVITATION_PENDING_TIMEOUT_UPPER_BOUND; }
        static uint32_t MinMaxHostConnections() noexcept { return MIDI_NETWORK_HOST_MAX_CONNECTIONS_LOWER_BOUND; }
        static uint32_t MaxMaxHostConnections() noexcept { return MIDI_NETWORK_HOST_MAX_CONNECTIONS_ABSOLUTE_MAX; }
        static uint32_t MinDirectConnectionScanIntervalMilliseconds() noexcept { return MIDI_NETWORK_DIRECT_CONNECTION_SCAN_INTERVAL_LOWER_BOUND; }
        static uint32_t MaxDirectConnectionScanIntervalMilliseconds() noexcept { return MIDI_NETWORK_DIRECT_CONNECTION_SCAN_INTERVAL_UPPER_BOUND; }

        void InternalInitialize(_In_ json::JsonObject const& settingsJson) noexcept;

    private:
        uint32_t m_maxForwardErrorCorrectionCommandPackets{ MIDI_NETWORK_FEC_PACKET_COUNT_DEFAULT };
        uint32_t m_maxRetransmitBufferCommandPackets{ MIDI_NETWORK_RETRANSMIT_BUFFER_PACKET_COUNT_DEFAULT };
        uint32_t m_outboundPingIntervalMilliseconds{ MIDI_NETWORK_OUTBOUND_PING_INTERVAL_DEFAULT };
        uint32_t m_invitationPendingTimeoutMilliseconds{ MIDI_NETWORK_INVITATION_PENDING_TIMEOUT_DEFAULT };
        uint32_t m_maxHostConnections{ MIDI_NETWORK_HOST_MAX_CONNECTIONS_DEFAULT };
        uint32_t m_directConnectionScanIntervalMilliseconds{ MIDI_NETWORK_DIRECT_CONNECTION_SCAN_INTERVAL_DEFAULT };
    };
}

namespace winrt::Windows::Devices::Midi2::Transports::Network::factory_implementation
{
    struct MidiNetworkTransportSettings : MidiNetworkTransportSettingsT<MidiNetworkTransportSettings, implementation::MidiNetworkTransportSettings>
    {
    };
}
