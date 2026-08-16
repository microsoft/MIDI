// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App WinRT API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once
#include "Transports.Network.MidiNetworkRemoteClientApprovalConfig.g.h"

namespace winrt::Windows::Devices::Midi2::Transports::Network::implementation
{
    struct MidiNetworkRemoteClientApprovalConfig : MidiNetworkRemoteClientApprovalConfigT<MidiNetworkRemoteClientApprovalConfig>
    {
        MidiNetworkRemoteClientApprovalConfig() = default;

        MidiNetworkRemoteClientApprovalConfig(
            _In_ winrt::guid const& hostId,
            _In_ winrt::hstring const& remoteClientName,
            _In_ winrt::hstring const& remoteClientProductInstanceId,
            _In_ bool const approve,
            _In_ bool const scopeIsThisRequestOnly) noexcept;

        winrt::guid TransportId() const noexcept { return network::MidiNetworkTransportManager::TransportId(); }

        winrt::guid HostId() const noexcept { return m_hostId; }
        void HostId(_In_ winrt::guid const& value) noexcept { m_hostId = value; }

        hstring RemoteClientName() const noexcept { return m_remoteClientName; }
        void RemoteClientName(_In_ winrt::hstring const& value) noexcept { m_remoteClientName = value; }

        hstring RemoteClientProductInstanceId() const noexcept { return m_remoteClientProductInstanceId; }
        void RemoteClientProductInstanceId(_In_ winrt::hstring const& value) noexcept { m_remoteClientProductInstanceId = value; }

        bool Approve() const noexcept { return m_approve; }
        void Approve(_In_ bool const value) noexcept { m_approve = value; }

        bool ScopeIsThisRequestOnly() const noexcept { return m_scopeIsThisRequestOnly; }
        void ScopeIsThisRequestOnly(_In_ bool const value) noexcept { m_scopeIsThisRequestOnly = value; }

        json::JsonObject ConfigJson() noexcept;

    private:
        winrt::guid m_hostId{};
        winrt::guid m_newClientId{};
        hstring m_remoteClientName{};
        hstring m_remoteClientProductInstanceId{};
        bool m_approve{ false };
        bool m_scopeIsThisRequestOnly{ false };


    };
}
namespace winrt::Windows::Devices::Midi2::Transports::Network::factory_implementation
{
    struct MidiNetworkRemoteClientApprovalConfig : MidiNetworkRemoteClientApprovalConfigT<MidiNetworkRemoteClientApprovalConfig, implementation::MidiNetworkRemoteClientApprovalConfig>
    {
    };
}
