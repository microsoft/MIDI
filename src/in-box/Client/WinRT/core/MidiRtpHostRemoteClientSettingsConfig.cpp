// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services WinRT API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

#include "pch.h"
#include "MidiRtpHostRemoteClientSettingsConfig.h"
#include "Transports.Rtp.MidiRtpHostRemoteClientSettingsConfig.g.cpp"

namespace winrt::Windows::Devices::Midi2::Transports::Rtp::implementation
{
    // Beside the host under create, like its remembered decisions, so the service applies it
    // to a running host and saving it never rewrites the host. The list is always written,
    // including when empty, because the configuration file merge replaces a list rather than
    // adding to it: an emptied list would otherwise never clear.
    json::JsonObject MidiRtpHostRemoteClientSettingsConfig::ConfigJson() const noexcept
    {
        try
        {
            json::JsonArray remoteClients;

            for (auto const& settings : m_remoteClientSettings)
            {
                // a remote is recognized by its name alone, so an entry without one never matches
                if (settings == nullptr || settings.RemoteClientName().empty()) continue;

                json::JsonObject entry;
                entry.SetNamedValue(MIDI_CONFIG_JSON_RTP_MIDI_REMOTE_NAME_KEY, json::JsonValue::CreateStringValue(settings.RemoteClientName()));
                entry.SetNamedValue(MIDI_CONFIG_JSON_RTP_MIDI_SEND_SPEED_LIMIT_KEY, json::JsonValue::CreateNumberValue(static_cast<int32_t>(settings.SendSpeedLimit())));

                remoteClients.Append(entry);
            }

            json::JsonObject hostEntry;
            hostEntry.SetNamedValue(MIDI_CONFIG_JSON_RTP_MIDI_REMOTE_CLIENTS_KEY, remoteClients);

            json::JsonObject remoteClientSettings;
            remoteClientSettings.SetNamedValue(MidiRtpSdkJson::EntryKey(m_hostId), hostEntry);

            json::JsonObject create;
            create.SetNamedValue(MIDI_CONFIG_JSON_RTP_MIDI_REMOTE_CLIENT_SETTINGS_KEY, remoteClientSettings);

            json::JsonObject section;
            section.SetNamedValue(MIDI_CONFIG_JSON_ENDPOINT_COMMON_CREATE_KEY, create);

            return MidiRtpSdkJson::WrapTransportSection(section);
        }
        catch (...)
        {
            LOG_IF_FAILED(E_FAIL);

            return nullptr;
        }
    }
}
