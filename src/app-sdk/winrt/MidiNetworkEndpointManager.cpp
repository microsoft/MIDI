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
    bool MidiNetworkEndpointManager::IsTransportAvailable() noexcept
    {
        // TODO: Check to see if service transport is installed and running. May require a new service call
        return true;
    }



    _Use_decl_annotations_
    network::MidiNetworkHostCreationResult MidiNetworkEndpointManager::CreateNetworkHost(
        network::MidiNetworkHostCreationConfig const& creationConfig)
    {
        UNREFERENCED_PARAMETER(creationConfig);

        // TEMP
        network::MidiNetworkHostCreationResult result{ };
        return result;
    }

    _Use_decl_annotations_
    network::MidiNetworkHostRemovalResult MidiNetworkEndpointManager::RemoveNetworkHost(
        network::MidiNetworkHostRemovalConfig const& removalConfig)
    {
        UNREFERENCED_PARAMETER(removalConfig);

        // TEMP
        network::MidiNetworkHostRemovalResult result{ };
        return result;
    }

    _Use_decl_annotations_
    network::MidiNetworkClientEndpointCreationResult MidiNetworkEndpointManager::CreateNetworkClient(
        network::MidiNetworkClientEndpointCreationConfig const& creationConfig)
    {
        UNREFERENCED_PARAMETER(creationConfig);

        // TEMP
        network::MidiNetworkClientEndpointCreationResult result{ };
        return result;
    }

    _Use_decl_annotations_
    network::MidiNetworkClientEndpointRemovalResult MidiNetworkEndpointManager::RemoveNetworkClient(
        network::MidiNetworkClientEndpointRemovalConfig const& removalConfig)
    {
        UNREFERENCED_PARAMETER(removalConfig);

        // TEMP
        network::MidiNetworkClientEndpointRemovalResult result{ };
        return result;
    }


    winrt::hstring MidiNetworkEndpointManager::MidiNetworkUdpDnsServiceType() noexcept
    { 
        return L"_midi2._udp"; 
    }

    winrt::hstring MidiNetworkEndpointManager::MidiNetworkUdpDnsDomain() noexcept
    { 
        return L"local"; 
    }

    winrt::hstring MidiNetworkEndpointManager::MidiNetworkUdpDnsSdQueryString() noexcept
    {
        // protocol guid from https://learn.microsoft.com/en-us/windows/uwp/devices-sensors/enumerate-devices-over-a-network

        return
            L"System.Devices.AepService.ProtocolId:={4526e8c1-8aac-4153-9b16-55e86ada0e54} AND " \
            L"System.Devices.Dnssd.ServiceName:=\"" + MidiNetworkEndpointManager::MidiNetworkUdpDnsServiceType() + L"\" AND " \
            L"System.Devices.Dnssd.Domain:=\"" + MidiNetworkEndpointManager::MidiNetworkUdpDnsDomain() + L"\"";
    }

    enumeration::DeviceInformationKind MidiNetworkEndpointManager::MidiNetworkUdpDnsSdDeviceInformationKind() noexcept
    {
        return enumeration::DeviceInformationKind::AssociationEndpointService;
    }


    collections::IVector<winrt::hstring> MidiNetworkEndpointManager::MidiNetworkUdpDnsSdQueryAdditionalProperties() noexcept
    {
        auto props = winrt::single_threaded_vector<winrt::hstring>();

        // https://learn.microsoft.com/en-us/windows/win32/properties/props-system-devices-dnssd-domain
        props.Append(L"System.Devices.AepService.ProtocolId");  // guid
        props.Append(L"System.Devices.Dnssd.HostName");         // string
        props.Append(L"System.Devices.Dnssd.FullName");         // string
        props.Append(L"System.Devices.Dnssd.ServiceName");      // string
        props.Append(L"System.Devices.Dnssd.Domain");           // string
        props.Append(L"System.Devices.Dnssd.InstanceName");     // string
        props.Append(L"System.Devices.IpAddress");              // multivalue string
        props.Append(L"System.Devices.Dnssd.PortNumber");       // uint16_t
        props.Append(L"System.Devices.Dnssd.TextAttributes");   // multivalue string

        return props;
    }



	// this method takes way too long. This needs to be changed to pull from the cache in the 
	// service via the json methods.

    collections::IVectorView<network::MidiNetworkAdvertisedHost> MidiNetworkEndpointManager::GetAdvertisedHosts() noexcept
    {
        auto results = winrt::single_threaded_vector<network::MidiNetworkAdvertisedHost>();

        auto entries = enumeration::DeviceInformation::FindAllAsync(
            MidiNetworkUdpDnsSdQueryString(), 
            MidiNetworkUdpDnsSdQueryAdditionalProperties(),
            MidiNetworkUdpDnsSdDeviceInformationKind()
        ).get();

        if (entries && entries.Size() > 0)
        {
            for (auto const& entry : entries)
            {
                auto host = winrt::make_self<network::implementation::MidiNetworkAdvertisedHost>();

                host->InternalUpdateFromDeviceInformation(entry);

                results.Append(*host);
            }

        }

        // empty collection if nothing found
        return results.GetView();

    }
}
