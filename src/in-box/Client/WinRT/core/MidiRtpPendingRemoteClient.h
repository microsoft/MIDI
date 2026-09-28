// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services WinRT API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once
#include "Transports.Rtp.MidiRtpPendingRemoteClient.g.h"

#include "MidiRtpSdkJson.h"

namespace winrt::Windows::Devices::Midi2::Transports::Rtp::implementation
{
    struct MidiRtpPendingRemoteClient : MidiRtpPendingRemoteClientT<MidiRtpPendingRemoteClient>
    {
        MidiRtpPendingRemoteClient() = default;

        winrt::guid HostId() const noexcept { return m_hostId; }
        winrt::hstring HostServiceInstanceName() const noexcept { return m_hostServiceInstanceName; }
        winrt::hstring RemoteClientName() const noexcept { return m_remoteClientName; }
        winrt::hstring RemoteAddress() const noexcept { return m_remoteAddress; }
        foundation::DateTime RequestTime() const noexcept { return m_requestTime; }
        bool IsApproved() const noexcept { return m_isApproved; }

        // false when the entry has no usable host identifier or name
        bool InternalInitialize(_In_ json::JsonObject const& source) noexcept;

    private:
        winrt::guid m_hostId{};
        winrt::hstring m_hostServiceInstanceName{};
        winrt::hstring m_remoteClientName{};
        winrt::hstring m_remoteAddress{};
        foundation::DateTime m_requestTime{};
        bool m_isApproved{ false };
    };
}
