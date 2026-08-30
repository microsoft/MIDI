// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App WinRT API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once
#include "Transports.Network.MidiNetworkAdvertisedHostRemovedEventArgs.g.h"

namespace winrt::Windows::Devices::Midi2::Transports::Network::implementation
{
    struct MidiNetworkAdvertisedHostRemovedEventArgs : MidiNetworkAdvertisedHostRemovedEventArgsT<MidiNetworkAdvertisedHostRemovedEventArgs>
    {
        MidiNetworkAdvertisedHostRemovedEventArgs() = default;

        winrt::hstring HostDeviceId() noexcept;
        winrt::hstring FullName() noexcept;

        void InternalInitialize(_In_ winrt::hstring const& id, _In_ winrt::hstring const& fullName) noexcept;

    private:
        winrt::hstring m_id{ };
        winrt::hstring m_fullName{ };
    };
}
