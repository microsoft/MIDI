// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services WinRT API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once
#include "Transports.Rtp.MidiRtpRemoteClientApprovalConfig.g.h"

#include "MidiRtpSdkJson.h"

namespace winrt::Windows::Devices::Midi2::Transports::Rtp::implementation
{
    struct MidiRtpRemoteClientApprovalConfig : MidiRtpRemoteClientApprovalConfigT<MidiRtpRemoteClientApprovalConfig>
    {
        MidiRtpRemoteClientApprovalConfig() = default;

        MidiRtpRemoteClientApprovalConfig(
            _In_ winrt::guid const& hostId,
            _In_ winrt::hstring const& remoteClientName,
            _In_ bool const approve,
            _In_ bool const restrictScopeToThisRequestOnly) noexcept :
            m_hostId(hostId),
            m_remoteClientName(remoteClientName),
            m_approve(approve),
            m_scopeIsThisRequestOnly(restrictScopeToThisRequestOnly)
        {
        }

        winrt::guid TransportId() const noexcept { return internal::StringToGuid(MIDI_RTP_TRANSPORT_ID); }

        winrt::guid HostId() const noexcept { return m_hostId; }
        void HostId(_In_ winrt::guid const& value) noexcept { m_hostId = value; }

        // not trimmed: it has to match the name the remote sent exactly
        winrt::hstring RemoteClientName() const noexcept { return m_remoteClientName; }
        void RemoteClientName(_In_ winrt::hstring const& value) noexcept { m_remoteClientName = value; }

        bool Approve() const noexcept { return m_approve; }
        void Approve(_In_ bool const value) noexcept { m_approve = value; }

        bool ScopeIsThisRequestOnly() const noexcept { return m_scopeIsThisRequestOnly; }
        void ScopeIsThisRequestOnly(_In_ bool const value) noexcept { m_scopeIsThisRequestOnly = value; }

        // a decision is a command, so there is nothing to save
        json::JsonObject ConfigJson() const noexcept { return json::JsonObject{}; }

    private:
        winrt::guid m_hostId{};
        winrt::hstring m_remoteClientName{};
        bool m_approve{ false };
        bool m_scopeIsThisRequestOnly{ false };
    };
}

namespace winrt::Windows::Devices::Midi2::Transports::Rtp::factory_implementation
{
    struct MidiRtpRemoteClientApprovalConfig : MidiRtpRemoteClientApprovalConfigT<MidiRtpRemoteClientApprovalConfig, implementation::MidiRtpRemoteClientApprovalConfig>
    {
    };
}
