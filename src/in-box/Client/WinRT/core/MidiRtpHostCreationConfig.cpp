// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services WinRT API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

#include "pch.h"
#include "MidiRtpHostCreationConfig.h"
#include "Transports.Rtp.MidiRtpHostCreationConfig.g.cpp"

#include "midi_network_adapters.h"

namespace winrt::Windows::Devices::Midi2::Transports::Rtp::implementation
{
    _Use_decl_annotations_
    void MidiRtpHostCreationConfig::NetworkAdapterId(winrt::guid const& value) noexcept
    {
        m_networkAdapterId = value;
        m_networkAdapterName = winrt::hstring{};
        m_networkAdapterPhysicalAddress = winrt::hstring{};

        try
        {
            ::WindowsMidiServicesInternal::MidiNetworkAdapterInfo adapter{};

            if (::WindowsMidiServicesInternal::TryGetMidiNetworkAdapter(value, adapter))
            {
                m_networkAdapterName = winrt::hstring{ adapter.Name };
                m_networkAdapterPhysicalAddress = winrt::hstring{ adapter.PhysicalAddress };
            }
        }
        catch (...)
        {
            MIDI_SDK_LOG_GENERAL_EXCEPTION(this, L"General exception looking up a network adapter.");
        }
    }

    // An empty name is left out, so the service uses this PC's name
    json::JsonObject MidiRtpHostCreationConfig::ConfigJson() const noexcept
    {
        try
        {
            json::JsonObject host;

            if (!m_name.empty())
            {
                host.SetNamedValue(MIDI_CONFIG_JSON_RTP_MIDI_NAME_KEY, json::JsonValue::CreateStringValue(m_name));
            }

            if (!m_serviceInstanceName.empty())
            {
                host.SetNamedValue(MIDI_CONFIG_JSON_RTP_MIDI_SERVICE_INSTANCE_NAME_KEY, json::JsonValue::CreateStringValue(m_serviceInstanceName));
            }

            host.SetNamedValue(MIDI_CONFIG_JSON_RTP_MIDI_PORT_KEY, json::JsonValue::CreateStringValue(
                m_useAutomaticPortAllocation ? winrt::hstring{ MIDI_CONFIG_JSON_RTP_MIDI_PORT_VALUE_AUTO } : winrt::to_hstring(m_manuallyAssignedPort)));

            host.SetNamedValue(MIDI_CONFIG_JSON_RTP_MIDI_ALLOW_PORT_FALLBACK_KEY, json::JsonValue::CreateBooleanValue(m_allowPortFallback));
            host.SetNamedValue(MIDI_CONFIG_JSON_RTP_MIDI_ADVERTISE_KEY, json::JsonValue::CreateBooleanValue(m_advertise));
            host.SetNamedValue(MIDI_CONFIG_JSON_RTP_MIDI_ENABLED_KEY, json::JsonValue::CreateBooleanValue(true));
            host.SetNamedValue(MIDI_CONFIG_JSON_RTP_MIDI_SEND_RECOVERY_JOURNAL_KEY, json::JsonValue::CreateBooleanValue(m_sendRecoveryJournal));
            host.SetNamedValue(MIDI_CONFIG_JSON_RTP_MIDI_SEND_SPEED_LIMIT_KEY, json::JsonValue::CreateNumberValue(static_cast<uint32_t>(m_sendSpeedLimit)));

            host.SetNamedValue(MIDI_CONFIG_JSON_RTP_MIDI_REMOTE_CLIENT_POLICY_KEY, json::JsonValue::CreateStringValue(
                m_remoteClientPolicy == rtp::MidiRtpRemoteClientPolicy::AllowAny ?
                MIDI_CONFIG_JSON_RTP_MIDI_REMOTE_CLIENT_POLICY_VALUE_ALLOW_ANY :
                MIDI_CONFIG_JSON_RTP_MIDI_REMOTE_CLIENT_POLICY_VALUE_REQUIRE_APPROVAL));

            // Written even for every adapter, so replacing a host limited to one undoes the limit
            host.SetNamedValue(MIDI_CONFIG_JSON_RTP_MIDI_NETWORK_ADAPTER_ID_KEY, json::JsonValue::CreateStringValue(
                winrt::hstring{ ::WindowsMidiServicesInternal::MidiNetworkAdapterIdToString(m_networkAdapterId) }));
            host.SetNamedValue(MIDI_CONFIG_JSON_RTP_MIDI_NETWORK_ADAPTER_NAME_KEY, json::JsonValue::CreateStringValue(m_networkAdapterName));
            host.SetNamedValue(MIDI_CONFIG_JSON_RTP_MIDI_NETWORK_ADAPTER_PHYSICAL_ADDRESS_KEY, json::JsonValue::CreateStringValue(m_networkAdapterPhysicalAddress));
            host.SetNamedValue(MIDI_CONFIG_JSON_RTP_MIDI_ALLOW_NETWORK_ADAPTER_FALLBACK_KEY, json::JsonValue::CreateBooleanValue(m_allowNetworkAdapterFallback));

            json::JsonObject hosts;
            hosts.SetNamedValue(MidiRtpSdkJson::EntryKey(m_hostId), host);

            json::JsonObject create;
            create.SetNamedValue(MIDI_CONFIG_JSON_RTP_MIDI_HOSTS_KEY, hosts);

            json::JsonObject section;
            section.SetNamedValue(MIDI_CONFIG_JSON_ENDPOINT_COMMON_CREATE_KEY, create);

            return MidiRtpSdkJson::WrapTransportSection(section);
        }
        catch (...)
        {
            LOG_IF_FAILED(E_FAIL);

            // the service and the configuration file both refuse a missing configuration
            return nullptr;
        }
    }
}
