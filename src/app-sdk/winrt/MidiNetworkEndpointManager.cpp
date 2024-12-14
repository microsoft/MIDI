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
            MidiNetworkUdpDnsSdQueryAdditionalProperties(),
            MidiNetworkUdpDnsSdDeviceInformationKind()
        ).get();

        if (entries && entries.Size() > 0)
        {
            for (auto const& entry : entries)
            {
                MidiAdvertisedHost host;

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
                        host.IPAddress = array.at(0);
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
