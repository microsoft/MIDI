// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App SDK and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once
#include "Endpoints.Network.MidiNetworkClientDisconnectResult.g.h"

namespace winrt::Microsoft::Windows::Devices::Midi2::Endpoints::Network::implementation
{
    struct MidiNetworkClientDisconnectResult : MidiNetworkClientDisconnectResultT<MidiNetworkClientDisconnectResult>
    {
        MidiNetworkClientDisconnectResult() = default;

        winrt::guid ClientId() const noexcept { return m_clientId; }
        bool Success() const noexcept { return m_success; }
        network::MidiNetworkClientDisconnectResultErrorCode ErrorCode() const noexcept { return m_errorCode; }
        winrt::hstring ErrorInformation() const noexcept { return m_errorInformation; }

        void InternalSetClientId(_In_ winrt::guid const& value) noexcept { m_clientId = value; }
        void InternalSetError(_In_ network::MidiNetworkClientDisconnectResultErrorCode const errorCode, _In_ winrt::hstring const& errorInformation) noexcept
        {
            m_success = false;
            m_errorCode = errorCode;
            m_errorInformation = errorInformation;
        }

        void InternalSetSuccess() noexcept
        {
            m_success = true;
            m_errorCode = network::MidiNetworkClientDisconnectResultErrorCode::NoErrorInformationAvailable;
            m_errorInformation = L"";
        }

    private:
        winrt::guid m_clientId{};
        bool m_success{ false };
        network::MidiNetworkClientDisconnectResultErrorCode m_errorCode{};
        winrt::hstring m_errorInformation{};


    };
}
