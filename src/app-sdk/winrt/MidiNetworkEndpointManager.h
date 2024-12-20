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
    struct MidiNetworkEndpointManager
    {
        //MidiNetworkEndpointManager() = default;

        // TODO: may want to move the enumeration functions to another class

        static winrt::hstring MidiNetworkUdpDnsServiceType();
        static winrt::hstring MidiNetworkUdpDnsDomain();
        static winrt::hstring MidiNetworkUdpDnsSdQueryString();
        static enumeration::DeviceInformationKind MidiNetworkUdpDnsSdDeviceInformationKind();

        static collections::IVector<winrt::hstring> MidiNetworkUdpDnsSdQueryAdditionalProperties();

        static bool IsTransportAvailable();
        
        static const winrt::guid TransportId() noexcept { return internal::StringToGuid(L"{c95dcd1f-cde3-4c2d-913c-528cb8a4cbe6}"); }


        static midi2::Endpoints::Network::MidiNetworkHostEndpointCreationResult CreateNetworkHost(midi2::Endpoints::Network::MidiNetworkHostEndpointCreationConfig const& creationConfig);
        static midi2::Endpoints::Network::MidiNetworkHostEndpointRemovalResult RemoveNetworkHost(midi2::Endpoints::Network::MidiNetworkHostEndpointRemovalConfig const& removalConfig);
        static midi2::Endpoints::Network::MidiNetworkClientEndpointCreationResult CreateNetworkClient(midi2::Endpoints::Network::MidiNetworkClientEndpointCreationConfig const& creationConfig);
        static midi2::Endpoints::Network::MidiNetworkClientEndpointRemovalResult RemoveNetworkClient(midi2::Endpoints::Network::MidiNetworkClientEndpointRemovalConfig const& removalConfig);

        static collections::IVector<midi2::Endpoints::Network::MidiAdvertisedHost> GetAdvertisedHosts();

    };
}
namespace winrt::Microsoft::Windows::Devices::Midi2::Endpoints::Network::factory_implementation
{
    struct MidiNetworkEndpointManager : MidiNetworkEndpointManagerT<MidiNetworkEndpointManager, implementation::MidiNetworkEndpointManager, winrt::static_lifetime>
    {
    };
}
