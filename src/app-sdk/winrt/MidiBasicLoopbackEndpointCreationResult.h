// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App SDK and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once
#include "Endpoints.BasicLoopback.MidiBasicLoopbackEndpointCreationResult.g.h"

namespace winrt::Microsoft::Windows::Devices::Midi2::Endpoints::BasicLoopback::implementation
{
    struct MidiBasicLoopbackEndpointCreationResult : MidiBasicLoopbackEndpointCreationResultT<MidiBasicLoopbackEndpointCreationResult>
    {
        MidiBasicLoopbackEndpointCreationResult() = default;

        bool Success() const noexcept { return m_success; };
        void Success(_In_ bool const value) noexcept { m_success = value; }

        bloop::MidiBasicLoopbackEndpointCreationResultErrorCode ErrorCode() const noexcept { return m_errorCode; }
        void ErrorCode(_In_ bloop::MidiBasicLoopbackEndpointCreationResultErrorCode const& value) noexcept { m_errorCode = value; }

        winrt::hstring ErrorInformation() const noexcept { return m_errorInformation; }
        void ErrorInformation(_In_ winrt::hstring const& value) noexcept { m_errorInformation = value; }

        winrt::guid AssociationId() const noexcept { return m_associationId; }
        void AssociationId(_In_ winrt::guid const& value) noexcept { m_associationId = value; }

        winrt::hstring EndpointDeviceId() const noexcept { return m_endpointDeviceId; }
        void EndpointDeviceId(_In_ winrt::hstring const& value) noexcept { m_endpointDeviceId = value; };


    private:
        bool m_success{ false };
        bloop::MidiBasicLoopbackEndpointCreationResultErrorCode m_errorCode { bloop::MidiBasicLoopbackEndpointCreationResultErrorCode::NoErrorInformationAvailable };
        winrt::hstring m_errorInformation{ };
        winrt::hstring m_endpointDeviceId{ };
        winrt::guid m_associationId{ };

    };
}
