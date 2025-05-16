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

    collections::IVector<midi2::Endpoints::Network::MidiNetworkAdvertisedHost> MidiNetworkEndpointManager::GetAdvertisedHosts() noexcept
    {
        auto results = winrt::single_threaded_vector<MidiNetworkAdvertisedHost>();

        auto entries = enumeration::DeviceInformation::FindAllAsync(
            MidiNetworkUdpDnsSdQueryString(), 
            MidiNetworkUdpDnsSdQueryAdditionalProperties(),
            MidiNetworkUdpDnsSdDeviceInformationKind()
        ).get();

        if (entries && entries.Size() > 0)
        {
            for (auto const& entry : entries)
            {
                MidiNetworkAdvertisedHost host;

                //props.Append(L"System.Devices.AepService.ProtocolId");  // guid
                //props.Append(L"System.Devices.Dnssd.HostName");         // string
                //props.Append(L"System.Devices.Dnssd.FullName");         // string
                //props.Append(L"System.Devices.Dnssd.ServiceName");      // string
                //props.Append(L"System.Devices.Dnssd.Domain");           // string
                //props.Append(L"System.Devices.Dnssd.InstanceName");     // string
                //props.Append(L"System.Devices.IpAddress");              // multivalue string
                //props.Append(L"System.Devices.Dnssd.PortNumber");       // uint16_t
                //props.Append(L"System.Devices.Dnssd.TextAttributes");   // multivalue string

                host.DeviceId = entry.Id();
                host.DeviceName = entry.Name();

                host.HostName = internal::GetDeviceInfoProperty<winrt::hstring>(entry.Properties(), L"System.Devices.Dnssd.HostName", L"");
                host.FullName = internal::GetDeviceInfoProperty<winrt::hstring>(entry.Properties(), L"System.Devices.Dnssd.FullName", L"");
                host.ServiceType = internal::GetDeviceInfoProperty<winrt::hstring>(entry.Properties(), L"System.Devices.Dnssd.ServiceName", L"");
                host.Domain = internal::GetDeviceInfoProperty<winrt::hstring>(entry.Properties(), L"System.Devices.Dnssd.Domain", L"");
                host.ServiceInstanceName = internal::GetDeviceInfoProperty<winrt::hstring>(entry.Properties(), L"System.Devices.Dnssd.InstanceName", L"");
                host.Port = internal::GetDeviceInfoProperty<uint16_t>(entry.Properties(), L"System.Devices.Dnssd.PortNumber", 0);

                if (entry.Properties().HasKey(L"System.Devices.IpAddress"))
                {
                    auto prop = entry.Properties().Lookup(L"System.Devices.IpAddress").as<foundation::IReferenceArray<winrt::hstring>>();
                    winrt::com_array<winrt::hstring> array;
                    prop.GetStringArray(array);

                    // we only take the top one right now. We should take the others as well
                    if (array.size() > 0)
                    {
                        // TEMP! We'll need to move these to a vector
                        for (auto const& ip : array)
                        {
                            if (host.IPAddress.empty())
                            {
                                host.IPAddress = ip;
                            }
                            else
                            {
                                host.IPAddress = host.IPAddress + L" | " + ip;
                            }
                        }
                        
                    }
                }

                if (entry.Properties().HasKey(L"System.Devices.Dnssd.TextAttributes"))
                {
                    auto txtEntryProp = entry.Properties().Lookup(L"System.Devices.Dnssd.TextAttributes").as<foundation::IReferenceArray<winrt::hstring>>();
                    winrt::com_array<winrt::hstring> txtEntryArray;
                    txtEntryProp.GetStringArray(txtEntryArray);

                    for (auto const& txt : txtEntryArray)
                    {
                        // TODO: we potentially lose info here by converting from a wide string. Need to check spec to see if ascii-only.
                        auto s = winrt::to_string(txt);

                        // split on the = sign
                        std::string name = s.substr(0, s.find("="));
                        std::string value = s.substr(s.find("=") + 1);

                        if (name == "UMPEndpointName")
                        {
                            host.UmpEndpointName = winrt::to_hstring(value);
                        }
                        else if (name == "ProductInstanceId")
                        {
                            host.ProductInstanceId = winrt::to_hstring(value);
                        }
                    }

                }

                results.Append(host);
            }

        }

        // empty collection if nothing found
        return results;

    }
}
