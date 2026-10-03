// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services WinRT API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once
#include "Transports.Rtp.MidiRtpSavedClient.g.h"

namespace winrt::Windows::Devices::Midi2::Transports::Rtp::implementation
{
    struct MidiRtpSavedClient : MidiRtpSavedClientT<MidiRtpSavedClient>
    {
        MidiRtpSavedClient() = default;

        winrt::guid ClientId() const noexcept { return m_clientId; }

        winrt::hstring Comment() const noexcept { return m_comment; }
        winrt::hstring Name() const noexcept { return m_name; }
        winrt::hstring CustomEndpointName() const noexcept { return m_customEndpointName; }

        rtp::MidiRtpClientMatchCriteria MatchCriteria() const noexcept;

        bool AutoReconnect() const noexcept { return m_autoReconnect; }
        bool SendRecoveryJournal() const noexcept { return m_sendRecoveryJournal; }
        bool IsEnabled() const noexcept { return m_isEnabled; }
        rtp::MidiRtpSendSpeedLimit SendSpeedLimit() const noexcept { return m_sendSpeedLimit; }

        void InternalInitialize(
            _In_ winrt::guid const& clientId,
            _In_ json::JsonObject const& entry) noexcept;

    private:
        winrt::guid m_clientId{};

        winrt::hstring m_comment{};
        winrt::hstring m_name{};
        winrt::hstring m_customEndpointName{};

        winrt::hstring m_matchServiceInstanceName{};
        winrt::hstring m_matchDirectHostNameOrIPAddress{};
        uint16_t m_matchDirectPort{ 0 };

        bool m_autoReconnect{ true };
        bool m_sendRecoveryJournal{ true };
        bool m_isEnabled{ true };
        rtp::MidiRtpSendSpeedLimit m_sendSpeedLimit{ rtp::MidiRtpSendSpeedLimit::Unlimited };
    };
}
