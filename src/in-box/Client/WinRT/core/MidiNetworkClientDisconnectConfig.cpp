// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App WinRT API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

#include "pch.h"
#include "MidiNetworkClientDisconnectConfig.h"
#include "Transports.Network.MidiNetworkClientDisconnectConfig.g.cpp"

#include "MidiNetworkTransportManager.h"

#include "..\..\..\Transport\UdpNetworkMidi2Transport\network_json_defs.h"

namespace winrt::Windows::Devices::Midi2::Transports::Network::implementation
{
    // Disconnecting is a command, and DisconnectNetworkClientAsync sends it. This is the
    // configuration file side: saved, it removes the client's saved entry, so the service does
    // not connect it again when it starts. The running service ignores a removal sent to it.
    json::JsonObject  MidiNetworkClientDisconnectConfig::ConfigJson() const noexcept
    {
        try
        {
            json::JsonObject clientsContainer{};
            clientsContainer.SetNamedValue(winrt::to_hstring(ClientId()), json::JsonObject{});

            // "clients": { "{id}": { } }
            json::JsonObject removeObject{};
            removeObject.SetNamedValue(MIDI_CONFIG_JSON_NETWORK_MIDI_CLIENTS_KEY, clientsContainer);

            // "remove": { ... }
            json::JsonObject transportObject{};
            transportObject.SetNamedValue(MIDI_CONFIG_JSON_ENDPOINT_COMMON_REMOVE_KEY, removeObject);

            json::JsonObject transportSettingsObject{};
            transportSettingsObject.SetNamedValue(
                internal::GuidToString(implementation::MidiNetworkTransportManager::TransportId()),
                transportObject);

            json::JsonObject wrapperObject{};
            wrapperObject.SetNamedValue(MIDI_CONFIG_JSON_TRANSPORT_PLUGIN_SETTINGS_OBJECT, transportSettingsObject);

            return wrapperObject;
        }
        catch (...)
        {
            LOG_IF_FAILED(E_FAIL);

            return json::JsonObject{};
        }
    }
}
