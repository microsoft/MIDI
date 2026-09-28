// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services WinRT API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once
#include "Transports.Rtp.MidiRtpRemoteClientDisconnectConfig.g.h"

#include "MidiRtpSdkJson.h"

namespace winrt::Windows::Devices::Midi2::Transports::Rtp::implementation
{
    struct MidiRtpRemoteClientDisconnectConfig : MidiRtpRemoteClientDisconnectConfigT<MidiRtpRemoteClientDisconnectConfig>
    {
        MidiRtpRemoteClientDisconnectConfig() = default;

        MidiRtpRemoteClientDisconnectConfig(_In_ winrt::guid const& hostId, _In_ uint32_t const connectionId) noexcept :
            m_hostId(hostId),
            m_connectionId(connectionId)
        {
        }

        winrt::guid TransportId() const noexcept { return internal::StringToGuid(MIDI_RTP_TRANSPORT_ID); }

        winrt::guid HostId() const noexcept { return m_hostId; }
        void HostId(_In_ winrt::guid const& value) noexcept { m_hostId = value; }

        uint32_t ConnectionId() const noexcept { return m_connectionId; }
        void ConnectionId(_In_ uint32_t const value) noexcept { m_connectionId = value; }

        // a disconnect is a command, so there is nothing to save
        json::JsonObject ConfigJson() const noexcept { return json::JsonObject{}; }

    private:
        winrt::guid m_hostId{};
        uint32_t m_connectionId{ 0 };
    };
}

namespace winrt::Windows::Devices::Midi2::Transports::Rtp::factory_implementation
{
    struct MidiRtpRemoteClientDisconnectConfig : MidiRtpRemoteClientDisconnectConfigT<MidiRtpRemoteClientDisconnectConfig, implementation::MidiRtpRemoteClientDisconnectConfig>
    {
    };
}
