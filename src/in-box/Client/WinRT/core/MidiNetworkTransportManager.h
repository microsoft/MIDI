// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App WinRT API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once
#include "Transports.Network.MidiNetworkTransportManager.g.h"

#include "..\..\..\Transport\UdpNetworkMidi2Transport\net2udp_transport_defs.h"


namespace winrt::Windows::Devices::Midi2::Transports::Network::implementation
{
    struct MidiNetworkTransportManager
    {
        //MidiNetworkEndpointManager() = default;

        // TODO: may want to move the enumeration functions to another class

        static winrt::hstring MidiNetworkUdpDnsServiceType() noexcept;
        static winrt::hstring MidiNetworkUdpDnsDomain() noexcept;
        static winrt::hstring MidiNetworkUdpDnsSdQueryName() noexcept;

        static uint16_t GenerateAvailableHostPort() noexcept;
        static bool IsHostPortAvailable(_In_ uint16_t const port) noexcept;

        static const winrt::guid TransportId() noexcept { return internal::StringToGuid(MIDI_NETWORK_TRANSPORT_ID); }


        static bool IsTransportAvailable() noexcept;
        
        static foundation::IAsyncOperation<network::MidiNetworkHostCreationResponse> CreateNetworkHostAsync(_In_ network::MidiNetworkHostCreationConfig const& creationConfig) noexcept;
        static foundation::IAsyncOperation<network::MidiNetworkHostRemovalResponse> RemoveNetworkHostAsync(_In_ network::MidiNetworkHostRemovalConfig const& removalConfig) noexcept;

        static foundation::IAsyncOperation<network::MidiNetworkHostUpdateResponse> StartNetworkHostAsync(_In_ winrt::guid const& hostId) noexcept;
        static foundation::IAsyncOperation<network::MidiNetworkHostUpdateResponse> StopNetworkHostAsync(_In_ winrt::guid const& hostId) noexcept;

        static foundation::IAsyncOperation<network::MidiNetworkClientConnectResponse> ConnectNetworkClientAsync(_In_ network::MidiNetworkClientConnectConfig const& creationConfig) noexcept;
        static foundation::IAsyncOperation<network::MidiNetworkClientDisconnectResponse> DisconnectNetworkClientAsync(_In_ network::MidiNetworkClientDisconnectConfig const& removalConfig) noexcept;

        static foundation::IAsyncOperation<network::MidiNetworkRemoteClientApprovalResponse> ApproveOrDenyRemoteClientConnectRequestAsync(_In_ network::MidiNetworkRemoteClientApprovalConfig const& approvalConfig) noexcept;
        static foundation::IAsyncOperation<network::MidiNetworkRemoteClientDisconnectResponse> DisconnectRemoteClientAsync(_In_ network::MidiNetworkRemoteClientDisconnectConfig const& disconnectConfig) noexcept;

        static collections::IVectorView<network::MidiNetworkConfiguredHost> GetConfiguredHosts() noexcept;
        static collections::IVectorView<network::MidiNetworkConfiguredClient> GetConfiguredClients() noexcept;
        static collections::IVectorView<network::MidiNetworkPendingRemoteClient> GetPendingRemoteClients() noexcept;

        static network::MidiNetworkTransportSettings GetTransportSettings() noexcept;


        static collections::IVectorView<network::MidiNetworkAdvertisedHost> GetAdvertisedHosts() noexcept;



    };
}
namespace winrt::Windows::Devices::Midi2::Transports::Network::factory_implementation
{
    struct MidiNetworkTransportManager : MidiNetworkTransportManagerT<MidiNetworkTransportManager, implementation::MidiNetworkTransportManager, winrt::static_lifetime>
    {
    };
}
