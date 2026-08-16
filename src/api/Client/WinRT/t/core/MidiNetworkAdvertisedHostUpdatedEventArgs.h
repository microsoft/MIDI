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
        enumeration::DeviceInformationUpdate DeviceInformationUpdate() noexcept;

        void InternalInitialize(_In_ winrt::hstring const& id, _In_ enumeration::DeviceInformationUpdate const& args) noexcept;

    private:
        winrt::hstring m_id{ };
        enumeration::DeviceInformationUpdate m_deviceInformationUpdate{ nullptr };
    };
}
