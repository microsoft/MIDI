// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App WinRT API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once
#include "Transports.Network.MidiNetworkHostConnection.g.h"

namespace winrt::Windows::Devices::Midi2::Transports::Network::implementation
{
    struct MidiNetworkHostConnection : MidiNetworkHostConnectionT<MidiNetworkHostConnection>
    {
        MidiNetworkHostConnection() = default;

        winrt::hstring UmpEndpointName() const noexcept { return m_umpEndpointName; }
        winrt::hstring ProductInstanceId() const noexcept { return m_productInstanceId; }

        winrt::hstring RemoteAddress() const noexcept { return m_remoteAddress; }
        winrt::hstring RemotePort() const noexcept { return m_remotePort; }

        bool IsSessionActive() const noexcept { return m_isSessionActive; }
        bool IsPendingApproval() const noexcept { return m_isPendingApproval; }

        winrt::hstring EndpointDeviceId() const noexcept { return m_endpointDeviceId; }

        uint64_t CurrentLatencyTicks() const noexcept { return m_currentLatencyTicks; }
        uint32_t RetransmitCount() const noexcept { return m_retransmitCount; }
        uint32_t RetransmitRequestCount() const noexcept { return m_retransmitRequestCount; }
        uint64_t TotalCountNetworkPacketsSent() const noexcept { return m_totalCountNetworkPacketsSent; }
        uint64_t TotalCountNetworkPacketsReceived() const noexcept { return m_totalCountNetworkPacketsReceived; }

        void InternalInitialize(
            _In_ winrt::hstring const& umpEndpointName,
            _In_ winrt::hstring const& productInstanceId,
            _In_ winrt::hstring const& remoteAddress,
            _In_ winrt::hstring const& remotePort,
            _In_ bool const isSessionActive,
            _In_ bool const isPendingApproval,
            _In_ winrt::hstring const& endpointDeviceId,
            _In_ uint64_t const currentLatencyTicks,
            _In_ uint32_t const retransmitCount,
            _In_ uint32_t const retransmitRequestCount,
            _In_ uint64_t const totalCountNetworkPacketsSent,
            _In_ uint64_t const totalCountNetworkPacketsReceived) noexcept
        {
            m_umpEndpointName = umpEndpointName;
            m_productInstanceId = productInstanceId;
            m_remoteAddress = remoteAddress;
            m_remotePort = remotePort;
            m_isSessionActive = isSessionActive;
            m_isPendingApproval = isPendingApproval;
            m_endpointDeviceId = endpointDeviceId;
            m_currentLatencyTicks = currentLatencyTicks;
            m_retransmitCount = retransmitCount;
            m_retransmitRequestCount = retransmitRequestCount;
            m_totalCountNetworkPacketsSent = totalCountNetworkPacketsSent;
            m_totalCountNetworkPacketsReceived = totalCountNetworkPacketsReceived;
        }

    private:
        winrt::hstring m_umpEndpointName{};
        winrt::hstring m_productInstanceId{};
        winrt::hstring m_remoteAddress{};
        winrt::hstring m_remotePort{};
        bool m_isSessionActive{ false };
        bool m_isPendingApproval{ false };
        winrt::hstring m_endpointDeviceId{};

        uint64_t m_currentLatencyTicks{ 0 };
        uint32_t m_retransmitCount{ 0 };
        uint32_t m_retransmitRequestCount{ 0 };
        uint64_t m_totalCountNetworkPacketsSent{ 0 };
        uint64_t m_totalCountNetworkPacketsReceived{ 0 };
    };
}
