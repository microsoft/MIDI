// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services WinRT API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

#include "pch.h"
#include "MidiRtpConfiguredClient.h"
#include "Transports.Rtp.MidiRtpConfiguredClient.g.cpp"

#include "MidiRtpConnection.h"

namespace winrt::Windows::Devices::Midi2::Transports::Rtp::implementation
{
    _Use_decl_annotations_
    bool MidiRtpConfiguredClient::InternalInitialize(json::JsonObject const& source) noexcept
    {
        try
        {
            if (!MidiRtpSdkJson::TryGuid(MidiRtpSdkJson::String(source, MIDI_CONFIG_JSON_RTP_MIDI_ENTRY_IDENTIFIER_KEY), m_clientId)) return false;

            m_name = MidiRtpSdkJson::String(source, MIDI_CONFIG_JSON_RTP_MIDI_NAME_KEY);
            m_isDirectConnection = MidiRtpSdkJson::Boolean(source, MIDI_CONFIG_JSON_RTP_MIDI_IS_DIRECT_KEY);
            m_remoteServiceInstanceName = MidiRtpSdkJson::String(source, MIDI_CONFIG_JSON_RTP_MIDI_SERVICE_INSTANCE_NAME_KEY);
            m_configuredDirectAddress = MidiRtpSdkJson::String(source, MIDI_CONFIG_JSON_RTP_MIDI_REMOTE_ADDRESS_KEY);
            m_configuredDirectPort = MidiRtpSdkJson::Unsigned<uint16_t>(source, MIDI_CONFIG_JSON_RTP_MIDI_REMOTE_PORT_KEY);
            m_customEndpointName = MidiRtpSdkJson::String(source, MIDI_CONFIG_JSON_RTP_MIDI_CUSTOM_ENDPOINT_NAME_KEY);
            m_autoReconnect = MidiRtpSdkJson::Boolean(source, MIDI_CONFIG_JSON_RTP_MIDI_AUTO_RECONNECT_KEY);
            m_isEnabled = MidiRtpSdkJson::Boolean(source, MIDI_CONFIG_JSON_RTP_MIDI_ENABLED_KEY);
            m_sendRecoveryJournal = MidiRtpSdkJson::Boolean(source, MIDI_CONFIG_JSON_RTP_MIDI_SEND_RECOVERY_JOURNAL_KEY);
            m_sendSpeedLimit = static_cast<rtp::MidiRtpSendSpeedLimit>(MidiRtpSdkJson::SendSpeedLimit(source, MIDI_CONFIG_JSON_RTP_MIDI_SEND_SPEED_LIMIT_KEY));

            // only reported while the client is running
            m_currentSendSpeedLimit = MidiRtpSdkJson::Find(source, MIDI_CONFIG_JSON_RTP_MIDI_CURRENT_SEND_SPEED_LIMIT_KEY, json::JsonValueType::Number) != nullptr ?
                static_cast<rtp::MidiRtpSendSpeedLimit>(MidiRtpSdkJson::SendSpeedLimit(source, MIDI_CONFIG_JSON_RTP_MIDI_CURRENT_SEND_SPEED_LIMIT_KEY)) :
                m_sendSpeedLimit;

            m_lastErrorCode = MidiRtpSdkJson::Hresult(source, MIDI_CONFIG_JSON_RTP_MIDI_LAST_ERROR_KEY);

            auto const state = MidiRtpSdkJson::String(source, MIDI_CONFIG_JSON_RTP_MIDI_ENTRY_STATE_KEY);

            if (state == MIDI_CONFIG_JSON_RTP_MIDI_ENTRY_STATE_VALUE_LIVE) m_entryState = rtp::MidiRtpClientEntryState::Active;
            else if (state == MIDI_CONFIG_JSON_RTP_MIDI_ENTRY_STATE_VALUE_FAILED) m_entryState = rtp::MidiRtpClientEntryState::Retrying;
            else if (state == MIDI_CONFIG_JSON_RTP_MIDI_ENTRY_STATE_VALUE_UNAVAILABLE) m_entryState = rtp::MidiRtpClientEntryState::Unavailable;
            else m_entryState = rtp::MidiRtpClientEntryState::Pending;

            // a client has one connection at most
            auto const connections = MidiRtpSdkJson::Objects(MidiRtpSdkJson::Array(source, MIDI_CONFIG_JSON_RTP_MIDI_CONNECTIONS_KEY));

            if (!connections.empty())
            {
                auto connection = winrt::make_self<MidiRtpConnection>();
                connection->InternalInitialize(connections.front(), m_sendSpeedLimit);
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
