// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App SDK and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

#include "pch.h"
#include "MidiNetworkClientConnectConfig.h"
#include "Endpoints.Network.MidiNetworkClientConnectConfig.g.cpp"

#include "..\..\..\api\Transport\UdpNetworkMidi2Transport\network_json_defs.h"

#include "MidiEndpointMatchCriteria.h"

namespace winrt::Microsoft::Windows::Devices::Midi2::Endpoints::Network::implementation
{

    json::JsonObject MidiNetworkClientConnectConfig::GetConfigJson() const noexcept
    {
        json::JsonObject clientObject{};

        clientObject.SetNamedValue(
            MIDI_CONFIG_JSON_NETWORK_MIDI_NETWORK_PROTOCOL_KEY,
            json::JsonValue::CreateStringValue(MIDI_CONFIG_JSON_NETWORK_MIDI_NETWORK_PROTOCOL_VALUE_UDP));                // only UDP allowed for now

        if (!Comment().empty())
        {
            clientObject.SetNamedValue(
                L"_comment",            // todo: standardize this key
                json::JsonValue::CreateStringValue(Comment()));
        }

        json::JsonObject matchObject = json::JsonObject::Parse(MatchCriteria().GetConfigJson());

        if (!UmpEndpointName().empty())
        {
            clientObject.SetNamedValue(
                MIDI_CONFIG_JSON_NETWORK_MIDI_COMMAND_PARAMETER_UMP_ENDPOINT_NAME,
                json::JsonValue::CreateStringValue(UmpEndpointName())
            );
        }

        clientObject.SetNamedValue(
            WindowsMidiServicesPluginConfigurationLib::MidiEndpointMatchCriteria::PropertyKey,
            matchObject);

        json::JsonObject clientsContainer{};
        clientsContainer.SetNamedValue(
            Id(),
            clientObject);

        // package it all up

        // "clients": { ... }
        json::JsonObject createObject{};
        createObject.SetNamedValue(
            MIDI_CONFIG_JSON_NETWORK_MIDI_CLIENTS_KEY,
            clientsContainer);

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
