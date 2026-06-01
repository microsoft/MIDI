// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services WinRT API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once
#include "MidiBasicLoopbackEndpointCreationResult.g.h"

namespace winrt::Windows::Devices::Midi2::Endpoints::BasicLoopback::implementation
{
    struct MidiBasicLoopbackEndpointCreationResult : MidiBasicLoopbackEndpointCreationResultT<MidiBasicLoopbackEndpointCreationResult>
    {
        MidiBasicLoopbackEndpointCreationResult() = default;

        bool Success() const noexcept { return m_success; };
        bloop::MidiBasicLoopbackEndpointCreationResultErrorCode ErrorCode() const noexcept { return m_errorCode; }
        winrt::hstring ErrorMessage() const noexcept { return m_errorMessage; }
        winrt::guid AssociationId() const noexcept { return m_associationId; }
        winrt::hstring EndpointDeviceId() const noexcept { return m_endpointDeviceId; }
        bloop::MidiBasicLoopbackEntry CreatedLoopbackEntry() const noexcept { return m_createdLoopbackEntry; }

        void InternalSetSuccess(_In_ winrt::guid associationId, _In_ bloop::MidiBasicLoopbackEntry& entry) noexcept;
        void InternalSetError(_In_ winrt::guid associationId, _In_ bloop::MidiBasicLoopbackEndpointCreationResultErrorCode errorCode, _In_ winrt::hstring const& errorMessage) noexcept;

    private:
        bool m_success{ false };
        bloop::MidiBasicLoopbackEndpointCreationResultErrorCode m_errorCode { bloop::MidiBasicLoopbackEndpointCreationResultErrorCode::UnknownOrUnspecified };
        winrt::hstring m_errorMessage{ };
        winrt::hstring m_endpointDeviceId{ };
        winrt::guid m_associationId{ };
        bloop::MidiBasicLoopbackEntry m_createdLoopbackEntry{ nullptr };

    };
}
