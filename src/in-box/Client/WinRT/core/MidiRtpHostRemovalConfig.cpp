// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services WinRT API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

#include "pch.h"
#include "MidiRtpHostRemovalConfig.h"
#include "Transports.Rtp.MidiRtpHostRemovalConfig.g.cpp"

namespace winrt::Windows::Devices::Midi2::Transports::Rtp::implementation
{
    // The configuration file form. RemoveRtpHostAsync sends a command instead, because the command
    // also stops the running host. The host's saved decisions and remote settings go with it.
    json::JsonObject MidiRtpHostRemovalConfig::ConfigJson() const noexcept
    {
        try
        {
            auto const key = MidiRtpSdkJson::EntryKey(m_hostId);

            json::JsonObject hosts;
            hosts.SetNamedValue(key, json::JsonObject{});

            json::JsonObject decisions;
            decisions.SetNamedValue(key, json::JsonObject{});

            json::JsonObject remoteClientSettings;
            remoteClientSettings.SetNamedValue(key, json::JsonObject{});

            json::JsonObject remove;
            remove.SetNamedValue(MIDI_CONFIG_JSON_RTP_MIDI_HOSTS_KEY, hosts);
            remove.SetNamedValue(MIDI_CONFIG_JSON_RTP_MIDI_REMOTE_CLIENT_DECISIONS_KEY, decisions);
            remove.SetNamedValue(MIDI_CONFIG_JSON_RTP_MIDI_REMOTE_CLIENT_SETTINGS_KEY, remoteClientSettings);

            json::JsonObject section;
            section.SetNamedValue(MIDI_CONFIG_JSON_ENDPOINT_COMMON_REMOVE_KEY, remove);

            return MidiRtpSdkJson::WrapTransportSection(section);
        }
        catch (...)
        {
            LOG_IF_FAILED(E_FAIL);

            return nullptr;
        }
    }
}
