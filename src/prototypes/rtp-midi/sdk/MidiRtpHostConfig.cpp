// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License

#include "pch.h"
#include "MidiRtpHostConfig.h"
#include "MidiRtpHostConfig.g.cpp"

namespace winrt::Windows::Devices::Midi2::Transports::Rtp::implementation
{
    MidiRtpHostConfig::MidiRtpHostConfig() noexcept
    {
        // an all-zero identifier is what an unset one looks like, so a failure leaves it zero
        GUID created{};
        if (SUCCEEDED(CoCreateGuid(&created))) m_hostId = created;
    }

    winrt::guid MidiRtpHostConfig::TransportId() const noexcept
    {
        return MIDI_RTP_TRANSPORT_ID_FOR_SDK;
    }

    json::JsonObject MidiRtpHostConfig::ConfigJson() const noexcept
    {
        try
        {
            json::JsonObject host;

            host.SetNamedValue(MIDI_CONFIG_JSON_RTP_MIDI_NAME_KEY, json::JsonValue::CreateStringValue(m_name));

            if (!m_serviceInstanceName.empty())
            {
                host.SetNamedValue(MIDI_CONFIG_JSON_RTP_MIDI_SERVICE_INSTANCE_NAME_KEY, json::JsonValue::CreateStringValue(m_serviceInstanceName));
            }

            host.SetNamedValue(MIDI_CONFIG_JSON_RTP_MIDI_PORT_KEY, json::JsonValue::CreateStringValue(
                m_useAutomaticPort ? winrt::hstring{ MIDI_CONFIG_JSON_RTP_MIDI_PORT_VALUE_AUTO } : winrt::to_hstring(m_port)));

            host.SetNamedValue(MIDI_CONFIG_JSON_RTP_MIDI_ALLOW_PORT_FALLBACK_KEY, json::JsonValue::CreateBooleanValue(m_allowPortFallback));
            host.SetNamedValue(MIDI_CONFIG_JSON_RTP_MIDI_ADVERTISE_KEY, json::JsonValue::CreateBooleanValue(m_advertise));
            host.SetNamedValue(MIDI_CONFIG_JSON_RTP_MIDI_ENABLED_KEY, json::JsonValue::CreateBooleanValue(true));
            host.SetNamedValue(MIDI_CONFIG_JSON_RTP_MIDI_SEND_RECOVERY_JOURNAL_KEY, json::JsonValue::CreateBooleanValue(m_sendRecoveryJournal));

            json::JsonObject hosts;
            hosts.SetNamedValue(winrt::hstring{ internal::GuidToString(m_hostId) }, host);

            json::JsonObject create;
            create.SetNamedValue(MIDI_CONFIG_JSON_RTP_MIDI_HOSTS_KEY, hosts);

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
