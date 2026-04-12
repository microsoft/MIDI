// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App SDK and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once
#include "Endpoints.Loopback.MidiLoopbackEndpointCreationResult.g.h"

namespace winrt::Microsoft::Windows::Devices::Midi2::Endpoints::Loopback::implementation
{
    struct MidiLoopbackEndpointCreationResult : MidiLoopbackEndpointCreationResultT<MidiLoopbackEndpointCreationResult>
    {
        MidiLoopbackEndpointCreationResult() = default;

        bool Success() const noexcept { return m_success; };
        void Success(_In_ bool const value) noexcept { m_success = value; }

        loop::MidiLoopbackEndpointCreationResultErrorCode ErrorCode() const noexcept { return m_errorCode; }
        void ErrorCode(_In_ loop::MidiLoopbackEndpointCreationResultErrorCode const& value) noexcept { m_errorCode = value; }

        winrt::hstring ErrorInformation() const noexcept { return m_errorInformation; }
        void ErrorInformation(_In_ winrt::hstring const& value) noexcept { m_errorInformation = value; }

        winrt::guid AssociationId() const noexcept { return m_associationId; }
        void AssociationId(_In_ winrt::guid const& value) noexcept { m_associationId = value; }

        winrt::hstring EndpointDeviceIdA() const noexcept { return m_endpointDeviceIdA; }
        void EndpointDeviceIdA(_In_ winrt::hstring const& value) noexcept { m_endpointDeviceIdA = value; };

        winrt::hstring EndpointDeviceIdB() const noexcept { return m_endpointDeviceIdB; }
        void EndpointDeviceIdB(_In_ winrt::hstring const& value) noexcept { m_endpointDeviceIdB = value; };


    private:
        bool m_success{ false };
        loop::MidiLoopbackEndpointCreationResultErrorCode m_errorCode{ loop::MidiLoopbackEndpointCreationResultErrorCode::NoErrorInformationAvailable };
        winrt::hstring m_errorInformation{ };
        winrt::hstring m_endpointDeviceIdA{ };
        winrt::hstring m_endpointDeviceIdB{ };
        winrt::guid m_associationId{ };
    };
}
