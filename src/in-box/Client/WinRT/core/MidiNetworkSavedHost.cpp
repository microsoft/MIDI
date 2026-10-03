// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App WinRT API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

#include "pch.h"
#include "MidiNetworkSavedHost.h"
#include "Transports.Network.MidiNetworkSavedHost.g.cpp"

#include "..\..\..\Transport\UdpNetworkMidi2Transport\net2udp_transport_defs.h"
#include "..\..\..\Transport\UdpNetworkMidi2Transport\network_json_defs.h"

#include "MidiNetworkKnownRemoteClient.h"
#include "midi_saved_config_json.h"
#include "midi_network_adapters.h"

namespace winrt::Windows::Devices::Midi2::Transports::Network::implementation
{
    // Read the way the transport reads a host entry, including its defaults for what is missing
    _Use_decl_annotations_
    void MidiNetworkSavedHost::InternalInitialize(
        winrt::guid const& hostId,
        json::JsonObject const& entry,
        std::vector<json::JsonObject> const& updates) noexcept
    {
        try
        {
            m_hostId = hostId;

            m_name = internal::TrimmedHStringCopy(MidiSavedConfigJson::String(entry, MIDI_CONFIG_JSON_ENDPOINT_COMMON_NAME_PROPERTY));
            m_serviceInstanceName = internal::TrimmedHStringCopy(MidiSavedConfigJson::String(entry, MIDI_CONFIG_JSON_NETWORK_MIDI_SERVICE_INSTANCE_NAME_KEY));
            m_productInstanceId = internal::TrimmedHStringCopy(MidiSavedConfigJson::String(entry, MIDI_CONFIG_JSON_NETWORK_MIDI_PRODUCT_INSTANCE_ID_PROPERTY));

            m_isEnabled = MidiSavedConfigJson::Boolean(entry, MIDI_CONFIG_JSON_NETWORK_MIDI_ENABLED_KEY, true);
            m_advertise = MidiSavedConfigJson::Boolean(entry, MIDI_CONFIG_JSON_NETWORK_MIDI_MDNS_ADVERTISE_KEY, true);
            m_allowPortFallback = MidiSavedConfigJson::Boolean(entry, MIDI_CONFIG_JSON_NETWORK_MIDI_ALLOW_PORT_FALLBACK_KEY, true);

            auto const port = internal::TrimmedHStringCopy(MidiSavedConfigJson::String(
                entry,
                MIDI_CONFIG_JSON_NETWORK_MIDI_NETWORK_PORT_KEY,
                MIDI_CONFIG_JSON_NETWORK_MIDI_NETWORK_PORT_VALUE_AUTO));

            m_useAutomaticPortAllocation =
                port.empty() ||
                internal::ToLowerTrimmedHStringCopy(port) == internal::ToLowerTrimmedHStringCopy(winrt::hstring{ MIDI_CONFIG_JSON_NETWORK_MIDI_NETWORK_PORT_VALUE_AUTO });

            m_manuallyAssignedPort = m_useAutomaticPortAllocation ? winrt::hstring{} : port;

            // anything but requireApproval is allowAny, which is what a host with no policy does
            m_remoteClientPolicy =
                internal::ToLowerTrimmedHStringCopy(MidiSavedConfigJson::String(entry, MIDI_CONFIG_JSON_NETWORK_MIDI_REMOTE_CLIENT_POLICY_KEY)) ==
                internal::ToLowerTrimmedHStringCopy(winrt::hstring{ MIDI_CONFIG_JSON_NETWORK_MIDI_REMOTE_CLIENT_POLICY_VALUE_REQUIRE_APPROVAL }) ?
                network::MidiNetworkRemoteClientPolicy::RequireApproval :
                network::MidiNetworkRemoteClientPolicy::AllowAny;

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

            // An id which is not a GUID is ignored, as the service ignores it
            auto const readNetworkAdapter = [this](json::JsonObject const& source)
                {
                    GUID id{};

                    if (::WindowsMidiServicesInternal::TryParseMidiNetworkAdapterId(
                            std::wstring{ MidiSavedConfigJson::String(
                                source,
                                MIDI_CONFIG_JSON_NETWORK_MIDI_NETWORK_ADAPTER_ID_KEY,
                                winrt::hstring{ ::WindowsMidiServicesInternal::MidiNetworkAdapterIdToString(m_networkAdapterId) }) },
                            id))
                    {
                        m_networkAdapterId = id;
                    }

                    m_networkAdapterName = internal::TrimmedHStringCopy(
                        MidiSavedConfigJson::String(source, MIDI_CONFIG_JSON_NETWORK_MIDI_NETWORK_ADAPTER_NAME_KEY, m_networkAdapterName));

                    m_allowNetworkAdapterFallback = MidiSavedConfigJson::Boolean(
                        source, MIDI_CONFIG_JSON_NETWORK_MIDI_ALLOW_NETWORK_ADAPTER_FALLBACK_KEY, m_allowNetworkAdapterFallback);
                };

            readNetworkAdapter(entry);

            auto sendSpeedLimit = MidiSavedConfigJson::SendSpeedLimit(entry, MIDI_CONFIG_JSON_NETWORK_MIDI_SEND_SPEED_LIMIT_KEY, 0);
            auto reduceSendSpeedAutomatically = MidiSavedConfigJson::Boolean(entry, MIDI_CONFIG_JSON_NETWORK_MIDI_REDUCE_SEND_SPEED_AUTOMATICALLY_KEY, false);

            // a value missing from a change leaves what came before it
            for (auto const& update : updates)
            {
                createMidi1Ports = MidiSavedConfigJson::Boolean(update, MIDI_CONFIG_JSON_NETWORK_MIDI_CREATE_MIDI1_PORTS_KEY, createMidi1Ports);

                fallbackMidi1PortCount = MidiSavedConfigJson::Byte(
                    update,
                    MIDI_CONFIG_JSON_NETWORK_MIDI_FALLBACK_MIDI1_PORT_COUNT_KEY,
                    fallbackMidi1PortCount,
                    MIDI_NETWORK_MIDI_FALLBACK_MIDI1_PORT_COUNT_MINIMUM,
                    MIDI_NETWORK_MIDI_FALLBACK_MIDI1_PORT_COUNT_MAXIMUM);

                sendSpeedLimit = MidiSavedConfigJson::SendSpeedLimit(update, MIDI_CONFIG_JSON_NETWORK_MIDI_SEND_SPEED_LIMIT_KEY, sendSpeedLimit);
                reduceSendSpeedAutomatically = MidiSavedConfigJson::Boolean(update, MIDI_CONFIG_JSON_NETWORK_MIDI_REDUCE_SEND_SPEED_AUTOMATICALLY_KEY, reduceSendSpeedAutomatically);

                readNetworkAdapter(update);
            }

            m_createOnlyUmpEndpoints = !createMidi1Ports;
            m_fallbackMidi1PortCount = fallbackMidi1PortCount;
            m_sendSpeedLimit = static_cast<network::MidiNetworkSendSpeedLimit>(sendSpeedLimit);
            m_reduceSendSpeedAutomatically = reduceSendSpeedAutomatically;

            // The service ignores a decision missing either half of the identity, so it is left out
            auto const addKnownClients = [this, &entry](std::wstring_view const key, bool const isAllowed)
                {
                    for (auto const& identity : MidiSavedConfigJson::Objects(MidiSavedConfigJson::Array(entry, key)))
                    {
                        auto const name = internal::TrimmedHStringCopy(MidiSavedConfigJson::String(identity, MIDI_CONFIG_JSON_NETWORK_MIDI_CLIENT_IDENTITY_NAME_KEY));
                        auto const productInstanceId = internal::TrimmedHStringCopy(MidiSavedConfigJson::String(identity, MIDI_CONFIG_JSON_NETWORK_MIDI_CLIENT_IDENTITY_PRODUCT_INSTANCE_ID_KEY));

                        if (name.empty() || productInstanceId.empty())
                        {
                            continue;
                        }

                        m_knownRemoteClients.Append(winrt::make<MidiNetworkKnownRemoteClient>(name, productInstanceId, isAllowed));
                    }
                };

            addKnownClients(MIDI_CONFIG_JSON_NETWORK_MIDI_ALLOWED_CLIENTS_KEY, true);
            addKnownClients(MIDI_CONFIG_JSON_NETWORK_MIDI_DENIED_CLIENTS_KEY, false);
        }
        catch (...)
        {
            MIDI_SDK_LOG_GENERAL_EXCEPTION(nullptr, L"General exception reading a saved network host.");
        }
    }
}
