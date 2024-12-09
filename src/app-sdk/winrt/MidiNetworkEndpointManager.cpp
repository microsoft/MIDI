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
        // TODO: Check to see if service transport is installed and running. May require a new service call
        return true;
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
        auto results = winrt::single_threaded_vector<MidiAdvertisedHost>();

        auto entries = enumeration::DeviceInformation::FindAllAsync(
            MidiNetworkUdpDnsSdQueryString(), 
            MidiNetworkUdpDnsSdQueryAdditionalProperties()).get();

        if (entries && entries.Size() > 0)
        {
            for (auto const& entry : entries)
            {
                MidiAdvertisedHost host;

                if (entry.Properties().HasKey(L"System.Devices.Dnssd.ServiceName"))
                {
                    host.ServiceType = L"Test Service Name";
                }

                if (entry.Properties().HasKey(L"System.Devices.Dnssd.InstanceName"))
                {
                    host.ServiceInstanceName = L"Test Instance Name";
                }

                if (entry.Properties().HasKey(L"System.Devices.IpAddress"))
                {
                    //host.Address = L"";
                }

                if (entry.Properties().HasKey(L"System.Devices.Dnssd.PortNumber"))
                {
                    //host.Port = L"";
                }

                if (entry.Properties().HasKey(L"System.Devices.Dnssd.TextAttributes"))
                {
                    // TODO: Parse out the text entries

                }

                results.Append(host);
            }

        }

        // empty collection if nothing found
        return results;

    }
}
