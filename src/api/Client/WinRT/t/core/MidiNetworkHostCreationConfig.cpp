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

#include "midi_network_port_picker.h"

// when this component goes in-box, move the json defs to the common json_defs.h
#include "..\..\..\Transport\UdpNetworkMidi2Transport\network_json_defs.h"


namespace winrt::Windows::Devices::Midi2::Transports::Network::implementation
{

    _Use_decl_annotations_
    winrt::hstring MidiNetworkHostCreationConfig::EnsureCompliantServiceInstanceName(winrt::hstring const& serviceInstanceName) noexcept
    {
        // This is the DNS-SD instance name only: the single label to the left of the service
        // type, so "contoso-synth-1" out of "contoso-synth-1._midi2._udp.local". RFC 6763 treats
        // it as user-visible Net-Unicode text, so spaces and non-ASCII are all legal. The only
        // characters removed are the ones which would corrupt the record.
        try
        {
            std::wstring cleaned{ };

            for (auto const ch : internal::TrimmedWStringCopy(serviceInstanceName.c_str()))
            {
                // a period would split this into two labels
                if (ch == L'.')
                {
                    continue;
                }

                // C0, DEL and C1, all excluded by Net-Unicode (RFC 5198)
                if (ch < 0x20 || ch == 0x7F || (ch >= 0x80 && ch <= 0x9F))
                {
                    continue;
                }

                cleaned += ch;
            }

            // trimmed again because truncation can leave a trailing space behind
            return winrt::hstring{ internal::TrimmedWStringCopy(
                internal::TruncateToUtf8ByteCount(cleaned, MIDI_DNSSD_SERVICE_INSTANCE_NAME_MAX_BYTE_COUNT)) };
        }
        catch (...)
        {
            MIDI_SDK_LOG_GENERAL_EXCEPTION(MIDI_SDK_STATIC_THIS_PLACEHOLDER_FIELD_VALUE, L"General exception cleaning service instance name.");
            return L"";
        }
    }





    network::MidiNetworkHostCreationConfig MidiNetworkHostCreationConfig::CreateDefault() noexcept
    {
        try
        {
            auto config = winrt::make_self<MidiNetworkHostCreationConfig>();

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

            // Just the instance label. The transport appends "." + the service type when it
            // builds the PTR record, so the suffix must not be stored here. We include
            // windows/midisrv because other implementations of this protocol already advertise
            // using the bare machine name.
            config->m_serviceInstanceName = EnsureCompliantServiceInstanceName(
                winrt::hstring{ internal::RemoveInvalidSWDUniqueIdCharacters(name.c_str()) + L"_windows_midisrv" });


            // create the product instance id. Per spec, this must be 42 ASCII characters or fewer
            // stripped guid + this prefix is exactly 42 characters.
            winrt::hstring instanceIdPrefix = L"winmidisrv";
            config->m_productInstanceId = instanceIdPrefix + internal::RemoveInvalidSWDUniqueIdCharacters(internal::GuidToString(config->m_id)).substr(0, 42 - instanceIdPrefix.size());

            // A port generated once and kept, rather than a dynamic one which moves every time
            // the service restarts. A remote which stores an address and port then finds the
            // port has changed cannot reconnect on its own.
            uint16_t generatedPort{ 0 };

            if (::WindowsMidiServicesInternal::TryGenerateAvailableHostPort(generatedPort))
            {
                config->m_useAutomaticPortAllocation = false;
                config->m_manuallyAssignedPort = winrt::to_hstring(generatedPort);
            }
            else
            {
                // Nothing free in the generated range is close to impossible, but a host with a
                // dynamic port still works.
                config->m_useAutomaticPortAllocation = true;
                config->m_manuallyAssignedPort = winrt::hstring{};
            }

            // If the kept port is taken next time the service starts, prefer a working host on a
            // different port over no host at all. The host reports that it did this.
            config->m_allowPortFallback = true;
           
            // yes, we advertise over mDNS
            config->m_advertise = true;

            // UMP endpoints only by default. A caller which wants the compatibility MIDI 1.0
            // ports for connected clients sets CreateOnlyUmpEndpoints to false.
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

            hostObject.SetNamedValue(
                MIDI_CONFIG_JSON_NETWORK_MIDI_ALLOW_PORT_FALLBACK_KEY,
                json::JsonValue::CreateBooleanValue(AllowPortFallback()));
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
            json::JsonValue::CreateStringValue(
                RemoteClientPolicy() == network::MidiNetworkRemoteClientPolicy::RequireApproval ?
                MIDI_CONFIG_JSON_NETWORK_MIDI_REMOTE_CLIENT_POLICY_VALUE_REQUIRE_APPROVAL :
                MIDI_CONFIG_JSON_NETWORK_MIDI_REMOTE_CLIENT_POLICY_VALUE_ALLOW_ANY));


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
