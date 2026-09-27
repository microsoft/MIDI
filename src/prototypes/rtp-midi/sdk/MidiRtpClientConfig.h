// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License

#pragma once
#include "MidiRtpClientConfig.g.h"

namespace winrt::Windows::Devices::Midi2::Transports::Rtp::implementation
{
    struct MidiRtpClientConfig : MidiRtpClientConfigT<MidiRtpClientConfig>
    {
        MidiRtpClientConfig() noexcept;

        winrt::guid TransportId() const noexcept;
        json::JsonObject ConfigJson() const noexcept;

        winrt::guid ClientId() const noexcept { return m_clientId; }
        void ClientId(_In_ winrt::guid const& value) noexcept { m_clientId = value; }

        winrt::hstring Name() const noexcept { return m_name; }
        void Name(_In_ winrt::hstring const& value) noexcept { m_name = value; }

        winrt::hstring RemoteServiceInstanceName() const noexcept { return m_remoteServiceInstanceName; }
        void RemoteServiceInstanceName(_In_ winrt::hstring const& value) noexcept { m_remoteServiceInstanceName = value; }

        winrt::hstring RemoteAddress() const noexcept { return m_remoteAddress; }
        void RemoteAddress(_In_ winrt::hstring const& value) noexcept { m_remoteAddress = value; }

        uint16_t RemotePort() const noexcept { return m_remotePort; }
        void RemotePort(_In_ uint16_t const value) noexcept { m_remotePort = value; }

        winrt::hstring CustomEndpointName() const noexcept { return m_customEndpointName; }
        void CustomEndpointName(_In_ winrt::hstring const& value) noexcept { m_customEndpointName = value; }

        bool AutoReconnect() const noexcept { return m_autoReconnect; }
        void AutoReconnect(_In_ bool const value) noexcept { m_autoReconnect = value; }

        bool SendRecoveryJournal() const noexcept { return m_sendRecoveryJournal; }
        void SendRecoveryJournal(_In_ bool const value) noexcept { m_sendRecoveryJournal = value; }

    private:
        winrt::guid m_clientId{};
        winrt::hstring m_name{};
        winrt::hstring m_remoteServiceInstanceName{};
        winrt::hstring m_remoteAddress{};
        uint16_t m_remotePort{ MIDI_RTP_DEFAULT_HOST_PORT };
        winrt::hstring m_customEndpointName{};
        bool m_autoReconnect{ true };
        bool m_sendRecoveryJournal{ true };
    };
}

namespace winrt::Windows::Devices::Midi2::Transports::Rtp::factory_implementation
{
    struct MidiRtpClientConfig : MidiRtpClientConfigT<MidiRtpClientConfig, implementation::MidiRtpClientConfig>
    {
    };
}
