// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App SDK and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================


#pragma once
#include "Endpoints.Network.MidiNetworkEndpointManager.g.h"

namespace winrt::Microsoft::Windows::Devices::Midi2::Endpoints::Network::implementation
{
    struct MidiNetworkEndpointManager : MidiNetworkEndpointManagerT<MidiNetworkEndpointManager>
    {
        MidiNetworkEndpointManager() = default;

        static bool IsTransportAvailable();
        static winrt::guid AbstractionId();
        static winrt::Microsoft::Windows::Devices::Midi2::Endpoints::Network::MidiNetworkHostEndpointCreationResult CreateNetworkHost(winrt::Microsoft::Windows::Devices::Midi2::Endpoints::Network::MidiNetworkHostEndpointCreationConfig const& creationConfig);
        static winrt::Microsoft::Windows::Devices::Midi2::Endpoints::Network::MidiNetworkHostEndpointRemovalResult RemoveNetworkHost(winrt::Microsoft::Windows::Devices::Midi2::Endpoints::Network::MidiNetworkHostEndpointRemovalConfig const& removalConfig);
        static winrt::Microsoft::Windows::Devices::Midi2::Endpoints::Network::MidiNetworkClientEndpointCreationResult CreateNetworkClient(winrt::Microsoft::Windows::Devices::Midi2::Endpoints::Network::MidiNetworkClientEndpointCreationConfig const& creationConfig);
        static winrt::Microsoft::Windows::Devices::Midi2::Endpoints::Network::MidiNetworkClientEndpointRemovalResult RemoveNetworkClient(winrt::Microsoft::Windows::Devices::Midi2::Endpoints::Network::MidiNetworkClientEndpointRemovalConfig const& removalConfig);
    };
}
namespace winrt::Microsoft::Windows::Devices::Midi2::Endpoints::Network::factory_implementation
{
    struct MidiNetworkEndpointManager : MidiNetworkEndpointManagerT<MidiNetworkEndpointManager, implementation::MidiNetworkEndpointManager>
    {
    };
}
