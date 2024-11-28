// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App SDK and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

#include "pch.h"
#include "MidiNetworkEndpointManager.h"
#include "Endpoints.Network.MidiNetworkEndpointManager.g.cpp"

namespace winrt::Microsoft::Windows::Devices::Midi2::Endpoints::Network::implementation
{
    bool MidiNetworkEndpointManager::IsTransportAvailable()
    {
        throw hresult_not_implemented();
    }
    winrt::guid MidiNetworkEndpointManager::TransportId()
    {
        throw hresult_not_implemented();
    }

    _Use_decl_annotations_
    network::MidiNetworkHostEndpointCreationResult MidiNetworkEndpointManager::CreateNetworkHost(
        network::MidiNetworkHostEndpointCreationConfig const& creationConfig)
    {
        UNREFERENCED_PARAMETER(creationConfig);
        throw hresult_not_implemented();
    }

    _Use_decl_annotations_
    network::MidiNetworkHostEndpointRemovalResult MidiNetworkEndpointManager::RemoveNetworkHost(
        network::MidiNetworkHostEndpointRemovalConfig const& removalConfig)
    {
        UNREFERENCED_PARAMETER(removalConfig);
        throw hresult_not_implemented();
    }

    _Use_decl_annotations_
    network::MidiNetworkClientEndpointCreationResult MidiNetworkEndpointManager::CreateNetworkClient(
        network::MidiNetworkClientEndpointCreationConfig const& creationConfig)
    {
        UNREFERENCED_PARAMETER(creationConfig);
        throw hresult_not_implemented();
    }

    _Use_decl_annotations_
    network::MidiNetworkClientEndpointRemovalResult MidiNetworkEndpointManager::RemoveNetworkClient(
        network::MidiNetworkClientEndpointRemovalConfig const& removalConfig)
    {
        UNREFERENCED_PARAMETER(removalConfig);
        throw hresult_not_implemented();
    }





    collections::IVector<midi2::Endpoints::Network::MidiAdvertisedHost> MidiNetworkEndpointManager::GetAdvertisedHosts()
    {
        //var askFor = new String[]{ "System.Devices.Dnssd.HostName",
        //                    "System.Devices.Dnssd.ServiceName",
        //                    "System.Devices.Dnssd.InstanceName",
        //                    "System.Devices.IpAddress",
        //                    "System.Devices.Dnssd.PortNumber",
        //                    "System.Devices.Dnssd.TextAttributes",
        //};


        auto results = enumeration::DeviceInformation::FindAllAsync(
            MidiNetworkUdpDnsSdQueryString(), 
            MidiNetworkUdpDnsSdQueryAdditionalProperties()).get();

        if (results && results.Size() > 0)
        {
            for (auto const& result : results)
            {
                if (result.Properties().HasKey(L"System.Devices.Dnssd.ServiceName"))
                {

                }

                if (result.Properties().HasKey(L"System.Devices.Dnssd.InstanceName"))
                {

                }

                if (result.Properties().HasKey(L"System.Devices.IpAddress"))
                {

                }

                if (result.Properties().HasKey(L"System.Devices.Dnssd.PortNumber"))
                {

                }

                if (result.Properties().HasKey(L"System.Devices.Dnssd.TextAttributes"))
                {

                }
            }

        }

        //var askFor = new String[]{ "System.Devices.Dnssd.HostName",
        //                    "System.Devices.Dnssd.ServiceName",
        //                    "System.Devices.Dnssd.InstanceName",
        //                    "System.Devices.IpAddress",
        //                    "System.Devices.Dnssd.PortNumber",
        //                    "System.Devices.Dnssd.TextAttributes",
        //};
        //dw = DeviceInformation.CreateWatcher(queryString, askFor, DeviceInformationKind.AssociationEndpointService);

        throw hresult_not_implemented();

    }
}
