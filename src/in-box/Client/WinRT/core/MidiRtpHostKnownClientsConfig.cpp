// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services WinRT API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

#include "pch.h"
#include "MidiRtpHostKnownClientsConfig.h"
#include "Transports.Rtp.MidiRtpHostKnownClientsConfig.g.cpp"

namespace winrt::Windows::Devices::Midi2::Transports::Rtp::implementation
{
    // Both lists are always written, including when empty, because the configuration file merge
    // replaces a list rather than adding to it. Leaving one out would keep the saved copy, and an
    // emptied list would never clear.
    json::JsonObject MidiRtpHostKnownClientsConfig::ConfigJson() const noexcept
    {
        try
        {
            json::JsonArray allowedClients;
            json::JsonArray deniedClients;

            for (auto const& client : m_knownClients)
            {
                // a remote is recognized by its name alone, so an entry without one never matches
                if (client == nullptr || client.RemoteClientName().empty()) continue;

                json::JsonObject entry;
                entry.SetNamedValue(MIDI_CONFIG_JSON_RTP_MIDI_REMOTE_NAME_KEY, json::JsonValue::CreateStringValue(client.RemoteClientName()));

                if (client.IsAllowed())
                {
                    allowedClients.Append(entry);
                }
                else
                {
                    deniedClients.Append(entry);
                }
            }

            json::JsonObject hostEntry;
            hostEntry.SetNamedValue(MIDI_CONFIG_JSON_RTP_MIDI_ALLOWED_CLIENTS_KEY, allowedClients);
            hostEntry.SetNamedValue(MIDI_CONFIG_JSON_RTP_MIDI_DENIED_CLIENTS_KEY, deniedClients);

            json::JsonObject decisions;
            decisions.SetNamedValue(MidiRtpSdkJson::EntryKey(m_hostId), hostEntry);

            json::JsonObject create;
            create.SetNamedValue(MIDI_CONFIG_JSON_RTP_MIDI_REMOTE_CLIENT_DECISIONS_KEY, decisions);

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
