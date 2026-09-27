// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License

#include "pch.h"
#include "MidiRtpConfiguredHost.h"
#include "MidiRtpConfiguredHost.g.cpp"
#include "MidiRtpConnection.h"

namespace winrt::Windows::Devices::Midi2::Transports::Rtp::implementation
{
    _Use_decl_annotations_
    bool MidiRtpConfiguredHost::InternalInitialize(json::JsonObject const& source) noexcept
    {
        try
        {
            if (!RtpSdkJson::TryGuid(RtpSdkJson::String(source, MIDI_CONFIG_JSON_RTP_MIDI_ENTRY_IDENTIFIER_KEY), m_hostId)) return false;

            m_name = RtpSdkJson::String(source, MIDI_CONFIG_JSON_RTP_MIDI_NAME_KEY);
            m_serviceInstanceName = RtpSdkJson::String(source, MIDI_CONFIG_JSON_RTP_MIDI_SERVICE_INSTANCE_NAME_KEY);
            m_actualServiceInstanceName = RtpSdkJson::String(source, MIDI_CONFIG_JSON_RTP_MIDI_ACTUAL_SERVICE_INSTANCE_NAME_KEY);
            m_serviceInstanceNameWasChanged = RtpSdkJson::Boolean(source, MIDI_CONFIG_JSON_RTP_MIDI_SERVICE_INSTANCE_NAME_CHANGED_KEY);
            m_isEnabled = RtpSdkJson::Boolean(source, MIDI_CONFIG_JSON_RTP_MIDI_ENABLED_KEY);
            m_hasStarted = RtpSdkJson::Boolean(source, MIDI_CONFIG_JSON_RTP_MIDI_HAS_STARTED_KEY);
            m_advertise = RtpSdkJson::Boolean(source, MIDI_CONFIG_JSON_RTP_MIDI_ADVERTISE_KEY);
            m_configuredPort = RtpSdkJson::String(source, MIDI_CONFIG_JSON_RTP_MIDI_CONFIGURED_PORT_KEY);
            m_allowPortFallback = RtpSdkJson::Boolean(source, MIDI_CONFIG_JSON_RTP_MIDI_ALLOW_PORT_FALLBACK_KEY);
            m_usedPortFallback = RtpSdkJson::Boolean(source, MIDI_CONFIG_JSON_RTP_MIDI_PORT_FALLBACK_USED_KEY);
            m_sendRecoveryJournal = RtpSdkJson::Boolean(source, MIDI_CONFIG_JSON_RTP_MIDI_SEND_RECOVERY_JOURNAL_KEY);
            m_lastErrorCode = RtpSdkJson::Hresult(source, MIDI_CONFIG_JSON_RTP_MIDI_LAST_ERROR_KEY);

            // a string, like Network MIDI 2.0's
            auto const actualPort = RtpSdkJson::Unsigned<uint16_t>(source, MIDI_CONFIG_JSON_RTP_MIDI_ACTUAL_PORT_KEY);
            m_actualPort = actualPort == 0 ? winrt::hstring{} : winrt::to_hstring(actualPort);

            for (auto const& entry : RtpSdkJson::Objects(RtpSdkJson::Array(source, MIDI_CONFIG_JSON_RTP_MIDI_CONNECTIONS_KEY)))
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
