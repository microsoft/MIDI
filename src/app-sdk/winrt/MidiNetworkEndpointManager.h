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

        static winrt::hstring MidiNetworkUdpDnsServiceType() noexcept;
        static winrt::hstring MidiNetworkUdpDnsDomain() noexcept;
        static winrt::hstring MidiNetworkUdpDnsSdQueryString() noexcept;
        static enumeration::DeviceInformationKind MidiNetworkUdpDnsSdDeviceInformationKind() noexcept;

        static collections::IVector<winrt::hstring> MidiNetworkUdpDnsSdQueryAdditionalProperties() noexcept;

        static bool IsTransportAvailable() noexcept;
        
        static const winrt::guid TransportId() noexcept { return internal::StringToGuid(MIDI_NETWORK_TRANSPORT_ID); }


        static network::MidiNetworkHostCreationResult CreateNetworkHost(_In_ network::MidiNetworkHostCreationConfig const& creationConfig);
        static network::MidiNetworkHostRemovalResult RemoveNetworkHost(_In_ network::MidiNetworkHostRemovalConfig const& removalConfig);

        static network::MidiNetworkClientEndpointCreationResult CreateNetworkClient(_In_ network::MidiNetworkClientEndpointCreationConfig const& creationConfig);
        static network::MidiNetworkClientEndpointRemovalResult RemoveNetworkClient(_In_ network::MidiNetworkClientEndpointRemovalConfig const& removalConfig);

        static collections::IVectorView<network::MidiNetworkAdvertisedHost> GetAdvertisedHosts() noexcept;
    };
}
namespace winrt::Microsoft::Windows::Devices::Midi2::Endpoints::Network::factory_implementation
{
    struct MidiNetworkEndpointManager : MidiNetworkEndpointManagerT<MidiNetworkEndpointManager, implementation::MidiNetworkEndpointManager, winrt::static_lifetime>
    {
    };
}
