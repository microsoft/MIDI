// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services WinRT API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================


#pragma once
#include "Transports.BasicLoopback.MidiBasicLoopbackUpdateResponse.g.h"

namespace winrt::Windows::Devices::Midi2::Transports::BasicLoopback::implementation
{
    struct MidiBasicLoopbackUpdateResponse : MidiBasicLoopbackUpdateResponseT<MidiBasicLoopbackUpdateResponse>
    {
        MidiBasicLoopbackUpdateResponse() = default;

        bool Success() const noexcept { return m_success; };
        bloop::MidiBasicLoopbackErrorCode ErrorCode() const noexcept { return m_errorCode; }
        winrt::hstring ErrorMessage() const noexcept { return m_errorMessage; }


        void InternalSetSuccess()
        {
            m_success = true;
        }

        void InternalSetFailure(_In_ bloop::MidiBasicLoopbackErrorCode const errorCode, _In_ winrt::hstring const& errorMessage)
        {
            m_success = false;
            m_errorCode = errorCode;
            m_errorMessage = errorMessage;
        }

    private:
        bool m_success{ false };
        bloop::MidiBasicLoopbackErrorCode m_errorCode{ bloop::MidiBasicLoopbackErrorCode::NoErrorInformationAvailable };
        winrt::hstring m_errorMessage{ };
    };
}
