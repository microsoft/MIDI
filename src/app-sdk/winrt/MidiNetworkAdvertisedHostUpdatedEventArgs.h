// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App SDK and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once
#include "Endpoints.Network.MidiNetworkAdvertisedHostUpdatedEventArgs.g.h"

namespace winrt::Microsoft::Windows::Devices::Midi2::Endpoints::Network::implementation
{
    struct MidiNetworkAdvertisedHostUpdatedEventArgs : MidiNetworkAdvertisedHostUpdatedEventArgsT<MidiNetworkAdvertisedHostUpdatedEventArgs>
    {
        MidiNetworkAdvertisedHostUpdatedEventArgs() = default;

        winrt::hstring HostId();
        enumeration::DeviceInformationUpdate DeviceInformationUpdate();

        void InternalInitialize(_In_ winrt::hstring const& id, _In_ enumeration::DeviceInformationUpdate const& args) noexcept;

    private:
        winrt::hstring m_id{ };
    };
}
