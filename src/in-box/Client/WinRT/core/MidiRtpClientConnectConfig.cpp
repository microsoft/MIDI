// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services WinRT API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

#include "pch.h"
#include "MidiRtpClientConnectConfig.h"
#include "Transports.Rtp.MidiRtpClientConnectConfig.g.cpp"

namespace winrt::Windows::Devices::Midi2::Transports::Rtp::implementation
{
    json::JsonObject MidiRtpClientConnectConfig::ConfigJson() const noexcept
    {
        try
        {
            json::JsonObject client;

            if (!m_comment.empty())
            {
                client.SetNamedValue(MIDI_CONFIG_JSON_COMMON_COMMENT_KEY, json::JsonValue::CreateStringValue(m_comment));
            }

            // an empty name is left out, so the service uses this PC's name
            if (!m_name.empty())
            {
                client.SetNamedValue(MIDI_CONFIG_JSON_RTP_MIDI_NAME_KEY, json::JsonValue::CreateStringValue(m_name));
            }

            // The service refuses an entry with both or neither, so both are passed on as given
            // rather than one quietly winning here
            if (m_matchCriteria != nullptr)
            {
                if (!m_matchCriteria.ServiceInstanceName().empty())
                {
                    client.SetNamedValue(MIDI_CONFIG_JSON_RTP_MIDI_SERVICE_INSTANCE_NAME_KEY, json::JsonValue::CreateStringValue(m_matchCriteria.ServiceInstanceName()));
                }

                if (!m_matchCriteria.DirectHostNameOrIPAddress().empty())
                {
                    client.SetNamedValue(MIDI_CONFIG_JSON_RTP_MIDI_REMOTE_ADDRESS_KEY, json::JsonValue::CreateStringValue(m_matchCriteria.DirectHostNameOrIPAddress()));
                    client.SetNamedValue(MIDI_CONFIG_JSON_RTP_MIDI_REMOTE_PORT_KEY, json::JsonValue::CreateNumberValue(m_matchCriteria.DirectPort()));
                }
            }

            if (!m_customEndpointName.empty())
            {
                client.SetNamedValue(MIDI_CONFIG_JSON_RTP_MIDI_CUSTOM_ENDPOINT_NAME_KEY, json::JsonValue::CreateStringValue(m_customEndpointName));
            }

            client.SetNamedValue(MIDI_CONFIG_JSON_RTP_MIDI_AUTO_RECONNECT_KEY, json::JsonValue::CreateBooleanValue(m_autoReconnect));
            client.SetNamedValue(MIDI_CONFIG_JSON_RTP_MIDI_ENABLED_KEY, json::JsonValue::CreateBooleanValue(true));
            client.SetNamedValue(MIDI_CONFIG_JSON_RTP_MIDI_SEND_RECOVERY_JOURNAL_KEY, json::JsonValue::CreateBooleanValue(m_sendRecoveryJournal));

            json::JsonObject clients;
            clients.SetNamedValue(MidiRtpSdkJson::EntryKey(m_clientId), client);

            json::JsonObject create;
            create.SetNamedValue(MIDI_CONFIG_JSON_RTP_MIDI_CLIENTS_KEY, clients);

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
