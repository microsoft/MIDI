// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App SDK and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once
#include "Endpoints.Network.MidiNetworkAdvertisedHostAddedEventArgs.g.h"

namespace winrt::Microsoft::Windows::Devices::Midi2::Endpoints::Network::implementation
{
    struct MidiNetworkAdvertisedHostAddedEventArgs : MidiNetworkAdvertisedHostAddedEventArgsT<MidiNetworkAdvertisedHostAddedEventArgs>
    {
        MidiNetworkAdvertisedHostAddedEventArgs() = default;

        midi2::Endpoints::Network::MidiNetworkAdvertisedHost AddedHost();
    };
}
