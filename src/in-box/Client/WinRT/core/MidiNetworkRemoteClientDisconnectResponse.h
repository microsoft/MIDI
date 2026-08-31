// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App WinRT API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once
#include "Transports.Network.MidiNetworkRemoteClientDisconnectResponse.g.h"

namespace winrt::Windows::Devices::Midi2::Transports::Network::implementation
{
    struct MidiNetworkRemoteClientDisconnectResponse : MidiNetworkRemoteClientDisconnectResponseT<MidiNetworkRemoteClientDisconnectResponse>
    {
        MidiNetworkRemoteClientDisconnectResponse() = default;

        winrt::guid HostId() const noexcept { return m_hostId; }

        winrt::hstring RemoteClientName() const noexcept { return m_remoteClientName; }
        winrt::hstring RemoteClientProductInstanceId() const noexcept { return m_remoteProductInstanceId; }

        bool Success() const noexcept { return m_success; }

        network::MidiNetworkRemoteClientDisconnectErrorCode ErrorCode() const noexcept { return m_errorCode; }
        winrt::hstring ErrorMessage() const noexcept { return m_errorMessage; }

        void InternalSetHostId(_In_ winrt::guid const& hostId) noexcept { m_hostId = hostId; }
        void InternalSetRemoteClientName(_In_ winrt::hstring const& remoteClientName) noexcept { m_remoteClientName = remoteClientName; }
        void InternalSetRemoteClientProductInstanceId(_In_ winrt::hstring const& remoteClientProductInstanceId) noexcept { m_remoteProductInstanceId = remoteClientProductInstanceId; }

        void InternalSetError(_In_ network::MidiNetworkRemoteClientDisconnectErrorCode const errorCode, _In_ winrt::hstring const& errorMessage) noexcept
        {
            m_success = false;
            m_errorCode = errorCode;
            m_errorMessage = errorMessage;
        }

        void InternalSetSuccess() noexcept
        {
            m_success = true;
            m_errorCode = network::MidiNetworkRemoteClientDisconnectErrorCode::NoErrorInformationAvailable;
            m_errorMessage = L"";
        }

    private:
        winrt::guid m_hostId{};
        bool m_success{ false };
        network::MidiNetworkRemoteClientDisconnectErrorCode m_errorCode{};
        winrt::hstring m_errorMessage{};

        winrt::hstring m_remoteClientName{};
        winrt::hstring m_remoteProductInstanceId{};
    };
}
