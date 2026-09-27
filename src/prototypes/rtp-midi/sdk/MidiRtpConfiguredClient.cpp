// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License

#include "pch.h"
#include "MidiRtpConfiguredClient.h"
#include "MidiRtpConfiguredClient.g.cpp"
#include "MidiRtpConnection.h"

namespace winrt::Windows::Devices::Midi2::Transports::Rtp::implementation
{
    namespace
    {
        rtp::MidiRtpClientEntryState EntryStateFromString(_In_ winrt::hstring const& value) noexcept
        {
            if (value == MIDI_CONFIG_JSON_RTP_MIDI_ENTRY_STATE_VALUE_LIVE) return rtp::MidiRtpClientEntryState::Active;
            if (value == MIDI_CONFIG_JSON_RTP_MIDI_ENTRY_STATE_VALUE_FAILED) return rtp::MidiRtpClientEntryState::Failed;
            if (value == MIDI_CONFIG_JSON_RTP_MIDI_ENTRY_STATE_VALUE_UNAVAILABLE) return rtp::MidiRtpClientEntryState::Unavailable;

            return rtp::MidiRtpClientEntryState::Pending;
        }
    }

    _Use_decl_annotations_
    bool MidiRtpConfiguredClient::InternalInitialize(json::JsonObject const& source) noexcept
    {
        try
        {
            if (!RtpSdkJson::TryGuid(RtpSdkJson::String(source, MIDI_CONFIG_JSON_RTP_MIDI_ENTRY_IDENTIFIER_KEY), m_clientId)) return false;

            m_name = RtpSdkJson::String(source, MIDI_CONFIG_JSON_RTP_MIDI_NAME_KEY);
            m_entryState = EntryStateFromString(RtpSdkJson::String(source, MIDI_CONFIG_JSON_RTP_MIDI_ENTRY_STATE_KEY));
            m_isDirectConnection = RtpSdkJson::Boolean(source, MIDI_CONFIG_JSON_RTP_MIDI_IS_DIRECT_KEY);
            m_remoteServiceInstanceName = RtpSdkJson::String(source, MIDI_CONFIG_JSON_RTP_MIDI_SERVICE_INSTANCE_NAME_KEY);
            m_configuredDirectAddress = RtpSdkJson::String(source, MIDI_CONFIG_JSON_RTP_MIDI_REMOTE_ADDRESS_KEY);
            m_configuredDirectPort = RtpSdkJson::Unsigned<uint16_t>(source, MIDI_CONFIG_JSON_RTP_MIDI_REMOTE_PORT_KEY);
            m_customEndpointName = RtpSdkJson::String(source, MIDI_CONFIG_JSON_RTP_MIDI_CUSTOM_ENDPOINT_NAME_KEY);
            m_autoReconnect = RtpSdkJson::Boolean(source, MIDI_CONFIG_JSON_RTP_MIDI_AUTO_RECONNECT_KEY);
            m_isEnabled = RtpSdkJson::Boolean(source, MIDI_CONFIG_JSON_RTP_MIDI_ENABLED_KEY);
            m_sendRecoveryJournal = RtpSdkJson::Boolean(source, MIDI_CONFIG_JSON_RTP_MIDI_SEND_RECOVERY_JOURNAL_KEY);
            m_lastErrorCode = RtpSdkJson::Hresult(source, MIDI_CONFIG_JSON_RTP_MIDI_LAST_ERROR_KEY);

            // a client has at most one connection
            auto const connections = RtpSdkJson::Objects(RtpSdkJson::Array(source, MIDI_CONFIG_JSON_RTP_MIDI_CONNECTIONS_KEY));

            if (!connections.empty())
            {
                auto connection = winrt::make_self<MidiRtpConnection>();
                connection->InternalInitialize(connections.front());
                m_connection = *connection;
            }

            return true;
        }
        catch (...)
        {
            return false;
        }
    }
}
