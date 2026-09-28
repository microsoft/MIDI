// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services WinRT API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

#include "pch.h"
#include "MidiRtpClientDisconnectConfig.h"
#include "Transports.Rtp.MidiRtpClientDisconnectConfig.g.cpp"

namespace winrt::Windows::Devices::Midi2::Transports::Rtp::implementation
{
    // The configuration file form. DisconnectRtpClientAsync sends a command instead, because the
    // command also ends the running connection.
    json::JsonObject MidiRtpClientDisconnectConfig::ConfigJson() const noexcept
    {
        try
        {
            json::JsonObject clients;
            clients.SetNamedValue(MidiRtpSdkJson::EntryKey(m_clientId), json::JsonObject{});

            json::JsonObject remove;
            remove.SetNamedValue(MIDI_CONFIG_JSON_RTP_MIDI_CLIENTS_KEY, clients);

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
