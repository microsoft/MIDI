// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services WinRT API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once
#include "MidiBasicLoopbackCreationResult.g.h"

namespace winrt::Windows::Devices::Midi2::Endpoints::BasicLoopback::implementation
{
    struct MidiBasicLoopbackCreationResult : MidiBasicLoopbackCreationResultT<MidiBasicLoopbackCreationResult>
    {
        MidiBasicLoopbackCreationResult() = default;

        bool Success() const noexcept { return m_success; };
        bloop::MidiBasicLoopbackErrorCode ErrorCode() const noexcept { return m_errorCode; }
        winrt::hstring ErrorMessage() const noexcept { return m_errorMessage; }
        winrt::guid AssociationId() const noexcept { return m_associationId; }
        winrt::hstring EndpointDeviceId() const noexcept { return m_endpointDeviceId; }
        bloop::MidiBasicLoopbackEntry CreatedLoopbackEntry() const noexcept { return m_createdLoopbackEntry; }

        void InternalSetSuccess(_In_ winrt::guid const& associationId, _In_ MidiBasicLoopbackEntry const& entry) noexcept
        {
            m_success = true;
            m_associationId = associationId;
            m_createdLoopbackEntry = entry;
        }

        void InternalSetFailure(_In_ winrt::guid const& associationId, _In_ bloop::MidiBasicLoopbackErrorCode errorCode, _In_ winrt::hstring const& errorMessage) noexcept
        {
            m_success = false;
            m_associationId = associationId;
            m_errorCode = errorCode;
            m_errorMessage = errorMessage;
        }

    private:
        bool m_success{ false };
        bloop::MidiBasicLoopbackErrorCode m_errorCode { bloop::MidiBasicLoopbackErrorCode::NoErrorInformationAvailable };
        winrt::hstring m_errorMessage{ };
        winrt::hstring m_endpointDeviceId{ };
        winrt::guid m_associationId{ };
        bloop::MidiBasicLoopbackEntry m_createdLoopbackEntry{ nullptr };

    };
}
