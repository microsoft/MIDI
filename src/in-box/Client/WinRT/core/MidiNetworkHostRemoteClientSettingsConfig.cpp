// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App WinRT API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

#include "pch.h"
#include "MidiNetworkHostRemoteClientSettingsConfig.h"
#include "Transports.Network.MidiNetworkHostRemoteClientSettingsConfig.g.cpp"

// when this component goes in-box, move the json defs to the common json_defs.h
#include "..\..\..\Transport\UdpNetworkMidi2Transport\network_json_defs.h"

namespace winrt::Windows::Devices::Midi2::Transports::Network::implementation
{
    // A change to the host, under "updateEntries" like MidiNetworkHostUpdateConfig, so the
    // service applies it to a running host. The array is always written, including when empty,
    // because the configuration file merge replaces a list rather than adding to it: an emptied
    // list would otherwise never clear.
    json::JsonObject MidiNetworkHostRemoteClientSettingsConfig::ConfigJson() const noexcept
    {
        try
        {
            json::JsonArray settingsArray{};

            for (auto const& settings : m_remoteClientSettings)
            {
                if (settings == nullptr)
                {
                    continue;
                }

                auto const name = internal::TrimmedHStringCopy(settings.RemoteClientName());
                auto const productInstanceId = internal::TrimmedHStringCopy(settings.RemoteClientProductInstanceId());

                // The service needs both halves of the identity to recognize a remote client, so
                // an entry missing either would never match anything
                if (name.empty() || productInstanceId.empty())
                {
                    continue;
                }

                json::JsonObject settingsObject{};

                settingsObject.SetNamedValue(
                    MIDI_CONFIG_JSON_NETWORK_MIDI_CLIENT_IDENTITY_NAME_KEY,
                    json::JsonValue::CreateStringValue(name));

                settingsObject.SetNamedValue(
                    MIDI_CONFIG_JSON_NETWORK_MIDI_CLIENT_IDENTITY_PRODUCT_INSTANCE_ID_KEY,
                    json::JsonValue::CreateStringValue(productInstanceId));

                settingsObject.SetNamedValue(
                    MIDI_CONFIG_JSON_NETWORK_MIDI_SEND_SPEED_LIMIT_KEY,
                    json::JsonValue::CreateNumberValue(static_cast<uint32_t>(settings.SendSpeedLimit())));

                settingsObject.SetNamedValue(
                    MIDI_CONFIG_JSON_NETWORK_MIDI_REDUCE_SEND_SPEED_AUTOMATICALLY_KEY,
                    json::JsonValue::CreateBooleanValue(settings.ReduceSendSpeedAutomatically()));

                settingsArray.Append(settingsObject);
            }

            json::JsonObject hostObject{};
            hostObject.SetNamedValue(MIDI_CONFIG_JSON_NETWORK_MIDI_REMOTE_CLIENT_SETTINGS_KEY, settingsArray);

            json::JsonObject hostsContainer{};
            hostsContainer.SetNamedValue(winrt::to_hstring(HostId()), hostObject);

            // "hosts": { ... }
            json::JsonObject updateObject{};
            updateObject.SetNamedValue(MIDI_CONFIG_JSON_NETWORK_MIDI_HOSTS_KEY, hostsContainer);

            // "updateEntries": { ... }
            json::JsonObject transportObject{};
            transportObject.SetNamedValue(MIDI_CONFIG_JSON_NETWORK_MIDI_UPDATE_ENTRIES_KEY, updateObject);

            // "{C95DCD1F-CDE3-4C2D-913C-528CB8A4CBE6}": { ... }
            json::JsonObject transportSettingsObject{};
            transportSettingsObject.SetNamedValue(
                internal::GuidToString(network::MidiNetworkTransportManager::TransportId()),
                transportObject);

            // "endpointTransportPluginSettings": { ... }
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
