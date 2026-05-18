// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App SDK and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once
#include "Endpoints.Network.MidiNetworkHostCreationResult.g.h"

namespace winrt::Microsoft::Windows::Devices::Midi2::Endpoints::Network::implementation
{
    struct MidiNetworkHostCreationResult : MidiNetworkHostCreationResultT<MidiNetworkHostCreationResult>
    {
        MidiNetworkHostCreationResult() = default;

        winrt::hstring HostId() const noexcept { return m_hostId; }
        bool Success() const noexcept { return m_success; }
        network::MidiNetworkHostCreationResultErrorCode ErrorCode() const noexcept { return m_errorCode; }
        winrt::hstring ErrorInformation() const noexcept { return m_errorInformation; }

        void InternalSetHostId(_In_ winrt::hstring const& value) noexcept { m_hostId = value; }
        void InternalSetError(_In_ network::MidiNetworkHostCreationResultErrorCode const errorCode, _In_ winrt::hstring const& errorInformation) noexcept
        {
            m_success = false;
            m_errorCode = errorCode;
            m_errorInformation = errorInformation;
        }

        void InternalSetSuccess() noexcept
        {
            m_success = true;
            m_errorCode = network::MidiNetworkHostCreationResultErrorCode::NoErrorInformationAvailable;
            m_errorInformation = L"";
        }

    private:
        winrt::hstring m_hostId{};
        bool m_success{ false };
        network::MidiNetworkHostCreationResultErrorCode m_errorCode{};
        winrt::hstring m_errorInformation{};


    };
}
