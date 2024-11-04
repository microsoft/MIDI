// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App SDK and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once
#include "Endpoints.Network.MidiNetworkClientEndpointCreationConfig.g.h"

namespace winrt::Microsoft::Windows::Devices::Midi2::Endpoints::Network::implementation
{
    struct MidiNetworkClientEndpointCreationConfig : MidiNetworkClientEndpointCreationConfigT<MidiNetworkClientEndpointCreationConfig>
    {
        MidiNetworkClientEndpointCreationConfig() = default;

        winrt::guid TransportId();
        hstring GetConfigJson();
    };
}
namespace winrt::Microsoft::Windows::Devices::Midi2::Endpoints::Network::factory_implementation
{
    struct MidiNetworkClientEndpointCreationConfig : MidiNetworkClientEndpointCreationConfigT<MidiNetworkClientEndpointCreationConfig, implementation::MidiNetworkClientEndpointCreationConfig>
    {
    };
}
