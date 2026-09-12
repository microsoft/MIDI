// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App WinRT API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once
#include "Transports.Network.MidiNetworkClientUpdateResponse.g.h"

namespace winrt::Windows::Devices::Midi2::Transports::Network::implementation
{
    struct MidiNetworkClientUpdateResponse : MidiNetworkClientUpdateResponseT<MidiNetworkClientUpdateResponse>
    {
        MidiNetworkClientUpdateResponse() = default;

        winrt::guid ClientId() const noexcept { return m_clientId; }
        bool Success() const noexcept { return m_success; }
        network::MidiNetworkClientUpdateErrorCode ErrorCode() const noexcept { return m_errorCode; }
        winrt::hstring ErrorMessage() const noexcept { return m_errorInformation; }

        void InternalSetClientId(_In_ winrt::guid const& value) noexcept { m_clientId = value; }
        void InternalSetError(_In_ network::MidiNetworkClientUpdateErrorCode const errorCode, _In_ winrt::hstring const& errorInformation) noexcept
        {
            m_success = false;
            m_errorCode = errorCode;
            m_errorInformation = errorInformation;
        }

        void InternalSetSuccess() noexcept
        {
            m_success = true;
            m_errorCode = network::MidiNetworkClientUpdateErrorCode::NoErrorInformationAvailable;
            m_errorInformation = L"";
        }

    private:
        winrt::guid m_clientId{};
        bool m_success{ false };
        network::MidiNetworkClientUpdateErrorCode m_errorCode{};
        winrt::hstring m_errorInformation{};


    };
}
