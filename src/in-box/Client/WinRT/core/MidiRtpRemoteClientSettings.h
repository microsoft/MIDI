// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services WinRT API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once
#include "Transports.Rtp.MidiRtpRemoteClientSettings.g.h"

namespace winrt::Windows::Devices::Midi2::Transports::Rtp::implementation
{
    struct MidiRtpRemoteClientSettings : MidiRtpRemoteClientSettingsT<MidiRtpRemoteClientSettings>
    {
        MidiRtpRemoteClientSettings() = default;

        MidiRtpRemoteClientSettings(_In_ winrt::hstring const& remoteClientName) noexcept :
            m_remoteClientName(remoteClientName)
        {
        }

        winrt::hstring RemoteClientName() const noexcept { return m_remoteClientName; }
        void RemoteClientName(_In_ winrt::hstring const& value) noexcept { m_remoteClientName = value; }

        rtp::MidiRtpSendSpeedLimit SendSpeedLimit() const noexcept { return m_sendSpeedLimit; }
        void SendSpeedLimit(_In_ rtp::MidiRtpSendSpeedLimit const& value) noexcept { m_sendSpeedLimit = value; }

    private:
        winrt::hstring m_remoteClientName{};
        rtp::MidiRtpSendSpeedLimit m_sendSpeedLimit{ rtp::MidiRtpSendSpeedLimit::Unlimited };
    };
}

namespace winrt::Windows::Devices::Midi2::Transports::Rtp::factory_implementation
{
    struct MidiRtpRemoteClientSettings : MidiRtpRemoteClientSettingsT<MidiRtpRemoteClientSettings, implementation::MidiRtpRemoteClientSettings>
    {
    };
}
