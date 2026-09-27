// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License

#include "pch.h"
#include "MidiRtpEntryRemovalConfig.h"
#include "MidiRtpEntryRemovalConfig.g.cpp"

namespace winrt::Windows::Devices::Midi2::Transports::Rtp::implementation
{
    // Mirrors the create section. An empty object is what the configuration file merge reads as
    // "delete this entry".
    json::JsonObject MidiRtpEntryRemovalConfig::ConfigJson() const noexcept
    {
        try
        {
            json::JsonObject entries;
            entries.SetNamedValue(winrt::hstring{ internal::GuidToString(m_entryId) }, json::JsonObject{});

            json::JsonObject remove;
            remove.SetNamedValue(m_isHost ? MIDI_CONFIG_JSON_RTP_MIDI_HOSTS_KEY : MIDI_CONFIG_JSON_RTP_MIDI_CLIENTS_KEY, entries);

            json::JsonObject section;
            section.SetNamedValue(MIDI_CONFIG_JSON_ENDPOINT_COMMON_REMOVE_KEY, remove);

            return RtpSdkJson::WrapTransportSection(TransportId(), section);
        }
        catch (...)
        {
            return nullptr;
        }
    }
}
