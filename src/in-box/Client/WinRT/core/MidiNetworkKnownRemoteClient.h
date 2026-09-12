// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App WinRT API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once
#include "Transports.Network.MidiNetworkKnownRemoteClient.g.h"

namespace winrt::Windows::Devices::Midi2::Transports::Network::implementation
{
    struct MidiNetworkKnownRemoteClient : MidiNetworkKnownRemoteClientT<MidiNetworkKnownRemoteClient>
    {
        MidiNetworkKnownRemoteClient() = default;

        MidiNetworkKnownRemoteClient(
            _In_ winrt::hstring const& remoteClientName,
            _In_ winrt::hstring const& remoteClientProductInstanceId,
            _In_ bool const isAllowed) noexcept :
            m_remoteClientName(remoteClientName),
            m_remoteClientProductInstanceId(remoteClientProductInstanceId),
            m_isAllowed(isAllowed)
        {
        }

        winrt::hstring RemoteClientName() const noexcept { return m_remoteClientName; }
        void RemoteClientName(_In_ winrt::hstring const& value) noexcept { m_remoteClientName = value; }

        winrt::hstring RemoteClientProductInstanceId() const noexcept { return m_remoteClientProductInstanceId; }
        void RemoteClientProductInstanceId(_In_ winrt::hstring const& value) noexcept { m_remoteClientProductInstanceId = value; }

        bool IsAllowed() const noexcept { return m_isAllowed; }
        void IsAllowed(_In_ bool const value) noexcept { m_isAllowed = value; }

    private:
        winrt::hstring m_remoteClientName{};
        winrt::hstring m_remoteClientProductInstanceId{};
        bool m_isAllowed{ true };
    };
}

namespace winrt::Windows::Devices::Midi2::Transports::Network::factory_implementation
{
    struct MidiNetworkKnownRemoteClient : MidiNetworkKnownRemoteClientT<MidiNetworkKnownRemoteClient, implementation::MidiNetworkKnownRemoteClient>
    {
    };
}
