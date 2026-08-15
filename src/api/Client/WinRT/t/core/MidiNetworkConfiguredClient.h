// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App WinRT API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once
#include "Transports.Network.MidiNetworkConfiguredClient.g.h"

namespace winrt::Windows::Devices::Midi2::Transports::Network::implementation
{
    struct MidiNetworkConfiguredClient : MidiNetworkConfiguredClientT<MidiNetworkConfiguredClient>
    {
        MidiNetworkConfiguredClient() = default;

        winrt::guid ClientId() const noexcept { return m_clientId; }
        winrt::guid HostId() const noexcept { return m_hostId; }

        bool IsSessionActive() const noexcept { return m_isSessionActive; }

        network::MidiNetworkClientEntryState EntryState() const noexcept { return m_entryState; }

        bool IsDirectConnection() const noexcept { return m_isDirectConnection; }
        winrt::hstring ConfiguredDirectAddress() const noexcept { return m_configuredDirectAddress; }
        winrt::hstring ConfiguredDirectPort() const noexcept { return m_configuredDirectPort; }

        winrt::hstring ConnectedRemoteAddress() const noexcept { return m_connectedRemoteAddress; }
        winrt::hstring ConnectedRemotePort() const noexcept { return m_connectedRemotePort; }
        winrt::hstring ConnectedLocalAddress() const noexcept { return m_connectedLocalAddress; }
        winrt::hstring ConnectedLocalPort() const noexcept { return m_connectedLocalPort; }

        winrt::hstring  EndpointDeviceId() const noexcept { return m_endpointDeviceId; }

        uint32_t RetransmitCount() const noexcept { return m_retransmitCount; }
        uint32_t RetransmitRequestCount() const noexcept { return m_retransmitRequestCount; }
        uint64_t CurrentLatencyTicks() const noexcept { return m_currentLatencyTicks; }
        uint64_t TotalCountNetworkPacketsSent() const noexcept { return m_totalCountNetworkPacketsSent; }
        uint64_t TotalCountNetworkPacketsReceived() const noexcept { return m_totalCountNetworkPacketsReceived; }


        void InternalInitialize(
            _In_ winrt::guid const& clientId,
            _In_ winrt::guid const& hostId,
            _In_ bool const isSessionActive,
            _In_ winrt::hstring const& connectedRemoteAddress,
            _In_ winrt::hstring const& connectedRemotePort,
            _In_ winrt::hstring const& connectedLocalAddress,
            _In_ winrt::hstring const& connectedLocalPort,
            _In_ winrt::hstring const& endpointDeviceId,

            _In_ uint64_t const currentLatencyTicks,
            _In_ uint32_t const retransmitCount,
            _In_ uint32_t const retransmitRequestCount,

            _In_ uint64_t const totalCountNetworkPacketsSent,
            _In_ uint64_t const totalCountNetworkPacketsReceived,

            _In_ network::MidiNetworkClientEntryState const entryState,
            _In_ bool const isDirectConnection,
            _In_ winrt::hstring const& configuredDirectAddress,
            _In_ winrt::hstring const& configuredDirectPort
        ) noexcept
        {
            m_clientId = clientId;
            m_hostId = hostId;
            m_isSessionActive = isSessionActive;
            m_connectedRemoteAddress = connectedRemoteAddress;
            m_connectedRemotePort = connectedRemotePort;
            m_connectedLocalAddress = connectedLocalAddress;
            m_connectedLocalPort = connectedLocalPort;
            m_endpointDeviceId = endpointDeviceId;
            m_currentLatencyTicks = currentLatencyTicks;
            m_retransmitCount = retransmitCount;
            m_retransmitRequestCount = retransmitRequestCount;
            m_totalCountNetworkPacketsSent = totalCountNetworkPacketsSent;
            m_totalCountNetworkPacketsReceived = totalCountNetworkPacketsReceived;
            m_entryState = entryState;
            m_isDirectConnection = isDirectConnection;
            m_configuredDirectAddress = configuredDirectAddress;
            m_configuredDirectPort = configuredDirectPort;
        }

    private:
        winrt::guid m_clientId{};
        winrt::guid m_hostId{};
        bool m_isSessionActive{ false };
        winrt::hstring m_connectedRemoteAddress{};
        winrt::hstring m_connectedRemotePort{};
        winrt::hstring m_connectedLocalAddress{};
        winrt::hstring m_connectedLocalPort{};
        winrt::hstring m_endpointDeviceId{};

        uint32_t m_retransmitCount{ 0 };
        uint32_t m_retransmitRequestCount{ 0 };
        uint64_t m_currentLatencyTicks{ 0 };
        uint64_t m_totalCountNetworkPacketsSent{ 0 };
        uint64_t m_totalCountNetworkPacketsReceived{ 0 };

        network::MidiNetworkClientEntryState m_entryState{ network::MidiNetworkClientEntryState::Pending };
        bool m_isDirectConnection{ false };
        winrt::hstring m_configuredDirectAddress{};
        winrt::hstring m_configuredDirectPort{};
    };
}
