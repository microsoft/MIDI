// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App WinRT API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

#include "pch.h"
#include "MidiNetworkSavedClient.h"
#include "Transports.Network.MidiNetworkSavedClient.g.cpp"

#include "..\..\..\Transport\UdpNetworkMidi2Transport\net2udp_transport_defs.h"
#include "..\..\..\Transport\UdpNetworkMidi2Transport\network_json_defs.h"

#include "MidiNetworkClientMatchCriteria.h"
#include "midi_saved_config_json.h"

namespace winrt::Windows::Devices::Midi2::Transports::Network::implementation
{
    network::MidiNetworkClientMatchCriteria MidiNetworkSavedClient::MatchCriteria() const noexcept
    {
        try
        {
            auto criteria = winrt::make_self<MidiNetworkClientMatchCriteria>();

            criteria->DeviceId(m_matchDeviceId);
            criteria->ProductInstanceId(m_matchProductInstanceId);
            criteria->UmpEndpointName(m_matchUmpEndpointName);
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
    void MidiNetworkSavedClient::InternalInitialize(
        winrt::guid const& clientId,
        json::JsonObject const& entry,
        std::vector<json::JsonObject> const& updates) noexcept
    {
        try
        {
            m_clientId = clientId;

            m_comment = MidiSavedConfigJson::String(entry, MIDI_CONFIG_JSON_COMMON_COMMENT_KEY);
            m_isEnabled = MidiSavedConfigJson::Boolean(entry, MIDI_CONFIG_JSON_NETWORK_MIDI_ENABLED_KEY, true);
            m_umpEndpointName = internal::TrimmedHStringCopy(MidiSavedConfigJson::String(entry, MIDI_CONFIG_JSON_NETWORK_MIDI_COMMAND_PARAMETER_UMP_ENDPOINT_NAME));

            auto customEndpointName = MidiSavedConfigJson::String(entry, MIDI_CONFIG_JSON_NETWORK_MIDI_CUSTOM_ENDPOINT_NAME_KEY);

            auto createMidi1Ports = MidiSavedConfigJson::Boolean(
                entry,
                MIDI_CONFIG_JSON_NETWORK_MIDI_CREATE_MIDI1_PORTS_KEY,
                MIDI_NETWORK_MIDI_CREATE_MIDI1_PORTS_DEFAULT);

            auto fallbackMidi1PortCount = MidiSavedConfigJson::Byte(
                entry,
                MIDI_CONFIG_JSON_NETWORK_MIDI_FALLBACK_MIDI1_PORT_COUNT_KEY,
                MIDI_NETWORK_MIDI_FALLBACK_MIDI1_PORT_COUNT_DEFAULT,
                MIDI_NETWORK_MIDI_FALLBACK_MIDI1_PORT_COUNT_MINIMUM,
                MIDI_NETWORK_MIDI_FALLBACK_MIDI1_PORT_COUNT_MAXIMUM);

            // a value missing from a change leaves what came before it
            for (auto const& update : updates)
            {
                customEndpointName = MidiSavedConfigJson::String(update, MIDI_CONFIG_JSON_NETWORK_MIDI_CUSTOM_ENDPOINT_NAME_KEY, customEndpointName);
                createMidi1Ports = MidiSavedConfigJson::Boolean(update, MIDI_CONFIG_JSON_NETWORK_MIDI_CREATE_MIDI1_PORTS_KEY, createMidi1Ports);

                fallbackMidi1PortCount = MidiSavedConfigJson::Byte(
                    update,
                    MIDI_CONFIG_JSON_NETWORK_MIDI_FALLBACK_MIDI1_PORT_COUNT_KEY,
                    fallbackMidi1PortCount,
                    MIDI_NETWORK_MIDI_FALLBACK_MIDI1_PORT_COUNT_MINIMUM,
                    MIDI_NETWORK_MIDI_FALLBACK_MIDI1_PORT_COUNT_MAXIMUM);
            }

            m_customEndpointName = internal::TrimmedHStringCopy(customEndpointName);
            m_createOnlyUmpEndpoints = !createMidi1Ports;
            m_fallbackMidi1PortCount = fallbackMidi1PortCount;

            auto const match = MidiSavedConfigJson::Object(entry, MIDI_CONFIG_JSON_NETWORK_MIDI_CLIENT_MATCH_OBJECT_KEY);

            m_matchDeviceId = internal::TrimmedHStringCopy(MidiSavedConfigJson::String(match, MIDI_CONFIG_JSON_NETWORK_MIDI_CLIENT_MATCH_ID_KEY));
            m_matchProductInstanceId = internal::TrimmedHStringCopy(MidiSavedConfigJson::String(match, MIDI_CONFIG_JSON_NETWORK_MIDI_CLIENT_MATCH_UMP_ENDPOINT_PID_KEY));
            m_matchUmpEndpointName = internal::TrimmedHStringCopy(MidiSavedConfigJson::String(match, MIDI_CONFIG_JSON_NETWORK_MIDI_CLIENT_MATCH_UMP_ENDPOINT_NAME_KEY));
            m_matchDirectHostNameOrIPAddress = internal::TrimmedHStringCopy(MidiSavedConfigJson::String(match, MIDI_CONFIG_JSON_NETWORK_MIDI_CLIENT_MATCH_HOST_NAME_OR_IP_ADDRESS_KEY));

            uint16_t port{ 0 };

            if (MidiSavedConfigJson::TryParsePort(
                std::wstring{ internal::TrimmedHStringCopy(MidiSavedConfigJson::String(match, MIDI_CONFIG_JSON_NETWORK_MIDI_CLIENT_MATCH_PORT_KEY)) },
                port))
            {
                m_matchDirectPort = port;
            }
        }
        catch (...)
        {
            MIDI_SDK_LOG_GENERAL_EXCEPTION(nullptr, L"General exception reading a saved network client.");
        }
    }
}
