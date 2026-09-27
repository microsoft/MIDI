// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License

#pragma once
#include "MidiRtpOperationResponse.g.h"

namespace winrt::Windows::Devices::Midi2::Transports::Rtp::implementation
{
    struct MidiRtpOperationResponse : MidiRtpOperationResponseT<MidiRtpOperationResponse>
    {
        MidiRtpOperationResponse() = default;

        bool Success() const noexcept { return m_success; }
        rtp::MidiRtpErrorCode ErrorCode() const noexcept { return m_errorCode; }
        winrt::hstring ErrorMessage() const noexcept { return m_errorMessage; }
        winrt::guid EntryId() const noexcept { return m_entryId; }

        void InternalSetEntryId(_In_ winrt::guid const& entryId) noexcept { m_entryId = entryId; }

        void InternalSetSuccess() noexcept
        {
            m_success = true;
            m_errorCode = rtp::MidiRtpErrorCode::None;
            m_errorMessage = {};
        }

        void InternalSetError(_In_ rtp::MidiRtpErrorCode const errorCode, _In_ winrt::hstring const& message) noexcept
        {
            m_success = false;
            m_errorCode = errorCode;
            m_errorMessage = message;
        }

    private:
        bool m_success{ false };
        rtp::MidiRtpErrorCode m_errorCode{ rtp::MidiRtpErrorCode::None };
        winrt::hstring m_errorMessage{};
        winrt::guid m_entryId{};
    };
}
