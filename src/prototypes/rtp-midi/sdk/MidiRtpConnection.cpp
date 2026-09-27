// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License

#include "pch.h"
#include "MidiRtpConnection.h"
#include "MidiRtpConnection.g.cpp"

namespace winrt::Windows::Devices::Midi2::Transports::Rtp::implementation
{
    _Use_decl_annotations_
    void MidiRtpConnection::InternalInitialize(json::JsonObject const& source) noexcept
    {
        m_connectionId = RtpSdkJson::Unsigned<uint32_t>(source, MIDI_CONFIG_JSON_RTP_MIDI_CONNECTION_ID_KEY);
        m_remoteName = RtpSdkJson::String(source, MIDI_CONFIG_JSON_RTP_MIDI_REMOTE_NAME_KEY);
        m_remoteAddress = RtpSdkJson::String(source, MIDI_CONFIG_JSON_RTP_MIDI_REMOTE_ADDRESS_KEY);
        m_remotePort = RtpSdkJson::Unsigned<uint16_t>(source, MIDI_CONFIG_JSON_RTP_MIDI_REMOTE_PORT_KEY);
        m_localPort = RtpSdkJson::Unsigned<uint16_t>(source, MIDI_CONFIG_JSON_RTP_MIDI_LOCAL_PORT_KEY);
        m_isConnected = RtpSdkJson::Boolean(source, MIDI_CONFIG_JSON_RTP_MIDI_IS_CONNECTED_KEY);
        m_thisPcInvited = RtpSdkJson::Boolean(source, MIDI_CONFIG_JSON_RTP_MIDI_WE_INITIATED_KEY);
        m_endpointDeviceId = RtpSdkJson::String(source, MIDI_CONFIG_JSON_RTP_MIDI_ENDPOINT_DEVICE_ID_KEY);
        m_currentLatencyTicks = RtpSdkJson::Unsigned<uint64_t>(source, MIDI_CONFIG_JSON_RTP_MIDI_CURRENT_LATENCY_KEY);
        m_bestLatencyTicks = RtpSdkJson::Unsigned<uint64_t>(source, MIDI_CONFIG_JSON_RTP_MIDI_BEST_LATENCY_KEY);
        m_packetsSent = RtpSdkJson::Unsigned<uint64_t>(source, MIDI_CONFIG_JSON_RTP_MIDI_TOTAL_PACKETS_SENT_KEY);
        m_packetsReceived = RtpSdkJson::Unsigned<uint64_t>(source, MIDI_CONFIG_JSON_RTP_MIDI_TOTAL_PACKETS_RECEIVED_KEY);
        m_packetsLost = RtpSdkJson::Unsigned<uint64_t>(source, MIDI_CONFIG_JSON_RTP_MIDI_TOTAL_PACKETS_LOST_KEY);
        m_lossesRepaired = RtpSdkJson::Unsigned<uint64_t>(source, MIDI_CONFIG_JSON_RTP_MIDI_TOTAL_LOSSES_REPAIRED_KEY);
        m_noteOffsRecovered = RtpSdkJson::Unsigned<uint64_t>(source, MIDI_CONFIG_JSON_RTP_MIDI_TOTAL_NOTES_ENDED_KEY);
        m_messagesSent = RtpSdkJson::Unsigned<uint64_t>(source, MIDI_CONFIG_JSON_RTP_MIDI_TOTAL_MESSAGES_SENT_KEY);
        m_messagesReceived = RtpSdkJson::Unsigned<uint64_t>(source, MIDI_CONFIG_JSON_RTP_MIDI_TOTAL_MESSAGES_RECEIVED_KEY);
    }
}
