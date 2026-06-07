// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services WinRT API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once
#include "MidiLoopbackEndpointCreationResult.g.h"

namespace winrt::Windows::Devices::Midi2::Endpoints::Loopback::implementation
{
    struct MidiLoopbackEndpointCreationResult : MidiLoopbackEndpointCreationResultT<MidiLoopbackEndpointCreationResult>
    {
        MidiLoopbackEndpointCreationResult() = default;

        bool Success() const noexcept { return m_success; };
        void Success(_In_ bool const value) noexcept { m_success = value; }

        loop::MidiLoopbackEndpointCreationResultErrorCode ErrorCode() const noexcept { return m_errorCode; }
        void ErrorCode(_In_ loop::MidiLoopbackEndpointCreationResultErrorCode const& value) noexcept { m_errorCode = value; }

        winrt::hstring ErrorMessage() const noexcept { return m_errorMessage; }
        void ErrorMessage(_In_ winrt::hstring const& value) noexcept { m_errorMessage = value; }

        loop::MidiLoopbackEntry CreatedLoopbackEntry() const noexcept { return m_createdLoopbackEntry; }
        void CreatedLoopbackEntry(_In_ loop::MidiLoopbackEntry const& value) noexcept { m_createdLoopbackEntry = value; };


    private:
        bool m_success{ false };
        loop::MidiLoopbackEndpointCreationResultErrorCode m_errorCode{ loop::MidiLoopbackEndpointCreationResultErrorCode::NoErrorInformationAvailable };
        winrt::hstring m_errorMessage{ };

        loop::MidiLoopbackEntry m_createdLoopbackEntry{ nullptr };
    };
}
