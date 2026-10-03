// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services WinRT API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once
#include "Transports.Rtp.MidiRtpConfiguredHost.g.h"

#include "MidiRtpSdkJson.h"

namespace winrt::Windows::Devices::Midi2::Transports::Rtp::implementation
{
    struct MidiRtpConfiguredHost : MidiRtpConfiguredHostT<MidiRtpConfiguredHost>
    {
        MidiRtpConfiguredHost() = default;

        winrt::guid HostId() const noexcept { return m_hostId; }
        winrt::hstring Name() const noexcept { return m_name; }
        winrt::hstring ServiceInstanceName() const noexcept { return m_serviceInstanceName; }
        winrt::hstring ActualServiceInstanceName() const noexcept { return m_actualServiceInstanceName; }
        bool ServiceInstanceNameWasChanged() const noexcept { return m_serviceInstanceNameWasChanged; }
        bool IsEnabled() const noexcept { return m_isEnabled; }
        bool HasStarted() const noexcept { return m_hasStarted; }
        bool Advertise() const noexcept { return m_advertise; }
        winrt::hstring ConfiguredPort() const noexcept { return m_configuredPort; }
        uint16_t ActualPort() const noexcept { return m_actualPort; }
        bool AllowPortFallback() const noexcept { return m_allowPortFallback; }
        bool UsedPortFallback() const noexcept { return m_usedPortFallback; }
        bool SendRecoveryJournal() const noexcept { return m_sendRecoveryJournal; }
        rtp::MidiRtpSendSpeedLimit SendSpeedLimit() const noexcept { return m_sendSpeedLimit; }
        rtp::MidiRtpSendSpeedLimit CurrentSendSpeedLimit() const noexcept { return m_currentSendSpeedLimit; }
        winrt::guid NetworkAdapterId() const noexcept { return m_networkAdapterId; }
        winrt::hstring NetworkAdapterName() const noexcept { return m_networkAdapterName; }
        bool AllowNetworkAdapterFallback() const noexcept { return m_allowNetworkAdapterFallback; }
        bool IsNetworkAdapterMissing() const noexcept { return m_isNetworkAdapterMissing; }
        bool UsedNetworkAdapterFallback() const noexcept { return m_usedNetworkAdapterFallback; }
        rtp::MidiRtpRemoteClientPolicy RemoteClientPolicy() const noexcept { return m_remoteClientPolicy; }
        collections::IVectorView<rtp::MidiRtpKnownRemoteClient> KnownRemoteClients() const noexcept { return m_knownRemoteClients.GetView(); }
        int32_t LastErrorCode() const noexcept { return m_lastErrorCode; }
        collections::IVectorView<rtp::MidiRtpConnection> Connections() const noexcept { return m_connections.GetView(); }

        // false when the entry has no usable identifier
        bool InternalInitialize(_In_ json::JsonObject const& source) noexcept;

    private:
        winrt::guid m_hostId{};
        winrt::hstring m_name{};
        winrt::hstring m_serviceInstanceName{};
        winrt::hstring m_actualServiceInstanceName{};
        bool m_serviceInstanceNameWasChanged{ false };
        bool m_isEnabled{ false };
        bool m_hasStarted{ false };
        bool m_advertise{ false };
        winrt::hstring m_configuredPort{};
        uint16_t m_actualPort{ 0 };
        bool m_allowPortFallback{ false };
        bool m_usedPortFallback{ false };
        bool m_sendRecoveryJournal{ false };
        rtp::MidiRtpSendSpeedLimit m_sendSpeedLimit{ rtp::MidiRtpSendSpeedLimit::Unlimited };
        rtp::MidiRtpSendSpeedLimit m_currentSendSpeedLimit{ rtp::MidiRtpSendSpeedLimit::Unlimited };
        winrt::guid m_networkAdapterId{};
        winrt::hstring m_networkAdapterName{};
        bool m_allowNetworkAdapterFallback{ true };
        bool m_isNetworkAdapterMissing{ false };
        bool m_usedNetworkAdapterFallback{ false };
        rtp::MidiRtpRemoteClientPolicy m_remoteClientPolicy{ rtp::MidiRtpRemoteClientPolicy::AllowAny };
        collections::IVector<rtp::MidiRtpKnownRemoteClient> m_knownRemoteClients{ winrt::single_threaded_vector<rtp::MidiRtpKnownRemoteClient>() };
        int32_t m_lastErrorCode{ 0 };
        collections::IVector<rtp::MidiRtpConnection> m_connections{ winrt::single_threaded_vector<rtp::MidiRtpConnection>() };
    };
}
