// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App WinRT API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

#include "pch.h"
#include "MidiNetworkHostRemovalConfig.h"
#include "Transports.Network.MidiNetworkHostRemovalConfig.g.cpp"

// when this component goes in-box, move the json defs to the common json_defs.h
#include "..\..\..\Transport\UdpNetworkMidi2Transport\network_json_defs.h"

namespace winrt::Windows::Devices::Midi2::Transports::Network::implementation
{
    // Mirrors MidiNetworkHostCreationConfig::ConfigJson, but under the "remove" key. The host
    // entry identifier is the object name, so no other properties are needed to identify it.
    // RemoveNetworkHostAsync uses the removeHost command rather than this, because the command
    // also shuts the running host down. This is the configuration file representation.
    json::JsonObject MidiNetworkHostRemovalConfig::ConfigJson() const noexcept
    {
        try
        {
            json::JsonObject hostsContainer{};
            hostsContainer.SetNamedValue(
                winrt::to_hstring(HostId()),
                json::JsonObject{});

            // "hosts": { ... }
            json::JsonObject removeObject{};
            removeObject.SetNamedValue(
                MIDI_CONFIG_JSON_NETWORK_MIDI_HOSTS_KEY,
                hostsContainer);

            // "remove": { ... }
            json::JsonObject transportObject{};
            transportObject.SetNamedValue(
                MIDI_CONFIG_JSON_ENDPOINT_COMMON_REMOVE_KEY,
                removeObject);

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
