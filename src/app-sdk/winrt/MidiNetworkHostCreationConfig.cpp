// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App SDK and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

#include "pch.h"
#include "MidiNetworkHostCreationConfig.h"
#include "Endpoints.Network.MidiNetworkHostCreationConfig.g.cpp"

// when this component goes in-box, move the json defs to the common json_defs.h
#include "..\..\..\api\Transport\UdpNetworkMidi2Transport\network_json_defs.h"


namespace winrt::Microsoft::Windows::Devices::Midi2::Endpoints::Network::implementation
{

    winrt::hstring MidiNetworkHostCreationConfig::GetConfigJson() const noexcept
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
            MIDI_CONFIG_JSON_NETWORK_MIDI_CONNECTION_POLICY_KEY,
            json::JsonValue::CreateStringValue(MIDI_CONFIG_JSON_NETWORK_MIDI_CONNECTION_POLICY_ALLOW_IPV4_VALUE_ANY));  // "allowAny" is only allowed value at the moment


        json::JsonObject hostsContainer{};
        hostsContainer.SetNamedValue(
            Id(),
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



		return wrapperObject.Stringify();
    }
}
