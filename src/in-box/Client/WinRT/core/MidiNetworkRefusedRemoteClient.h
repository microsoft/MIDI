// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App WinRT API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once
#include "Transports.Network.MidiNetworkRefusedRemoteClient.g.h"

namespace winrt::Windows::Devices::Midi2::Transports::Network::implementation
{
    struct MidiNetworkRefusedRemoteClient : MidiNetworkRefusedRemoteClientT<MidiNetworkRefusedRemoteClient>
    {
        MidiNetworkRefusedRemoteClient() = default;

        winrt::guid HostId() const noexcept { return m_hostId; }

        winrt::hstring UmpEndpointName() const noexcept { return m_umpEndpointName; }
        winrt::hstring ProductInstanceId() const noexcept { return m_productInstanceId; }
        winrt::hstring RemoteAddress() const noexcept { return m_remoteAddress; }

        foundation::DateTime RefusedTime() const noexcept { return m_refusedTime; }

        void InternalInitialize(
            _In_ winrt::guid const& hostId,
            _In_ winrt::hstring const& umpEndpointName,
            _In_ winrt::hstring const& productInstanceId,
            _In_ winrt::hstring const& remoteAddress,
            _In_ foundation::DateTime const refusedTime
        ) noexcept
        {
            m_hostId = hostId;
            m_umpEndpointName = umpEndpointName;
            m_productInstanceId = productInstanceId;
            m_remoteAddress = remoteAddress;
            m_refusedTime = refusedTime;
        }

    private:
        winrt::guid m_hostId{};

        winrt::hstring m_umpEndpointName{};
        winrt::hstring m_productInstanceId{};
        winrt::hstring m_remoteAddress{};

        foundation::DateTime m_refusedTime{};
    };
}
