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

        static winrt::hstring MidiNetworkUdpDnsServiceType() { return L"_midi2_udp.local"; }

        static winrt::hstring MidiNetworkUdpDnsSdQueryString()
        { 
            return
                L"System.Devices.AepService.ProtocolId:={4526e8c1-8aac-4153-9b16-55e86ada0e54} AND " \
                L"System.Devices.Dnssd.ServiceName: = \"" + MidiNetworkEndpointManager::MidiNetworkUdpDnsServiceType() + L"\" AND " \
                L"System.Devices.Dnssd.Domain: = \"local\"";
        }

        static collections::IVector<winrt::hstring> MidiNetworkUdpDnsSdQueryAdditionalProperties()
        {
            auto props = winrt::single_threaded_vector<winrt::hstring>();

            props.Append(L"System.Devices.Dnssd.ServiceName");
            props.Append(L"System.Devices.Dnssd.InstanceName");
            props.Append(L"System.Devices.IpAddress");
            props.Append(L"System.Devices.Dnssd.PortNumber");
            props.Append(L"System.Devices.Dnssd.TextAttributes");

            return props;
        }

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
