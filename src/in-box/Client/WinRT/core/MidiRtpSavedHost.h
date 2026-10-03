// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services WinRT API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once
#include "Transports.Rtp.MidiRtpSavedHost.g.h"

namespace winrt::Windows::Devices::Midi2::Transports::Rtp::implementation
{
    struct MidiRtpSavedHost : MidiRtpSavedHostT<MidiRtpSavedHost>
    {
        MidiRtpSavedHost() = default;

        winrt::guid HostId() const noexcept { return m_hostId; }

        winrt::hstring Name() const noexcept { return m_name; }
        winrt::hstring ServiceInstanceName() const noexcept { return m_serviceInstanceName; }

        bool IsEnabled() const noexcept { return m_isEnabled; }

        bool UseAutomaticPortAllocation() const noexcept { return m_useAutomaticPortAllocation; }
        uint16_t ManuallyAssignedPort() const noexcept { return m_manuallyAssignedPort; }
        bool AllowPortFallback() const noexcept { return m_allowPortFallback; }

        bool Advertise() const noexcept { return m_advertise; }

        rtp::MidiRtpRemoteClientPolicy RemoteClientPolicy() const noexcept { return m_remoteClientPolicy; }

        bool SendRecoveryJournal() const noexcept { return m_sendRecoveryJournal; }

        rtp::MidiRtpSendSpeedLimit SendSpeedLimit() const noexcept { return m_sendSpeedLimit; }

        winrt::guid NetworkAdapterId() const noexcept { return m_networkAdapterId; }
        winrt::hstring NetworkAdapterName() const noexcept { return m_networkAdapterName; }
        bool AllowNetworkAdapterFallback() const noexcept { return m_allowNetworkAdapterFallback; }

        collections::IVectorView<rtp::MidiRtpKnownRemoteClient> KnownRemoteClients() const noexcept { return m_knownRemoteClients.GetView(); }

        // The saved entry, and the decisions saved for it, which are kept beside it in the file
        void InternalInitialize(
            _In_ winrt::guid const& hostId,
            _In_ json::JsonObject const& entry,
            _In_ std::vector<json::JsonObject> const& decisions) noexcept;

    private:
        winrt::guid m_hostId{};

        winrt::hstring m_name{};
        winrt::hstring m_serviceInstanceName{};

        bool m_isEnabled{ true };

        bool m_useAutomaticPortAllocation{ false };
        uint16_t m_manuallyAssignedPort{ 0 };
        bool m_allowPortFallback{ true };

        bool m_advertise{ true };

        rtp::MidiRtpRemoteClientPolicy m_remoteClientPolicy{ rtp::MidiRtpRemoteClientPolicy::AllowAny };

        bool m_sendRecoveryJournal{ true };

        rtp::MidiRtpSendSpeedLimit m_sendSpeedLimit{ rtp::MidiRtpSendSpeedLimit::Unlimited };

        winrt::guid m_networkAdapterId{};
        winrt::hstring m_networkAdapterName{};
        bool m_allowNetworkAdapterFallback{ true };

        collections::IVector<rtp::MidiRtpKnownRemoteClient> m_knownRemoteClients{ winrt::single_threaded_vector<rtp::MidiRtpKnownRemoteClient>() };
    };
}
