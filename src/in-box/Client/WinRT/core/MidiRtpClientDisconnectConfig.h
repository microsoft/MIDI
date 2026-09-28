// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services WinRT API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once
#include "Transports.Rtp.MidiRtpClientDisconnectConfig.g.h"

#include "MidiRtpSdkJson.h"

namespace winrt::Windows::Devices::Midi2::Transports::Rtp::implementation
{
    struct MidiRtpClientDisconnectConfig : MidiRtpClientDisconnectConfigT<MidiRtpClientDisconnectConfig>
    {
        MidiRtpClientDisconnectConfig() = default;
        MidiRtpClientDisconnectConfig(_In_ winrt::guid const& clientId) noexcept : m_clientId(clientId) {}

        winrt::guid TransportId() const noexcept { return internal::StringToGuid(MIDI_RTP_TRANSPORT_ID); }

        winrt::guid ClientId() const noexcept { return m_clientId; }
        void ClientId(_In_ winrt::guid const& value) noexcept { m_clientId = value; }

        json::JsonObject ConfigJson() const noexcept;

    private:
        winrt::guid m_clientId{};
    };
}

namespace winrt::Windows::Devices::Midi2::Transports::Rtp::factory_implementation
{
    struct MidiRtpClientDisconnectConfig : MidiRtpClientDisconnectConfigT<MidiRtpClientDisconnectConfig, implementation::MidiRtpClientDisconnectConfig>
    {
    };
}
