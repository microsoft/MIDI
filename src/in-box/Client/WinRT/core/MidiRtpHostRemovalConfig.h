// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services WinRT API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once
#include "Transports.Rtp.MidiRtpHostRemovalConfig.g.h"

#include "MidiRtpSdkJson.h"

namespace winrt::Windows::Devices::Midi2::Transports::Rtp::implementation
{
    struct MidiRtpHostRemovalConfig : MidiRtpHostRemovalConfigT<MidiRtpHostRemovalConfig>
    {
        MidiRtpHostRemovalConfig() = default;
        MidiRtpHostRemovalConfig(_In_ winrt::guid const& hostId) noexcept : m_hostId(hostId) {}

        winrt::guid TransportId() const noexcept { return internal::StringToGuid(MIDI_RTP_TRANSPORT_ID); }

        winrt::guid HostId() const noexcept { return m_hostId; }
        void HostId(_In_ winrt::guid const& value) noexcept { m_hostId = value; }

        json::JsonObject ConfigJson() const noexcept;

    private:
        winrt::guid m_hostId{};
    };
}

namespace winrt::Windows::Devices::Midi2::Transports::Rtp::factory_implementation
{
    struct MidiRtpHostRemovalConfig : MidiRtpHostRemovalConfigT<MidiRtpHostRemovalConfig, implementation::MidiRtpHostRemovalConfig>
    {
    };
}
