// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App SDK and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once
#include "Transports.Network.MidiNetworkHostCreationResponse.g.h"

namespace winrt::Windows::Devices::Midi2::Transports::Network::implementation
{
    struct MidiNetworkHostCreationResponse : MidiNetworkHostCreationResponseT<MidiNetworkHostCreationResponse>
    {
        MidiNetworkHostCreationResponse() = default;

        winrt::hstring HostId() const noexcept { return m_hostId; }
        bool Success() const noexcept { return m_success; }
        network::MidiNetworkHostCreationErrorCode ErrorCode() const noexcept { return m_errorCode; }
        winrt::hstring ErrorMessage() const noexcept { return m_errorInformation; }

        void InternalSetHostId(_In_ winrt::hstring const& value) noexcept { m_hostId = value; }
        void InternalSetError(_In_ network::MidiNetworkHostCreationErrorCode const errorCode, _In_ winrt::hstring const& errorInformation) noexcept
        {
            m_success = false;
            m_errorCode = errorCode;
            m_errorInformation = errorInformation;
        }

        void InternalSetSuccess() noexcept
        {
            m_success = true;
            m_errorCode = network::MidiNetworkHostCreationErrorCode::NoErrorInformationAvailable;
            m_errorInformation = L"";
        }

    private:
        winrt::hstring m_hostId{};
        bool m_success{ false };
        network::MidiNetworkHostCreationErrorCode m_errorCode{};
        winrt::hstring m_errorInformation{};


    };
}
