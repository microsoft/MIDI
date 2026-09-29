// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services WinRT API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

#include "pch.h"
#include "MidiRtpSavedClient.h"
#include "Transports.Rtp.MidiRtpSavedClient.g.cpp"

#include "MidiRtpSdkJson.h"
#include "MidiRtpClientMatchCriteria.h"

namespace winrt::Windows::Devices::Midi2::Transports::Rtp::implementation
{
    rtp::MidiRtpClientMatchCriteria MidiRtpSavedClient::MatchCriteria() const noexcept
    {
        try
        {
            auto criteria = winrt::make_self<MidiRtpClientMatchCriteria>();

            criteria->ServiceInstanceName(m_matchServiceInstanceName);
            criteria->DirectHostNameOrIPAddress(m_matchDirectHostNameOrIPAddress);
            criteria->DirectPort(m_matchDirectPort);

            return *criteria;
        }
        catch (...)
        {
            return nullptr;
        }
    }

    // Read the way the transport reads a client entry, including its defaults for what is missing
    _Use_decl_annotations_
    void MidiRtpSavedClient::InternalInitialize(
        winrt::guid const& clientId,
        json::JsonObject const& entry) noexcept
    {
        try
        {
            m_clientId = clientId;

            m_comment = MidiRtpSdkJson::String(entry, MIDI_CONFIG_JSON_COMMON_COMMENT_KEY);
            m_name = MidiRtpSdkJson::String(entry, MIDI_CONFIG_JSON_RTP_MIDI_NAME_KEY);
            m_customEndpointName = MidiRtpSdkJson::String(entry, MIDI_CONFIG_JSON_RTP_MIDI_CUSTOM_ENDPOINT_NAME_KEY);

            m_autoReconnect = MidiRtpSdkJson::Boolean(entry, MIDI_CONFIG_JSON_RTP_MIDI_AUTO_RECONNECT_KEY, true);
            m_sendRecoveryJournal = MidiRtpSdkJson::Boolean(entry, MIDI_CONFIG_JSON_RTP_MIDI_SEND_RECOVERY_JOURNAL_KEY, true);
            m_isEnabled = MidiRtpSdkJson::Boolean(entry, MIDI_CONFIG_JSON_RTP_MIDI_ENABLED_KEY, true);

            m_matchServiceInstanceName = MidiRtpSdkJson::String(entry, MIDI_CONFIG_JSON_RTP_MIDI_SERVICE_INSTANCE_NAME_KEY);
            m_matchDirectHostNameOrIPAddress = MidiRtpSdkJson::String(entry, MIDI_CONFIG_JSON_RTP_MIDI_REMOTE_ADDRESS_KEY);

            uint16_t port{ 0 };
            bool isAuto{ false };
            bool isPresent{ false };

            // A missing port is 5004, and a port the service cannot read shows as zero, because
            // the service does not connect that client
            if (MidiRtpSdkJson::TryPort(entry, MIDI_CONFIG_JSON_RTP_MIDI_REMOTE_PORT_KEY, port, isAuto, isPresent) && !isAuto)
            {
                m_matchDirectPort = !isPresent ? static_cast<uint16_t>(MIDI_RTP_SDK_DEFAULT_HOST_PORT) : port;
            }
        }
        catch (...)
        {
            MIDI_SDK_LOG_GENERAL_EXCEPTION(nullptr, L"General exception reading a saved RTP-MIDI client.");
        }
    }
}
