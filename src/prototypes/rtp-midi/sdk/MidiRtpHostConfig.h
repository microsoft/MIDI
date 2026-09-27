// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License

#pragma once
#include "MidiRtpHostConfig.g.h"

namespace winrt::Windows::Devices::Midi2::Transports::Rtp::implementation
{
    struct MidiRtpHostConfig : MidiRtpHostConfigT<MidiRtpHostConfig>
    {
        MidiRtpHostConfig() noexcept;

        winrt::guid TransportId() const noexcept;
        json::JsonObject ConfigJson() const noexcept;

        winrt::guid HostId() const noexcept { return m_hostId; }
        void HostId(_In_ winrt::guid const& value) noexcept { m_hostId = value; }

        winrt::hstring Name() const noexcept { return m_name; }
        void Name(_In_ winrt::hstring const& value) noexcept { m_name = value; }

        winrt::hstring ServiceInstanceName() const noexcept { return m_serviceInstanceName; }
        void ServiceInstanceName(_In_ winrt::hstring const& value) noexcept { m_serviceInstanceName = value; }

        bool UseAutomaticPort() const noexcept { return m_useAutomaticPort; }
        void UseAutomaticPort(_In_ bool const value) noexcept { m_useAutomaticPort = value; }

        uint16_t Port() const noexcept { return m_port; }
        void Port(_In_ uint16_t const value) noexcept { m_port = value; }

        bool AllowPortFallback() const noexcept { return m_allowPortFallback; }
        void AllowPortFallback(_In_ bool const value) noexcept { m_allowPortFallback = value; }

        bool Advertise() const noexcept { return m_advertise; }
        void Advertise(_In_ bool const value) noexcept { m_advertise = value; }

        bool SendRecoveryJournal() const noexcept { return m_sendRecoveryJournal; }
        void SendRecoveryJournal(_In_ bool const value) noexcept { m_sendRecoveryJournal = value; }

    private:
        winrt::guid m_hostId{};
        winrt::hstring m_name{};
        winrt::hstring m_serviceInstanceName{};
        bool m_useAutomaticPort{ true };
        uint16_t m_port{ MIDI_RTP_DEFAULT_HOST_PORT };
        bool m_allowPortFallback{ true };
        bool m_advertise{ true };
        bool m_sendRecoveryJournal{ true };
    };
}

namespace winrt::Windows::Devices::Midi2::Transports::Rtp::factory_implementation
{
    struct MidiRtpHostConfig : MidiRtpHostConfigT<MidiRtpHostConfig, implementation::MidiRtpHostConfig>
    {
    };
}
