// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App WinRT API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once
#include "Transports.Network.MidiNetworkPendingRemoteClient.g.h"

namespace winrt::Windows::Devices::Midi2::Transports::Network::implementation
{
    struct MidiNetworkPendingRemoteClient : MidiNetworkPendingRemoteClientT<MidiNetworkPendingRemoteClient>
    {
        MidiNetworkPendingRemoteClient() = default;

        winrt::guid TransportId() const noexcept { return network::MidiNetworkTransportManager::TransportId(); }

        // A pending client is a report of something the service is holding, not a configuration
        // to send. The decision is made through ApproveOrDenyRemoteClientConnectRequestAsync.
        json::JsonObject ConfigJson() const noexcept { return json::JsonObject{}; }

        winrt::guid HostId() const noexcept { return m_hostId; }

        winrt::hstring HostServiceInstanceName() const noexcept { return m_hostServiceInstanceName; }
        winrt::hstring HostUmpEndpointName() const noexcept { return m_hostUmpEndpointName; }

        winrt::hstring UmpEndpointName() const noexcept { return m_umpEndpointName; }
        winrt::hstring ProductInstanceId() const noexcept { return m_productInstanceId; }
        winrt::hstring RemoteAddress() const noexcept { return m_remoteAddress; }

        foundation::DateTime RequestTime() const noexcept { return m_requestTime; }

        void InternalInitialize(
            _In_ winrt::guid const& hostId,
            _In_ winrt::hstring const& hostServiceInstanceName,
            _In_ winrt::hstring const& hostUmpEndpointName,
            _In_ winrt::hstring const& umpEndpointName,
            _In_ winrt::hstring const& productInstanceId,
            _In_ winrt::hstring const& remoteAddress,
            _In_ foundation::DateTime const requestTime
        ) noexcept
        {
            m_hostId = hostId;
            m_hostServiceInstanceName = hostServiceInstanceName;
            m_hostUmpEndpointName = hostUmpEndpointName;
            m_umpEndpointName = umpEndpointName;
            m_productInstanceId = productInstanceId;
            m_remoteAddress = remoteAddress;
            m_requestTime = requestTime;
        }

    private:
        winrt::guid m_hostId{};

        winrt::hstring m_hostServiceInstanceName{};
        winrt::hstring m_hostUmpEndpointName{};

        winrt::hstring m_umpEndpointName{};
        winrt::hstring m_productInstanceId{};
        winrt::hstring m_remoteAddress{};

        foundation::DateTime m_requestTime{};
    };
}
