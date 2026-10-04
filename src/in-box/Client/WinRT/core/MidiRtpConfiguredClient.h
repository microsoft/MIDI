// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services WinRT API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once
#include "Transports.Rtp.MidiRtpConfiguredClient.g.h"

#include "MidiRtpSdkJson.h"

namespace winrt::Windows::Devices::Midi2::Transports::Rtp::implementation
{
    struct MidiRtpConfiguredClient : MidiRtpConfiguredClientT<MidiRtpConfiguredClient>
    {
        MidiRtpConfiguredClient() = default;

        winrt::guid ClientId() const noexcept { return m_clientId; }
        winrt::hstring Name() const noexcept { return m_name; }
        rtp::MidiRtpClientEntryState EntryState() const noexcept { return m_entryState; }
        bool IsDirectConnection() const noexcept { return m_isDirectConnection; }
        winrt::hstring RemoteServiceInstanceName() const noexcept { return m_remoteServiceInstanceName; }
        winrt::hstring ConfiguredDirectAddress() const noexcept { return m_configuredDirectAddress; }
        uint16_t ConfiguredDirectPort() const noexcept { return m_configuredDirectPort; }
        winrt::hstring CustomEndpointName() const noexcept { return m_customEndpointName; }
        bool AutoReconnect() const noexcept { return m_autoReconnect; }
        bool IsEnabled() const noexcept { return m_isEnabled; }
        bool SendRecoveryJournal() const noexcept { return m_sendRecoveryJournal; }
        rtp::MidiRtpSendSpeedLimit SendSpeedLimit() const noexcept { return m_sendSpeedLimit; }
        rtp::MidiRtpSendSpeedLimit CurrentSendSpeedLimit() const noexcept { return m_currentSendSpeedLimit; }
        int32_t LastErrorCode() const noexcept { return m_lastErrorCode; }
        rtp::MidiRtpConnection Connection() const noexcept { return m_connection; }

        // false when the entry has no usable identifier
        bool InternalInitialize(_In_ json::JsonObject const& source) noexcept;

    private:
        winrt::guid m_clientId{};
        winrt::hstring m_name{};
        rtp::MidiRtpClientEntryState m_entryState{ rtp::MidiRtpClientEntryState::Pending };
        bool m_isDirectConnection{ false };
        winrt::hstring m_remoteServiceInstanceName{};
        winrt::hstring m_configuredDirectAddress{};
        uint16_t m_configuredDirectPort{ 0 };
        winrt::hstring m_customEndpointName{};
        bool m_autoReconnect{ false };
        bool m_isEnabled{ false };
        bool m_sendRecoveryJournal{ false };
        rtp::MidiRtpSendSpeedLimit m_sendSpeedLimit{ rtp::MidiRtpSendSpeedLimit::Unlimited };
        rtp::MidiRtpSendSpeedLimit m_currentSendSpeedLimit{ rtp::MidiRtpSendSpeedLimit::Unlimited };
        int32_t m_lastErrorCode{ 0 };
        rtp::MidiRtpConnection m_connection{ nullptr };
    };
}
