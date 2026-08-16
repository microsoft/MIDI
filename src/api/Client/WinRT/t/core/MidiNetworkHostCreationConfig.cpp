// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App WinRT API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

#include "pch.h"
#include "MidiNetworkHostCreationConfig.h"
#include "Transports.Network.MidiNetworkHostCreationConfig.g.cpp"

// when this component goes in-box, move the json defs to the common json_defs.h
#include "..\..\..\Transport\UdpNetworkMidi2Transport\network_json_defs.h"


namespace winrt::Windows::Devices::Midi2::Transports::Network::implementation
{

    _Use_decl_annotations_
    winrt::hstring MidiNetworkHostCreationConfig::EnsureCompliantServiceInstanceName(winrt::hstring const& serviceInstanceName) noexcept
    {
        // ensures all ASCII characters, and removes any invalid characters for a SWD unique id.

        auto cleanId = internal::RemoveInvalidSWDUniqueIdCharacters(serviceInstanceName.c_str());

        // if it doesn't have the required suffix





        // TEMP
        return L"";

    }





    network::MidiNetworkHostCreationConfig MidiNetworkHostCreationConfig::CreateDefault() noexcept
    {
        try
        {
            auto config = winrt::make_self<MidiNetworkHostCreationConfig>();

            config->m_id = foundation::GuidHelper::CreateNewGuid();

            winrt::hstring name{};

            // get the computer name

            wchar_t computerName[MAX_COMPUTERNAME_LENGTH + 1]; // Buffer for name
            DWORD size = sizeof(computerName);              // Size in bytes

            // Attempt to get the computer name
            if (GetComputerNameW(computerName, &size)) 
            {
                name = winrt::hstring{ computerName };
            }
            else
            {
                // couldn't get the computer name. This is a faulure
                return nullptr;
            }

            // TODO: default the endpoint name to the computer name
            // The constant here is the max number of UTF-8 bytes allowed.
            // here we're treating it as ascii character count. That's not
            // correct and so needs changing.
            std::wstring nameStr = name.c_str();
            config->m_name = nameStr.substr(0, MIDI_MAX_UMP_ENDPOINT_NAME_BYTE_COUNT);

            // build the service instance name. Like the instance id, we include Windows / midisrv because
            // there are already implementations of this protocol which use the machine name directly from apps.
            config->m_serviceInstanceName = internal::RemoveInvalidSWDUniqueIdCharacters(name.c_str()) + 
                L"_windows_midisrv." + 
                MidiNetworkTransportManager::MidiNetworkUdpDnsServiceType() +
                L"." +
                MidiNetworkTransportManager::MidiNetworkUdpDnsDomain();


            // create the product instance id. Per spec, this must be 42 ASCII characters or fewer
            // stripped guid + this prefix is exactly 42 characters.
            winrt::hstring instanceIdPrefix = L"winmidisrv";
            config->m_productInstanceId = instanceIdPrefix + internal::RemoveInvalidSWDUniqueIdCharacters(internal::GuidToString(config->m_id)).substr(0, 42 - instanceIdPrefix.size());

            // use a dynamic port by default
            config->m_useAutomaticPortAllocation = true;
            config->m_manuallyAssignedPort = winrt::hstring{};
           
            // yes, we advertise over mDNS
            config->m_advertise = true;

            // create MIDI 1 API ports for this
            config->m_umpOnly = true;

            // default to no authentication.
            config->m_authenticationType = network::MidiNetworkAuthenticationType::NoAuthentication;
            
            
            return *config;
        }
        catch (winrt::hresult_error const& ex)
        {
            MIDI_SDK_LOG_HRESULT_EXCEPTION(MIDI_SDK_STATIC_THIS_PLACEHOLDER_FIELD_VALUE, ex, L"hresult error creating default network host config.");
            return nullptr;
        }
        catch (...)
        {
            MIDI_SDK_LOG_GENERAL_EXCEPTION(MIDI_SDK_STATIC_THIS_PLACEHOLDER_FIELD_VALUE, L"General exception creating default network host config.");
            return nullptr;
        }


    }




    json::JsonObject MidiNetworkHostCreationConfig::ConfigJson() const noexcept
    {
        json::JsonObject hostObject{};

        hostObject.SetNamedValue(
            MIDI_CONFIG_JSON_ENDPOINT_COMMON_NAME_PROPERTY,
            json::JsonValue::CreateStringValue(Name()));

        hostObject.SetNamedValue(
            MIDI_CONFIG_JSON_NETWORK_MIDI_SERVICE_INSTANCE_NAME_KEY,
            json::JsonValue::CreateStringValue(ServiceInstanceName()));

        hostObject.SetNamedValue(
            MIDI_CONFIG_JSON_NETWORK_MIDI_PRODUCT_INSTANCE_ID_PROPERTY,
            json::JsonValue::CreateStringValue(ProductInstanceId()));

        hostObject.SetNamedValue(
            MIDI_CONFIG_JSON_NETWORK_MIDI_NETWORK_PROTOCOL_KEY,
            json::JsonValue::CreateStringValue(MIDI_CONFIG_JSON_NETWORK_MIDI_NETWORK_PROTOCOL_VALUE_UDP));                // only UDP allowed for now

        if (UseAutomaticPortAllocation())
        {
            hostObject.SetNamedValue(
                MIDI_CONFIG_JSON_NETWORK_MIDI_NETWORK_PORT_KEY,
                json::JsonValue::CreateStringValue(MIDI_CONFIG_JSON_NETWORK_MIDI_NETWORK_PORT_VALUE_AUTO));
        }
        else
        {
            hostObject.SetNamedValue(
                MIDI_CONFIG_JSON_NETWORK_MIDI_NETWORK_PORT_KEY,
                json::JsonValue::CreateStringValue(ManuallyAssignedPort()));
        }

        hostObject.SetNamedValue(
            MIDI_CONFIG_JSON_NETWORK_MIDI_CREATE_MIDI1_PORTS_KEY,
            json::JsonValue::CreateBooleanValue(!CreateOnlyUmpEndpoints()));

        hostObject.SetNamedValue(
            MIDI_CONFIG_JSON_NETWORK_MIDI_MDNS_ADVERTISE_KEY,
            json::JsonValue::CreateBooleanValue(Advertise()));

        hostObject.SetNamedValue(
            MIDI_CONFIG_JSON_NETWORK_MIDI_ENABLED_KEY,
            json::JsonValue::CreateBooleanValue(true)); // enabled is always true when created through the transport manager

        hostObject.SetNamedValue(
            MIDI_CONFIG_JSON_NETWORK_MIDI_HOST_AUTHENTICATION_KEY,
            json::JsonValue::CreateStringValue(MIDI_CONFIG_JSON_NETWORK_MIDI_HOST_AUTHENTICATION_VALUE_NONE));  // "none" is only allowed value at the moment

        hostObject.SetNamedValue(
            MIDI_CONFIG_JSON_NETWORK_MIDI_REMOTE_CLIENT_POLICY_KEY,
            json::JsonValue::CreateStringValue(MIDI_CONFIG_JSON_NETWORK_MIDI_REMOTE_CLIENT_POLICY_VALUE_ALLOW_ANY));


        json::JsonObject hostsContainer{};
        hostsContainer.SetNamedValue(
            winrt::to_hstring(HostId()),
            hostObject);

        // package it all up

        // "hosts": { ... }
        json::JsonObject createObject{};
        createObject.SetNamedValue(
            MIDI_CONFIG_JSON_NETWORK_MIDI_HOSTS_KEY, 
            hostsContainer);
        
        // "create": { ... }
        json::JsonObject transportObject{};
        transportObject.SetNamedValue(
            MIDI_CONFIG_JSON_ENDPOINT_COMMON_CREATE_KEY, 
            createObject);

        // "{C95DCD1F-CDE3-4C2D-913C-528CB8A4CBE6}": { ... }
        json::JsonObject transportSettingsObject{};
        transportSettingsObject.SetNamedValue(
            internal::GuidToString(network::MidiNetworkTransportManager::TransportId()),
            transportObject);


        // "endpointTransportPluginSettings": { ... }
        json::JsonObject wrapperObject{};
        wrapperObject.SetNamedValue(
            MIDI_CONFIG_JSON_TRANSPORT_PLUGIN_SETTINGS_OBJECT,
            transportSettingsObject);


		return wrapperObject;
    }
}
