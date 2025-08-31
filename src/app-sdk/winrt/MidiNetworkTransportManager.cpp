// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App SDK and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

#include "pch.h"
#include "MidiNetworkTransportManager.h"
#include "Endpoints.Network.MidiNetworkTransportManager.g.cpp"

#include "network_json_defs.h"

//#include <pplawait.h>

namespace winrt::Microsoft::Windows::Devices::Midi2::Endpoints::Network::implementation
{
    bool MidiNetworkTransportManager::IsTransportAvailable() noexcept
    {
        // TODO: Check to see if service transport is installed and running. May require a new service call

        // maybe just enumerate transports and check for this one, using existing calls

        return true;
    }


    _Use_decl_annotations_ 
    foundation::IAsyncOperation<MidiNetworkHostUpdateResult> MidiNetworkTransportManager::StartNetworkHostAsync(winrt::hstring const& hostId)
    {
        midi2::ServiceConfig::MidiServiceTransportCommand command(network::MidiNetworkTransportManager::TransportId());

        command.Verb(MIDI_CONFIG_JSON_NETWORK_MIDI_COMMAND_VERB_START_HOST);
        command.Arguments().Insert(
            MIDI_CONFIG_JSON_NETWORK_MIDI_COMMAND_PARAMETER_HOST_ENTRY_IDENTIFIER,
            hostId);

        co_await winrt::resume_background();

        // this could take a few since it closes all the connections synchronously in the service
        auto response = midi2::ServiceConfig::MidiServiceConfig::SendTransportCommand(command);

        MidiNetworkHostUpdateResult result;
        
        if (response.Status == midi2::ServiceConfig::MidiServiceConfigResponseStatus::Success)
        {
            result.Success = true;
        }
        else
        {
            result.Success = false;
            result.ErrorInformation = response.ServiceMessage;
        }

        co_return result;
    }
    
    _Use_decl_annotations_ 
    foundation::IAsyncOperation<MidiNetworkHostUpdateResult> MidiNetworkTransportManager::StopNetworkHostAsync(winrt::hstring const& hostId)
    {
        midi2::ServiceConfig::MidiServiceTransportCommand command(network::MidiNetworkTransportManager::TransportId());

        command.Verb(MIDI_CONFIG_JSON_NETWORK_MIDI_COMMAND_VERB_STOP_HOST);
        command.Arguments().Insert(
            MIDI_CONFIG_JSON_NETWORK_MIDI_COMMAND_PARAMETER_HOST_ENTRY_IDENTIFIER,
            hostId);

        co_await winrt::resume_background();

        // this could take a few since it closes all the connections synchronously in the service
        auto response = midi2::ServiceConfig::MidiServiceConfig::SendTransportCommand(command);

        MidiNetworkHostUpdateResult result;

        if (response.Status == midi2::ServiceConfig::MidiServiceConfigResponseStatus::Success)
        {
            result.Success = true;
        }
        else
        {
            result.Success = false;
            result.ErrorInformation = response.ServiceMessage;
        }

        co_return result;
    }


    collections::IVectorView<network::MidiNetworkConfiguredHost> MidiNetworkTransportManager::GetConfiguredHosts() noexcept
    {
        midi2::ServiceConfig::MidiServiceTransportCommand command(network::MidiNetworkTransportManager::TransportId());
        command.Verb(MIDI_CONFIG_JSON_NETWORK_MIDI_COMMAND_VERB_ENUMERATE_HOSTS);

        OutputDebugString(L"\n");
        OutputDebugString(command.GetConfigJson().Stringify().c_str());
        OutputDebugString(L"\n");

        auto response = midi2::ServiceConfig::MidiServiceConfig::SendTransportCommand(command);

        auto results = winrt::single_threaded_vector<network::MidiNetworkConfiguredHost>();

        if (response.Status == midi2::ServiceConfig::MidiServiceConfigResponseStatus::Success)
        {
            auto responseJson = response.ResponseJson;

            json::JsonObject responseObject;
            if (json::JsonObject::TryParse(responseJson, responseObject))
            {
                if (responseObject.HasKey(MIDI_CONFIG_JSON_NETWORK_MIDI_ENUM_HOSTS_RESPONSE_HOSTS_ARRAY_KEY))
                {
                    auto hostsArray = responseObject.GetNamedArray(MIDI_CONFIG_JSON_NETWORK_MIDI_ENUM_HOSTS_RESPONSE_HOSTS_ARRAY_KEY);

                    for (auto const& entry : hostsArray)
                    {
                        auto entryObject = entry.GetObject();

                        if (entryObject != nullptr)
                        {
                            network::MidiNetworkConfiguredHost host;

                            host.Id = entryObject.GetNamedString(MIDI_CONFIG_JSON_NETWORK_MIDI_ENUM_HOSTS_RESPONSE_CONFIG_ID_KEY, L"");
                            host.IsEnabled = entryObject.GetNamedBoolean(MIDI_CONFIG_JSON_NETWORK_MIDI_ENUM_HOSTS_RESPONSE_IS_ENABLED_KEY, false);
                            host.HasStarted = entryObject.GetNamedBoolean(MIDI_CONFIG_JSON_NETWORK_MIDI_ENUM_HOSTS_RESPONSE_HAS_STARTED_KEY, false);
                            host.ActualAddress = entryObject.GetNamedString(MIDI_CONFIG_JSON_NETWORK_MIDI_ENUM_HOSTS_RESPONSE_ACTUAL_ADDRESS_KEY, L"");
                            host.ActualPort = entryObject.GetNamedString(MIDI_CONFIG_JSON_NETWORK_MIDI_ENUM_HOSTS_RESPONSE_ACTUAL_PORT_KEY, L"");
                            host.UmpEndpointName = entryObject.GetNamedString(MIDI_CONFIG_JSON_NETWORK_MIDI_ENUM_HOSTS_RESPONSE_NAME_KEY, L"");
                            host.ProductInstanceId = entryObject.GetNamedString(MIDI_CONFIG_JSON_NETWORK_MIDI_ENUM_HOSTS_RESPONSE_PRODUCT_INSTANCE_ID_KEY, L"");
                            host.CreateMidi1Ports = entryObject.GetNamedBoolean(MIDI_CONFIG_JSON_NETWORK_MIDI_ENUM_HOSTS_RESPONSE_CREATE_MIDI1_PORTS_KEY, false);
                            host.ServiceInstanceName = entryObject.GetNamedString(MIDI_CONFIG_JSON_NETWORK_MIDI_ENUM_HOSTS_RESPONSE_SERVICE_INSTANCE_NAME_KEY, L"");

                            results.Append(host);
                        }
                    }
                }
                else
                {
                    // no response array
                }
            }
        }
        else
        {
            // failed
        }

        return results.GetView();
    }




    //_Use_decl_annotations_
    //network::MidiNetworkHostCreationResult MidiNetworkTransportManager::CreateNetworkHost(
    //    network::MidiNetworkHostCreationConfig const& creationConfig)
    //{
    //    auto createResponse = svc::MidiServiceConfig::UpdateTransportPluginConfig(creationConfig);

    //    network::MidiNetworkHostCreationResult result{ };

    //    if (createResponse.Status == svc::MidiServiceConfigResponseStatus::Success)
    //    {
    //        result.Success = true;
    //    }
    //    else
    //    {
    //        result.Success = false;
    //    }

    //    return result;
    //}

    //_Use_decl_annotations_
    //network::MidiNetworkHostRemovalResult MidiNetworkTransportManager::RemoveNetworkHost(
    //    network::MidiNetworkHostRemovalConfig const& removalConfig)
    //{
    //    UNREFERENCED_PARAMETER(removalConfig);

    //    // TEMP
    //    network::MidiNetworkHostRemovalResult result{ };
    //    return result;
    //}

    //_Use_decl_annotations_
    //network::MidiNetworkClientEndpointCreationResult MidiNetworkTransportManager::CreateNetworkClient(
    //    network::MidiNetworkClientEndpointCreationConfig const& creationConfig)
    //{
    //    auto createResponse = svc::MidiServiceConfig::UpdateTransportPluginConfig(creationConfig);

    //    network::MidiNetworkClientEndpointCreationResult result{ };

    //    if (createResponse.Status == svc::MidiServiceConfigResponseStatus::Success)
    //    {
    //        result.Success = true;
    //    }
    //    else
    //    {
    //        result.Success = false;
    //    }

    //    return result;
    //}

    //_Use_decl_annotations_
    //network::MidiNetworkClientEndpointRemovalResult MidiNetworkTransportManager::RemoveNetworkClient(
    //    network::MidiNetworkClientEndpointRemovalConfig const& removalConfig)
    //{
    //    UNREFERENCED_PARAMETER(removalConfig);

    //    // TEMP
    //    network::MidiNetworkClientEndpointRemovalResult result{ };
    //    return result;
    //}


    winrt::hstring MidiNetworkTransportManager::MidiNetworkUdpDnsServiceType() noexcept
    { 
        return L"_midi2._udp"; 
    }

    winrt::hstring MidiNetworkTransportManager::MidiNetworkUdpDnsDomain() noexcept
    { 
        return L"local"; 
    }

    winrt::hstring MidiNetworkTransportManager::MidiNetworkUdpDnsSdQueryString() noexcept
    {
        // protocol guid from https://learn.microsoft.com/en-us/windows/uwp/devices-sensors/enumerate-devices-over-a-network

        return
            L"System.Devices.AepService.ProtocolId:={4526e8c1-8aac-4153-9b16-55e86ada0e54} AND " \
            L"System.Devices.Dnssd.ServiceName:=\"" + MidiNetworkTransportManager::MidiNetworkUdpDnsServiceType() + L"\" AND " \
            L"System.Devices.Dnssd.Domain:=\"" + MidiNetworkTransportManager::MidiNetworkUdpDnsDomain() + L"\"";
    }

    enumeration::DeviceInformationKind MidiNetworkTransportManager::MidiNetworkUdpDnsSdDeviceInformationKind() noexcept
    {
        return enumeration::DeviceInformationKind::AssociationEndpointService;
    }


    collections::IVector<winrt::hstring> MidiNetworkTransportManager::MidiNetworkUdpDnsSdQueryAdditionalProperties() noexcept
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

    collections::IVectorView<network::MidiNetworkAdvertisedHost> MidiNetworkTransportManager::GetAdvertisedHosts() noexcept
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
