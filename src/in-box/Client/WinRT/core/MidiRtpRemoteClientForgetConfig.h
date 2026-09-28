// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services WinRT API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once
#include "Transports.Rtp.MidiRtpRemoteClientForgetConfig.g.h"

#include "MidiRtpSdkJson.h"

namespace winrt::Windows::Devices::Midi2::Transports::Rtp::implementation
{
    struct MidiRtpRemoteClientForgetConfig : MidiRtpRemoteClientForgetConfigT<MidiRtpRemoteClientForgetConfig>
    {
        MidiRtpRemoteClientForgetConfig() = default;

        MidiRtpRemoteClientForgetConfig(_In_ winrt::guid const& hostId, _In_ winrt::hstring const& remoteClientName) noexcept :
            m_hostId(hostId),
            m_remoteClientName(remoteClientName)
        {
        }

        winrt::guid TransportId() const noexcept { return internal::StringToGuid(MIDI_RTP_TRANSPORT_ID); }

        winrt::guid HostId() const noexcept { return m_hostId; }
        void HostId(_In_ winrt::guid const& value) noexcept { m_hostId = value; }

        // not trimmed: it has to match the saved name exactly
        winrt::hstring RemoteClientName() const noexcept { return m_remoteClientName; }
        void RemoteClientName(_In_ winrt::hstring const& value) noexcept { m_remoteClientName = value; }

        // Forgetting is a command. To take the name out of the configuration file as well, save a
        // MidiRtpHostKnownClientsConfig without it.
        json::JsonObject ConfigJson() const noexcept { return json::JsonObject{}; }

    private:
        winrt::guid m_hostId{};
        winrt::hstring m_remoteClientName{};
    };
}

namespace winrt::Windows::Devices::Midi2::Transports::Rtp::factory_implementation
{
    struct MidiRtpRemoteClientForgetConfig : MidiRtpRemoteClientForgetConfigT<MidiRtpRemoteClientForgetConfig, implementation::MidiRtpRemoteClientForgetConfig>
    {
    };
}
