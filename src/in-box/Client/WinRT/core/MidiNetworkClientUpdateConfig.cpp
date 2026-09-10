// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App WinRT API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

#include "pch.h"
#include "MidiNetworkClientUpdateConfig.h"
#include "Transports.Network.MidiNetworkClientUpdateConfig.g.cpp"

// when this component goes in-box, move the json defs to the common json_defs.h
#include "..\..\..\Transport\UdpNetworkMidi2Transport\network_json_defs.h"

namespace winrt::Windows::Devices::Midi2::Transports::Network::implementation
{
    // The same client entry shape as MidiNetworkClientConnectConfig, under the "update" key. Only
    // the named properties are touched: the merge into the configuration file cannot delete a key,
    // so the match criteria and the rest of the entry are left alone.
    json::JsonObject MidiNetworkClientUpdateConfig::ConfigJson() const noexcept
    {
        try
        {
            json::JsonObject clientObject{};

            clientObject.SetNamedValue(
                MIDI_CONFIG_JSON_NETWORK_MIDI_CREATE_MIDI1_PORTS_KEY,
                json::JsonValue::CreateBooleanValue(CreateMidi1Ports()));

            clientObject.SetNamedValue(
                MIDI_CONFIG_JSON_NETWORK_MIDI_FALLBACK_MIDI1_PORT_COUNT_KEY,
                json::JsonValue::CreateNumberValue(FallbackMidi1PortCount()));

            json::JsonObject clientsContainer{};
            clientsContainer.SetNamedValue(
                winrt::to_hstring(ClientId()),
                clientObject);

            // "clients": { ... }
            json::JsonObject updateObject{};
            updateObject.SetNamedValue(
                MIDI_CONFIG_JSON_NETWORK_MIDI_CLIENTS_KEY,
                clientsContainer);

            // "update": { ... }
            json::JsonObject transportObject{};
            transportObject.SetNamedValue(
                MIDI_CONFIG_JSON_NETWORK_MIDI_UPDATE_ENTRIES_KEY,
                updateObject);

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
        catch (...)
        {
            LOG_IF_FAILED(E_FAIL);

            return json::JsonObject{};
        }
    }
}
