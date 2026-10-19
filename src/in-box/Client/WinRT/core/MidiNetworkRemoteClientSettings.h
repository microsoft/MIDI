// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App WinRT API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once
#include "Transports.Network.MidiNetworkRemoteClientSettings.g.h"

namespace winrt::Windows::Devices::Midi2::Transports::Network::implementation
{
    struct MidiNetworkRemoteClientSettings : MidiNetworkRemoteClientSettingsT<MidiNetworkRemoteClientSettings>
    {
        MidiNetworkRemoteClientSettings() = default;

        MidiNetworkRemoteClientSettings(
            _In_ winrt::hstring const& remoteClientName,
            _In_ winrt::hstring const& remoteClientProductInstanceId) noexcept :
            m_remoteClientName(remoteClientName),
            m_remoteClientProductInstanceId(remoteClientProductInstanceId)
        {
        }

        winrt::hstring RemoteClientName() const noexcept { return m_remoteClientName; }
        void RemoteClientName(_In_ winrt::hstring const& value) noexcept { m_remoteClientName = value; }

        winrt::hstring RemoteClientProductInstanceId() const noexcept { return m_remoteClientProductInstanceId; }
        void RemoteClientProductInstanceId(_In_ winrt::hstring const& value) noexcept { m_remoteClientProductInstanceId = value; }

        network::MidiNetworkSendSpeedLimit SendSpeedLimit() const noexcept { return m_sendSpeedLimit; }
        void SendSpeedLimit(_In_ network::MidiNetworkSendSpeedLimit const& value) noexcept { m_sendSpeedLimit = value; }

        bool ReduceSendSpeedAutomatically() const noexcept { return m_reduceSendSpeedAutomatically; }
        void ReduceSendSpeedAutomatically(_In_ bool const value) noexcept { m_reduceSendSpeedAutomatically = value; }

        bool SkipInBetweenValuesToCatchUp() const noexcept { return m_skipInBetweenValuesToCatchUp; }
        void SkipInBetweenValuesToCatchUp(_In_ bool const value) noexcept { m_skipInBetweenValuesToCatchUp = value; }

        bool UseHostSendSpeed() const noexcept { return m_useHostSendSpeed; }
        void UseHostSendSpeed(_In_ bool const value) noexcept { m_useHostSendSpeed = value; }

        collections::IVector<network::MidiNetworkMidi1PortGroup> FallbackMidi1PortGroups() const noexcept { return m_fallbackMidi1PortGroups; }

    private:
        winrt::hstring m_remoteClientName{};
        winrt::hstring m_remoteClientProductInstanceId{};
        network::MidiNetworkSendSpeedLimit m_sendSpeedLimit{ network::MidiNetworkSendSpeedLimit::Unlimited };
        bool m_reduceSendSpeedAutomatically{ false };
        bool m_skipInBetweenValuesToCatchUp{ true };
        bool m_useHostSendSpeed{ false };

        collections::IVector<network::MidiNetworkMidi1PortGroup> m_fallbackMidi1PortGroups{
            winrt::single_threaded_vector<network::MidiNetworkMidi1PortGroup>() };
    };
}

namespace winrt::Windows::Devices::Midi2::Transports::Network::factory_implementation
{
    struct MidiNetworkRemoteClientSettings : MidiNetworkRemoteClientSettingsT<MidiNetworkRemoteClientSettings, implementation::MidiNetworkRemoteClientSettings>
    {
    };
}
