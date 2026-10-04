// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services WinRT API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

#include "pch.h"
#include "MidiRtpConfiguredHost.h"
#include "Transports.Rtp.MidiRtpConfiguredHost.g.cpp"

#include "MidiRtpConnection.h"
#include "MidiRtpKnownRemoteClient.h"

#include "midi_network_adapters.h"

namespace winrt::Windows::Devices::Midi2::Transports::Rtp::implementation
{
    _Use_decl_annotations_
    bool MidiRtpConfiguredHost::InternalInitialize(json::JsonObject const& source) noexcept
    {
        try
        {
            if (!MidiRtpSdkJson::TryGuid(MidiRtpSdkJson::String(source, MIDI_CONFIG_JSON_RTP_MIDI_ENTRY_IDENTIFIER_KEY), m_hostId)) return false;

            m_name = MidiRtpSdkJson::String(source, MIDI_CONFIG_JSON_RTP_MIDI_NAME_KEY);
            m_serviceInstanceName = MidiRtpSdkJson::String(source, MIDI_CONFIG_JSON_RTP_MIDI_SERVICE_INSTANCE_NAME_KEY);
            m_actualServiceInstanceName = MidiRtpSdkJson::String(source, MIDI_CONFIG_JSON_RTP_MIDI_ACTUAL_SERVICE_INSTANCE_NAME_KEY);
            m_serviceInstanceNameWasChanged = MidiRtpSdkJson::Boolean(source, MIDI_CONFIG_JSON_RTP_MIDI_SERVICE_INSTANCE_NAME_CHANGED_KEY);
            m_isEnabled = MidiRtpSdkJson::Boolean(source, MIDI_CONFIG_JSON_RTP_MIDI_ENABLED_KEY);
            m_hasStarted = MidiRtpSdkJson::Boolean(source, MIDI_CONFIG_JSON_RTP_MIDI_HAS_STARTED_KEY);
            m_advertise = MidiRtpSdkJson::Boolean(source, MIDI_CONFIG_JSON_RTP_MIDI_ADVERTISE_KEY);
            m_configuredPort = MidiRtpSdkJson::String(source, MIDI_CONFIG_JSON_RTP_MIDI_CONFIGURED_PORT_KEY);
            m_actualPort = MidiRtpSdkJson::Unsigned<uint16_t>(source, MIDI_CONFIG_JSON_RTP_MIDI_ACTUAL_PORT_KEY);
            m_allowPortFallback = MidiRtpSdkJson::Boolean(source, MIDI_CONFIG_JSON_RTP_MIDI_ALLOW_PORT_FALLBACK_KEY);
            m_usedPortFallback = MidiRtpSdkJson::Boolean(source, MIDI_CONFIG_JSON_RTP_MIDI_PORT_FALLBACK_USED_KEY);
            m_sendRecoveryJournal = MidiRtpSdkJson::Boolean(source, MIDI_CONFIG_JSON_RTP_MIDI_SEND_RECOVERY_JOURNAL_KEY);
            m_sendSpeedLimit = static_cast<rtp::MidiRtpSendSpeedLimit>(MidiRtpSdkJson::SendSpeedLimit(source, MIDI_CONFIG_JSON_RTP_MIDI_SEND_SPEED_LIMIT_KEY));

            // only reported while the host is running
            m_currentSendSpeedLimit = MidiRtpSdkJson::Find(source, MIDI_CONFIG_JSON_RTP_MIDI_CURRENT_SEND_SPEED_LIMIT_KEY, json::JsonValueType::Number) != nullptr ?
                static_cast<rtp::MidiRtpSendSpeedLimit>(MidiRtpSdkJson::SendSpeedLimit(source, MIDI_CONFIG_JSON_RTP_MIDI_CURRENT_SEND_SPEED_LIMIT_KEY)) :
                m_sendSpeedLimit;

            m_lastErrorCode = MidiRtpSdkJson::Hresult(source, MIDI_CONFIG_JSON_RTP_MIDI_LAST_ERROR_KEY);

            // An older service reports none of these, which reads as every adapter
            GUID networkAdapterId{};

            if (::WindowsMidiServicesInternal::TryParseMidiNetworkAdapterId(
                    std::wstring{ MidiRtpSdkJson::String(source, MIDI_CONFIG_JSON_RTP_MIDI_NETWORK_ADAPTER_ID_KEY) },
                    networkAdapterId))
            {
                m_networkAdapterId = networkAdapterId;
            }

            m_networkAdapterName = MidiRtpSdkJson::String(source, MIDI_CONFIG_JSON_RTP_MIDI_NETWORK_ADAPTER_NAME_KEY);
            m_allowNetworkAdapterFallback = MidiRtpSdkJson::Boolean(source, MIDI_CONFIG_JSON_RTP_MIDI_ALLOW_NETWORK_ADAPTER_FALLBACK_KEY, true);
            m_isNetworkAdapterMissing = MidiRtpSdkJson::Boolean(source, MIDI_CONFIG_JSON_RTP_MIDI_NETWORK_ADAPTER_MISSING_KEY);
            m_usedNetworkAdapterFallback = MidiRtpSdkJson::Boolean(source, MIDI_CONFIG_JSON_RTP_MIDI_NETWORK_ADAPTER_FALLBACK_USED_KEY);

            // missing means anyone may connect, as it does in the configuration file
            auto const policy = MidiRtpSdkJson::String(source, MIDI_CONFIG_JSON_RTP_MIDI_REMOTE_CLIENT_POLICY_KEY);

            m_remoteClientPolicy =
                policy.empty() || _wcsicmp(policy.c_str(), MIDI_CONFIG_JSON_RTP_MIDI_REMOTE_CLIENT_POLICY_VALUE_ALLOW_ANY) == 0 ?
                rtp::MidiRtpRemoteClientPolicy::AllowAny :
                rtp::MidiRtpRemoteClientPolicy::RequireApproval;

            // Only what survives a restart. A decision kept until restart is left out, so that
            // saving this list never turns it into a permanent one.
            auto const addDecisions = [&](wchar_t const* const key, bool const isAllowed)
                {
                    for (auto const& entry : MidiRtpSdkJson::Objects(MidiRtpSdkJson::Array(source, key)))
                    {
                        if (MidiRtpSdkJson::Boolean(entry, MIDI_CONFIG_JSON_RTP_MIDI_UNTIL_RESTART_KEY)) continue;

                        auto const name = MidiRtpSdkJson::String(entry, MIDI_CONFIG_JSON_RTP_MIDI_REMOTE_NAME_KEY);
                        if (!name.empty()) m_knownRemoteClients.Append(winrt::make<MidiRtpKnownRemoteClient>(name, isAllowed));
                    }
                };

            addDecisions(MIDI_CONFIG_JSON_RTP_MIDI_ALLOWED_CLIENTS_KEY, true);
            addDecisions(MIDI_CONFIG_JSON_RTP_MIDI_DENIED_CLIENTS_KEY, false);

            for (auto const& entry : MidiRtpSdkJson::Objects(MidiRtpSdkJson::Array(source, MIDI_CONFIG_JSON_RTP_MIDI_CONNECTIONS_KEY)))
            {
                auto connection = winrt::make_self<MidiRtpConnection>();
                connection->InternalInitialize(entry);
                m_connections.Append(*connection);
            }

            return true;
        }
        catch (...)
        {
            return false;
        }
    }
}
