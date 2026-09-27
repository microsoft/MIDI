// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License

#pragma once
#include "MidiRtpConfiguredClient.g.h"

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
        int32_t m_lastErrorCode{ 0 };
        rtp::MidiRtpConnection m_connection{ nullptr };
    };
}
