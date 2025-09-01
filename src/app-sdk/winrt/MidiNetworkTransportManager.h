// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App SDK and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once
#include "Endpoints.Network.MidiNetworkTransportManager.g.h"

namespace winrt::Microsoft::Windows::Devices::Midi2::Endpoints::Network::implementation
{
    struct MidiNetworkTransportManager
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

        static foundation::IAsyncOperation<MidiNetworkHostUpdateResult> StartNetworkHostAsync(_In_ winrt::hstring const& hostId);
        static foundation::IAsyncOperation<MidiNetworkHostUpdateResult> StopNetworkHostAsync(_In_ winrt::hstring const& hostId);

        static foundation::IAsyncOperation<network::MidiNetworkHostCreationResult> CreateNetworkHostAsync(_In_ network::MidiNetworkHostCreationConfig const& creationConfig) noexcept;
        static foundation::IAsyncOperation<network::MidiNetworkHostRemovalResult> RemoveNetworkHostAsync(_In_ network::MidiNetworkHostRemovalConfig const& removalConfig) noexcept;

        static foundation::IAsyncOperation<network::MidiNetworkClientConnectionResult> ConnectNetworkClientAsync(_In_ network::MidiNetworkClientConnectConfig const& creationConfig) noexcept;
        static foundation::IAsyncOperation<network::MidiNetworkClientConnectionResult> DisconnectNetworkClientAsync(_In_ network::MidiNetworkClientDisconnectConfig const& removalConfig) noexcept;

        static collections::IVectorView<network::MidiNetworkAdvertisedHost> GetAdvertisedHosts() noexcept;


        static collections::IVectorView<network::MidiNetworkConfiguredHost> GetConfiguredHosts() noexcept;
        static collections::IVectorView<network::MidiNetworkConfiguredClient> GetConfiguredClients() noexcept;

    };
}
namespace winrt::Microsoft::Windows::Devices::Midi2::Endpoints::Network::factory_implementation
{
    struct MidiNetworkTransportManager : MidiNetworkTransportManagerT<MidiNetworkTransportManager, implementation::MidiNetworkTransportManager, winrt::static_lifetime>
    {
    };
}
