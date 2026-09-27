// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License

#include "pch.h"
#include "MidiRtpClientConfig.h"
#include "MidiRtpClientConfig.g.cpp"

namespace winrt::Windows::Devices::Midi2::Transports::Rtp::implementation
{
    MidiRtpClientConfig::MidiRtpClientConfig() noexcept
    {
        GUID created{};
        if (SUCCEEDED(CoCreateGuid(&created))) m_clientId = created;
    }

    winrt::guid MidiRtpClientConfig::TransportId() const noexcept
    {
        return MIDI_RTP_TRANSPORT_ID_FOR_SDK;
    }

    json::JsonObject MidiRtpClientConfig::ConfigJson() const noexcept
    {
        try
        {
            json::JsonObject client;

            client.SetNamedValue(MIDI_CONFIG_JSON_RTP_MIDI_NAME_KEY, json::JsonValue::CreateStringValue(m_name));

            // The service rejects an entry with both or neither, so both are passed through as
            // given rather than one quietly winning here
            if (!m_remoteServiceInstanceName.empty())
            {
                client.SetNamedValue(MIDI_CONFIG_JSON_RTP_MIDI_SERVICE_INSTANCE_NAME_KEY, json::JsonValue::CreateStringValue(m_remoteServiceInstanceName));
            }

            if (!m_remoteAddress.empty())
            {
                client.SetNamedValue(MIDI_CONFIG_JSON_RTP_MIDI_REMOTE_ADDRESS_KEY, json::JsonValue::CreateStringValue(m_remoteAddress));
                client.SetNamedValue(MIDI_CONFIG_JSON_RTP_MIDI_REMOTE_PORT_KEY, json::JsonValue::CreateNumberValue(m_remotePort));
            }

            if (!m_customEndpointName.empty())
            {
                client.SetNamedValue(MIDI_CONFIG_JSON_RTP_MIDI_CUSTOM_ENDPOINT_NAME_KEY, json::JsonValue::CreateStringValue(m_customEndpointName));
            }

            client.SetNamedValue(MIDI_CONFIG_JSON_RTP_MIDI_AUTO_RECONNECT_KEY, json::JsonValue::CreateBooleanValue(m_autoReconnect));
            client.SetNamedValue(MIDI_CONFIG_JSON_RTP_MIDI_ENABLED_KEY, json::JsonValue::CreateBooleanValue(true));
            client.SetNamedValue(MIDI_CONFIG_JSON_RTP_MIDI_SEND_RECOVERY_JOURNAL_KEY, json::JsonValue::CreateBooleanValue(m_sendRecoveryJournal));

            json::JsonObject clients;
            clients.SetNamedValue(winrt::hstring{ internal::GuidToString(m_clientId) }, client);

            json::JsonObject create;
            create.SetNamedValue(MIDI_CONFIG_JSON_RTP_MIDI_CLIENTS_KEY, clients);

            json::JsonObject section;
            section.SetNamedValue(MIDI_CONFIG_JSON_ENDPOINT_COMMON_CREATE_KEY, create);

            return RtpSdkJson::WrapTransportSection(TransportId(), section);
        }
        catch (...)
        {
            return nullptr;
        }
    }
}
