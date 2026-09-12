// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App WinRT API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

#include "pch.h"
#include "MidiNetworkHostKnownClientsConfig.h"
#include "Transports.Network.MidiNetworkHostKnownClientsConfig.g.cpp"

// when this component goes in-box, move the json defs to the common json_defs.h
#include "..\..\..\Transport\UdpNetworkMidi2Transport\network_json_defs.h"

namespace winrt::Windows::Devices::Midi2::Transports::Network::implementation
{
    // Sits beside the host's other settings under "create", which is where the service reads the
    // lists from at startup. Both arrays are always written, including when empty, because the
    // merge replaces a list outright rather than adding to it: leaving one out would keep the
    // saved version of it, and an emptied list would never actually clear.
    json::JsonObject MidiNetworkHostKnownClientsConfig::ConfigJson() const noexcept
    {
        try
        {
            json::JsonArray allowedClients{};
            json::JsonArray deniedClients{};

            for (auto const& client : m_knownClients)
            {
                if (client == nullptr)
                {
                    continue;
                }

                // A client with neither half of its identity cannot be recognized again, so it
                // would only ever sit in the file unmatched.
                if (client.RemoteClientName().empty() && client.RemoteClientProductInstanceId().empty())
                {
                    continue;
                }

                json::JsonObject identity{};

                identity.SetNamedValue(
                    MIDI_CONFIG_JSON_NETWORK_MIDI_CLIENT_IDENTITY_NAME_KEY,
                    json::JsonValue::CreateStringValue(client.RemoteClientName()));

                identity.SetNamedValue(
                    MIDI_CONFIG_JSON_NETWORK_MIDI_CLIENT_IDENTITY_PRODUCT_INSTANCE_ID_KEY,
                    json::JsonValue::CreateStringValue(client.RemoteClientProductInstanceId()));

                if (client.IsAllowed())
                {
                    allowedClients.Append(identity);
                }
                else
                {
                    deniedClients.Append(identity);
                }
            }

            // "{hostid}": { "allowedClients": [ ... ], "deniedClients": [ ... ] }
            json::JsonObject hostEntry{};
            hostEntry.SetNamedValue(MIDI_CONFIG_JSON_NETWORK_MIDI_ALLOWED_CLIENTS_KEY, allowedClients);
            hostEntry.SetNamedValue(MIDI_CONFIG_JSON_NETWORK_MIDI_DENIED_CLIENTS_KEY, deniedClients);

            json::JsonObject hostsContainer{};
            hostsContainer.SetNamedValue(winrt::to_hstring(HostId()), hostEntry);

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
        catch (...)
        {
            LOG_IF_FAILED(E_FAIL);

            return json::JsonObject{};
        }
    }
}
