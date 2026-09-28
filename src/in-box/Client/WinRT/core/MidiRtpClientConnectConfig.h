// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services WinRT API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once
#include "Transports.Rtp.MidiRtpClientConnectConfig.g.h"

#include "MidiRtpSdkJson.h"

namespace winrt::Windows::Devices::Midi2::Transports::Rtp::implementation
{
    struct MidiRtpClientConnectConfig : MidiRtpClientConnectConfigT<MidiRtpClientConnectConfig>
    {
        MidiRtpClientConnectConfig() = default;

        winrt::guid TransportId() const noexcept { return internal::StringToGuid(MIDI_RTP_TRANSPORT_ID); }

        winrt::guid ClientId() const noexcept { return m_clientId; }
        void ClientId(_In_ winrt::guid const& value) noexcept { m_clientId = value; }

        winrt::hstring Comment() const noexcept { return m_comment; }
        void Comment(_In_ winrt::hstring const& value) noexcept { m_comment = value; }

        winrt::hstring Name() const noexcept { return m_name; }
        void Name(_In_ winrt::hstring const& value) noexcept { m_name = internal::TrimmedHStringCopy(value); }

        winrt::hstring CustomEndpointName() const noexcept { return m_customEndpointName; }
        void CustomEndpointName(_In_ winrt::hstring const& value) noexcept { m_customEndpointName = internal::TrimmedHStringCopy(value); }

        rtp::MidiRtpClientMatchCriteria MatchCriteria() const noexcept { return m_matchCriteria; }
        void MatchCriteria(_In_ rtp::MidiRtpClientMatchCriteria const& value) noexcept { m_matchCriteria = value; }

        bool AutoReconnect() const noexcept { return m_autoReconnect; }
        void AutoReconnect(_In_ bool const value) noexcept { m_autoReconnect = value; }

        bool SendRecoveryJournal() const noexcept { return m_sendRecoveryJournal; }
        void SendRecoveryJournal(_In_ bool const value) noexcept { m_sendRecoveryJournal = value; }

        json::JsonObject ConfigJson() const noexcept;

    private:
        winrt::guid m_clientId{ foundation::GuidHelper::CreateNewGuid() };
        winrt::hstring m_comment{};
        winrt::hstring m_name{};
        winrt::hstring m_customEndpointName{};
        rtp::MidiRtpClientMatchCriteria m_matchCriteria{ nullptr };
        bool m_autoReconnect{ true };
        bool m_sendRecoveryJournal{ true };
    };
}

namespace winrt::Windows::Devices::Midi2::Transports::Rtp::factory_implementation
{
    struct MidiRtpClientConnectConfig : MidiRtpClientConnectConfigT<MidiRtpClientConnectConfig, implementation::MidiRtpClientConnectConfig>
    {
    };
}
