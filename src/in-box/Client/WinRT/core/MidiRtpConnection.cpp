// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services WinRT API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

#include "pch.h"
#include "MidiRtpConnection.h"
#include "Transports.Rtp.MidiRtpConnection.g.cpp"

namespace winrt::Windows::Devices::Midi2::Transports::Rtp::implementation
{
    _Use_decl_annotations_
    void MidiRtpConnection::InternalInitialize(json::JsonObject const& source) noexcept
    {
        m_connectionId = MidiRtpSdkJson::Unsigned<uint32_t>(source, MIDI_CONFIG_JSON_RTP_MIDI_CONNECTION_ID_KEY);
        m_remoteName = MidiRtpSdkJson::String(source, MIDI_CONFIG_JSON_RTP_MIDI_REMOTE_NAME_KEY);
        m_remoteAddress = MidiRtpSdkJson::String(source, MIDI_CONFIG_JSON_RTP_MIDI_REMOTE_ADDRESS_KEY);
        m_remotePort = MidiRtpSdkJson::Unsigned<uint16_t>(source, MIDI_CONFIG_JSON_RTP_MIDI_REMOTE_PORT_KEY);
        m_localPort = MidiRtpSdkJson::Unsigned<uint16_t>(source, MIDI_CONFIG_JSON_RTP_MIDI_LOCAL_PORT_KEY);
        m_remoteHostName = MidiRtpSdkJson::String(source, MIDI_CONFIG_JSON_RTP_MIDI_REMOTE_HOST_NAME_KEY);
        m_isConnected = MidiRtpSdkJson::Boolean(source, MIDI_CONFIG_JSON_RTP_MIDI_IS_CONNECTED_KEY);
        m_thisPcInvited = MidiRtpSdkJson::Boolean(source, MIDI_CONFIG_JSON_RTP_MIDI_WE_INITIATED_KEY);
        m_endpointDeviceId = MidiRtpSdkJson::String(source, MIDI_CONFIG_JSON_RTP_MIDI_ENDPOINT_DEVICE_ID_KEY);
        m_currentLatencyTicks = MidiRtpSdkJson::Unsigned<uint64_t>(source, MIDI_CONFIG_JSON_RTP_MIDI_CURRENT_LATENCY_KEY);
        m_bestLatencyTicks = MidiRtpSdkJson::Unsigned<uint64_t>(source, MIDI_CONFIG_JSON_RTP_MIDI_BEST_LATENCY_KEY);
        m_packetsSent = MidiRtpSdkJson::Unsigned<uint64_t>(source, MIDI_CONFIG_JSON_RTP_MIDI_TOTAL_PACKETS_SENT_KEY);
        m_packetsReceived = MidiRtpSdkJson::Unsigned<uint64_t>(source, MIDI_CONFIG_JSON_RTP_MIDI_TOTAL_PACKETS_RECEIVED_KEY);
        m_packetsLost = MidiRtpSdkJson::Unsigned<uint64_t>(source, MIDI_CONFIG_JSON_RTP_MIDI_TOTAL_PACKETS_LOST_KEY);
        m_lossesRepaired = MidiRtpSdkJson::Unsigned<uint64_t>(source, MIDI_CONFIG_JSON_RTP_MIDI_TOTAL_LOSSES_REPAIRED_KEY);
        m_noteOffsRecovered = MidiRtpSdkJson::Unsigned<uint64_t>(source, MIDI_CONFIG_JSON_RTP_MIDI_TOTAL_NOTES_ENDED_KEY);
        m_messagesSent = MidiRtpSdkJson::Unsigned<uint64_t>(source, MIDI_CONFIG_JSON_RTP_MIDI_TOTAL_MESSAGES_SENT_KEY);
        m_messagesReceived = MidiRtpSdkJson::Unsigned<uint64_t>(source, MIDI_CONFIG_JSON_RTP_MIDI_TOTAL_MESSAGES_RECEIVED_KEY);
    }
}
