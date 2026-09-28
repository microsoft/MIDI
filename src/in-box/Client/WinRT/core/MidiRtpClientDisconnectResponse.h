// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services WinRT API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once
#include "Transports.Rtp.MidiRtpClientDisconnectResponse.g.h"

namespace winrt::Windows::Devices::Midi2::Transports::Rtp::implementation
{
    struct MidiRtpClientDisconnectResponse : MidiRtpClientDisconnectResponseT<MidiRtpClientDisconnectResponse>
    {
        MidiRtpClientDisconnectResponse() = default;

        winrt::guid ClientId() const noexcept { return m_clientId; }
        bool Success() const noexcept { return m_success; }
        rtp::MidiRtpClientDisconnectErrorCode ErrorCode() const noexcept { return m_errorCode; }
        winrt::hstring ErrorMessage() const noexcept { return m_errorMessage; }

        void InternalSetClientId(_In_ winrt::guid const& value) noexcept { m_clientId = value; }

        void InternalSetError(_In_ rtp::MidiRtpClientDisconnectErrorCode const errorCode, _In_ winrt::hstring const& errorMessage) noexcept
        {
            m_success = false;
            m_errorCode = errorCode;
            m_errorMessage = errorMessage;
        }

        void InternalSetSuccess() noexcept
        {
            m_success = true;
            m_errorCode = rtp::MidiRtpClientDisconnectErrorCode::NoErrorInformationAvailable;
            m_errorMessage = L"";
        }

    private:
        winrt::guid m_clientId{};
        bool m_success{ false };
        rtp::MidiRtpClientDisconnectErrorCode m_errorCode{ rtp::MidiRtpClientDisconnectErrorCode::NoErrorInformationAvailable };
        winrt::hstring m_errorMessage{};
    };
}
