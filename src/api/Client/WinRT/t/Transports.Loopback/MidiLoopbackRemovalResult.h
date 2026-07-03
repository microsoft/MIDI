// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services WinRT API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once
#include "MidiLoopbackRemovalResult.g.h"

namespace winrt::Windows::Devices::Midi2::Transports::Loopback::implementation
{
    struct MidiLoopbackRemovalResult : MidiLoopbackRemovalResultT<MidiLoopbackRemovalResult>
    {
        MidiLoopbackRemovalResult() = default;

        bool Success() const noexcept { return m_success; };
        loop::MidiLoopbackErrorCode ErrorCode() const noexcept { return m_errorCode; }
        winrt::hstring ErrorMessage() const noexcept { return m_errorMessage; }


        void InternalSetSuccess()
        {
            m_success = true;
        }

        void InternalSetFailure(_In_ loop::MidiLoopbackErrorCode const errorCode, _In_ winrt::hstring const& errorMessage)
        {
            m_success = false;
            m_errorCode = errorCode;
            m_errorMessage = errorMessage;
        }

    private:
        bool m_success{ false };
        loop::MidiLoopbackErrorCode m_errorCode{ loop::MidiLoopbackErrorCode::NoErrorInformationAvailable };
        winrt::hstring m_errorMessage{ };
    };
}
