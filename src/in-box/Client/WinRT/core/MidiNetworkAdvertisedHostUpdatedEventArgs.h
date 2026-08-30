// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App WinRT API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once
#include "Transports.Network.MidiNetworkAdvertisedHostUpdatedEventArgs.g.h"

namespace winrt::Windows::Devices::Midi2::Transports::Network::implementation
{
    struct MidiNetworkAdvertisedHostUpdatedEventArgs : MidiNetworkAdvertisedHostUpdatedEventArgsT<MidiNetworkAdvertisedHostUpdatedEventArgs>
    {
        MidiNetworkAdvertisedHostUpdatedEventArgs() = default;

        winrt::hstring HostDeviceId() noexcept;
        network::MidiNetworkAdvertisedHostChangedProperties ChangedProperties() noexcept;
        network::MidiNetworkAdvertisedHost UpdatedHost() noexcept;

        void InternalInitialize(
            _In_ winrt::hstring const& id,
            _In_ network::MidiNetworkAdvertisedHostChangedProperties const changedProperties,
            _In_ network::MidiNetworkAdvertisedHost const& host) noexcept;

    private:
        winrt::hstring m_id{ };
        network::MidiNetworkAdvertisedHostChangedProperties m_changedProperties{ network::MidiNetworkAdvertisedHostChangedProperties::None };
        network::MidiNetworkAdvertisedHost m_host{ nullptr };
    };
}
