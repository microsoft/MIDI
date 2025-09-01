// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App SDK and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once
#include "Endpoints.Network.MidiNetworkHostRemovalConfig.g.h"

namespace winrt::Microsoft::Windows::Devices::Midi2::Endpoints::Network::implementation
{
    struct MidiNetworkHostRemovalConfig : MidiNetworkHostRemovalConfigT<MidiNetworkHostRemovalConfig>
    {
        MidiNetworkHostRemovalConfig() = default;

        winrt::guid TransportId() const noexcept { return internal::StringToGuid(MIDI_NETWORK_TRANSPORT_ID); }
        
        json::JsonObject GetConfigJson() const noexcept;
    };
}
namespace winrt::Microsoft::Windows::Devices::Midi2::Endpoints::Network::factory_implementation
{
    struct MidiNetworkHostRemovalConfig : MidiNetworkHostRemovalConfigT<MidiNetworkHostRemovalConfig, implementation::MidiNetworkHostRemovalConfig>
    {
    };
}
