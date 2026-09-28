// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services WinRT API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once
#include "Transports.Rtp.MidiRtpKnownRemoteClient.g.h"

namespace winrt::Windows::Devices::Midi2::Transports::Rtp::implementation
{
    struct MidiRtpKnownRemoteClient : MidiRtpKnownRemoteClientT<MidiRtpKnownRemoteClient>
    {
        MidiRtpKnownRemoteClient() = default;

        MidiRtpKnownRemoteClient(_In_ winrt::hstring const& remoteClientName, _In_ bool const isAllowed) :
            m_remoteClientName(remoteClientName),
            m_isAllowed(isAllowed)
        {
        }

        winrt::hstring RemoteClientName() const noexcept { return m_remoteClientName; }
        void RemoteClientName(_In_ winrt::hstring const& value) noexcept { m_remoteClientName = value; }

        bool IsAllowed() const noexcept { return m_isAllowed; }
        void IsAllowed(_In_ bool const value) noexcept { m_isAllowed = value; }

    private:
        winrt::hstring m_remoteClientName{};
        bool m_isAllowed{ false };
    };
}

namespace winrt::Windows::Devices::Midi2::Transports::Rtp::factory_implementation
{
    struct MidiRtpKnownRemoteClient : MidiRtpKnownRemoteClientT<MidiRtpKnownRemoteClient, implementation::MidiRtpKnownRemoteClient>
    {
    };
}
