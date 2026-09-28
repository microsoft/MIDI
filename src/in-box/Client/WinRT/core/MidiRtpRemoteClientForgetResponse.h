// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services WinRT API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once
#include "Transports.Rtp.MidiRtpRemoteClientForgetResponse.g.h"

namespace winrt::Windows::Devices::Midi2::Transports::Rtp::implementation
{
    struct MidiRtpRemoteClientForgetResponse : MidiRtpRemoteClientForgetResponseT<MidiRtpRemoteClientForgetResponse>
    {
        MidiRtpRemoteClientForgetResponse() = default;

        winrt::guid HostId() const noexcept { return m_hostId; }
        winrt::hstring RemoteClientName() const noexcept { return m_remoteClientName; }
        bool Success() const noexcept { return m_success; }
        rtp::MidiRtpRemoteClientForgetErrorCode ErrorCode() const noexcept { return m_errorCode; }
        winrt::hstring ErrorMessage() const noexcept { return m_errorMessage; }

        void InternalSetHostId(_In_ winrt::guid const& value) noexcept { m_hostId = value; }
        void InternalSetRemoteClientName(_In_ winrt::hstring const& value) noexcept { m_remoteClientName = value; }

        void InternalSetError(_In_ rtp::MidiRtpRemoteClientForgetErrorCode const errorCode, _In_ winrt::hstring const& errorMessage) noexcept
        {
            m_success = false;
            m_errorCode = errorCode;
            m_errorMessage = errorMessage;
        }

        void InternalSetSuccess() noexcept
        {
            m_success = true;
            m_errorCode = rtp::MidiRtpRemoteClientForgetErrorCode::NoErrorInformationAvailable;
            m_errorMessage = L"";
        }

    private:
        winrt::guid m_hostId{};
        winrt::hstring m_remoteClientName{};
        bool m_success{ false };
        rtp::MidiRtpRemoteClientForgetErrorCode m_errorCode{ rtp::MidiRtpRemoteClientForgetErrorCode::NoErrorInformationAvailable };
        winrt::hstring m_errorMessage{};
    };
}
