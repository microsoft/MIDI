// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services WinRT API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

#include "pch.h"
#include "MidiRtpSavedHost.h"
#include "Transports.Rtp.MidiRtpSavedHost.g.cpp"

#include "MidiRtpSdkJson.h"
#include "MidiRtpKnownRemoteClient.h"

namespace winrt::Windows::Devices::Midi2::Transports::Rtp::implementation
{
    // Read the way the transport reads a host entry, including its defaults for what is missing
    _Use_decl_annotations_
    void MidiRtpSavedHost::InternalInitialize(
        winrt::guid const& hostId,
        json::JsonObject const& entry,
        std::vector<json::JsonObject> const& decisions) noexcept
    {
        try
        {
            m_hostId = hostId;

            m_name = MidiRtpSdkJson::String(entry, MIDI_CONFIG_JSON_RTP_MIDI_NAME_KEY);
            m_serviceInstanceName = MidiRtpSdkJson::String(entry, MIDI_CONFIG_JSON_RTP_MIDI_SERVICE_INSTANCE_NAME_KEY);

            m_isEnabled = MidiRtpSdkJson::Boolean(entry, MIDI_CONFIG_JSON_RTP_MIDI_ENABLED_KEY, true);
            m_advertise = MidiRtpSdkJson::Boolean(entry, MIDI_CONFIG_JSON_RTP_MIDI_ADVERTISE_KEY, true);
            m_allowPortFallback = MidiRtpSdkJson::Boolean(entry, MIDI_CONFIG_JSON_RTP_MIDI_ALLOW_PORT_FALLBACK_KEY, true);
            m_sendRecoveryJournal = MidiRtpSdkJson::Boolean(entry, MIDI_CONFIG_JSON_RTP_MIDI_SEND_RECOVERY_JOURNAL_KEY, true);

            uint16_t port{ 0 };
            bool isAuto{ false };
            bool isPresent{ false };

            // A missing port is 5004, and a port the service cannot read shows as zero, because
            // the service does not start that host
            if (MidiRtpSdkJson::TryPort(entry, MIDI_CONFIG_JSON_RTP_MIDI_PORT_KEY, port, isAuto, isPresent))
            {
                m_useAutomaticPortAllocation = isAuto;
                m_manuallyAssignedPort = !isPresent ? static_cast<uint16_t>(MIDI_RTP_SDK_DEFAULT_HOST_PORT) : port;
            }

            // Missing means anyone may connect. Anything present but allowAny needs approval, so
            // a damaged entry never opens a host up.
            bool const hasPolicy = entry != nullptr && entry.HasKey(MIDI_CONFIG_JSON_RTP_MIDI_REMOTE_CLIENT_POLICY_KEY);

            m_remoteClientPolicy =
                !hasPolicy ||
                _wcsicmp(MidiRtpSdkJson::String(entry, MIDI_CONFIG_JSON_RTP_MIDI_REMOTE_CLIENT_POLICY_KEY).c_str(), MIDI_CONFIG_JSON_RTP_MIDI_REMOTE_CLIENT_POLICY_VALUE_ALLOW_ANY) == 0 ?
                rtp::MidiRtpRemoteClientPolicy::AllowAny :
                rtp::MidiRtpRemoteClientPolicy::RequireApproval;

            // a decision without a name never matches, so the service skips it
            for (auto const& decision : decisions)
            {
                for (auto const& [key, isAllowed] : {
                    std::pair{ std::wstring_view{ MIDI_CONFIG_JSON_RTP_MIDI_ALLOWED_CLIENTS_KEY }, true },
                    std::pair{ std::wstring_view{ MIDI_CONFIG_JSON_RTP_MIDI_DENIED_CLIENTS_KEY }, false } })
                {
                    for (auto const& client : MidiRtpSdkJson::Objects(MidiRtpSdkJson::Array(decision, key)))
                    {
                        auto const name = MidiRtpSdkJson::String(client, MIDI_CONFIG_JSON_RTP_MIDI_REMOTE_NAME_KEY);

                        if (name.empty())
                        {
                            continue;
                        }

                        m_knownRemoteClients.Append(winrt::make<MidiRtpKnownRemoteClient>(name, isAllowed));
                    }
                }
            }
        }
        catch (...)
        {
            MIDI_SDK_LOG_GENERAL_EXCEPTION(nullptr, L"General exception reading a saved RTP-MIDI host.");
        }
    }
}
