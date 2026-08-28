// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services WinRT API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

#include "pch.h"
#include "MidiNetworkTransportSettings.h"
#include "Transports.Network.MidiNetworkTransportSettings.g.cpp"

#include "..\..\..\Transport\UdpNetworkMidi2Transport\network_json_defs.h"

namespace winrt::Windows::Devices::Midi2::Transports::Network::implementation
{
    namespace
    {
        // The service clamps anything out of range, so this is only about giving the caller a
        // sane number back rather than whatever a malformed response happened to contain.
        uint32_t ReadSetting(
            _In_ json::JsonObject const& settingsJson,
            _In_ winrt::hstring const& name,
            _In_ uint32_t const defaultValue) noexcept
        {
            try
            {
                if (settingsJson == nullptr || !settingsJson.HasKey(name))
                {
                    return defaultValue;
                }

                auto const value = settingsJson.Lookup(name);

                if (value == nullptr || value.ValueType() != json::JsonValueType::Number)
                {
                    return defaultValue;
                }

                auto const number = value.GetNumber();

                if (number != number || number < 0.0)
                {
                    return defaultValue;
                }

                return static_cast<uint32_t>(number);
            }
            catch (...)
            {
                return defaultValue;
            }
        }
    }

    _Use_decl_annotations_
    void MidiNetworkTransportSettings::InternalInitialize(json::JsonObject const& settingsJson) noexcept
    {
        m_maxForwardErrorCorrectionCommandPackets = ReadSetting(
            settingsJson, MIDI_CONFIG_JSON_NETWORK_MIDI_MAX_FEC_PACKETS_KEY, MIDI_NETWORK_FEC_PACKET_COUNT_DEFAULT);

        m_maxRetransmitBufferCommandPackets = ReadSetting(
            settingsJson, MIDI_CONFIG_JSON_NETWORK_MIDI_RETRANSMIT_BUFFER_SIZE_KEY, MIDI_NETWORK_RETRANSMIT_BUFFER_PACKET_COUNT_DEFAULT);

        m_outboundPingIntervalMilliseconds = ReadSetting(
            settingsJson, MIDI_CONFIG_JSON_NETWORK_MIDI_OUTBOUND_PING_INTERVAL_KEY, MIDI_NETWORK_OUTBOUND_PING_INTERVAL_DEFAULT);

        m_invitationPendingTimeoutMilliseconds = ReadSetting(
            settingsJson, MIDI_CONFIG_JSON_NETWORK_MIDI_INVITATION_PENDING_TIMEOUT_KEY, MIDI_NETWORK_INVITATION_PENDING_TIMEOUT_DEFAULT);

        m_maxHostConnections = ReadSetting(
            settingsJson, MIDI_CONFIG_JSON_NETWORK_MIDI_MAX_HOST_CONNECTIONS_KEY, MIDI_NETWORK_HOST_MAX_CONNECTIONS_DEFAULT);

        m_directConnectionScanIntervalMilliseconds = ReadSetting(
            settingsJson, MIDI_CONFIG_JSON_NETWORK_MIDI_DIRECT_CONNECTION_SCAN_INTERVAL_KEY, MIDI_NETWORK_DIRECT_CONNECTION_SCAN_INTERVAL_DEFAULT);
    }

    json::JsonObject MidiNetworkTransportSettings::ConfigJson()
    {
        json::JsonObject configJson;

        try
        {
            json::JsonObject settingsJson;

            settingsJson.SetNamedValue(
                MIDI_CONFIG_JSON_NETWORK_MIDI_MAX_FEC_PACKETS_KEY,
                json::JsonValue::CreateNumberValue(m_maxForwardErrorCorrectionCommandPackets));

            settingsJson.SetNamedValue(
                MIDI_CONFIG_JSON_NETWORK_MIDI_RETRANSMIT_BUFFER_SIZE_KEY,
                json::JsonValue::CreateNumberValue(m_maxRetransmitBufferCommandPackets));

            settingsJson.SetNamedValue(
                MIDI_CONFIG_JSON_NETWORK_MIDI_OUTBOUND_PING_INTERVAL_KEY,
                json::JsonValue::CreateNumberValue(m_outboundPingIntervalMilliseconds));

            settingsJson.SetNamedValue(
                MIDI_CONFIG_JSON_NETWORK_MIDI_INVITATION_PENDING_TIMEOUT_KEY,
                json::JsonValue::CreateNumberValue(m_invitationPendingTimeoutMilliseconds));

            settingsJson.SetNamedValue(
                MIDI_CONFIG_JSON_NETWORK_MIDI_MAX_HOST_CONNECTIONS_KEY,
                json::JsonValue::CreateNumberValue(m_maxHostConnections));

            settingsJson.SetNamedValue(
                MIDI_CONFIG_JSON_NETWORK_MIDI_DIRECT_CONNECTION_SCAN_INTERVAL_KEY,
                json::JsonValue::CreateNumberValue(m_directConnectionScanIntervalMilliseconds));

            configJson.SetNamedValue(MIDI_CONFIG_JSON_NETWORK_MIDI_TRANSPORT_SETTINGS_KEY, settingsJson);
        }
        catch (...)
        {
            MIDI_SDK_LOG_GENERAL_EXCEPTION(nullptr, L"Exception building the network transport settings configuration.");
        }

        return configJson;
    }
}
