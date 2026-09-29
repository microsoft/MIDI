// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App WinRT API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once
#include "Transports.Network.MidiNetworkSavedHost.g.h"

namespace winrt::Windows::Devices::Midi2::Transports::Network::implementation
{
    struct MidiNetworkSavedHost : MidiNetworkSavedHostT<MidiNetworkSavedHost>
    {
        MidiNetworkSavedHost() = default;

        winrt::guid HostId() const noexcept { return m_hostId; }

        winrt::hstring Name() const noexcept { return m_name; }
        winrt::hstring ServiceInstanceName() const noexcept { return m_serviceInstanceName; }
        winrt::hstring ProductInstanceId() const noexcept { return m_productInstanceId; }

        bool IsEnabled() const noexcept { return m_isEnabled; }

        bool CreateOnlyUmpEndpoints() const noexcept { return m_createOnlyUmpEndpoints; }
        uint8_t FallbackMidi1PortCount() const noexcept { return m_fallbackMidi1PortCount; }

        bool UseAutomaticPortAllocation() const noexcept { return m_useAutomaticPortAllocation; }
        winrt::hstring ManuallyAssignedPort() const noexcept { return m_manuallyAssignedPort; }
        bool AllowPortFallback() const noexcept { return m_allowPortFallback; }

        bool Advertise() const noexcept { return m_advertise; }

        network::MidiNetworkRemoteClientPolicy RemoteClientPolicy() const noexcept { return m_remoteClientPolicy; }

        collections::IVectorView<network::MidiNetworkKnownRemoteClient> KnownRemoteClients() const noexcept { return m_knownRemoteClients.GetView(); }

        // The saved entry, then each saved change to it in the order the service applies them
        void InternalInitialize(
            _In_ winrt::guid const& hostId,
            _In_ json::JsonObject const& entry,
            _In_ std::vector<json::JsonObject> const& updates) noexcept;

    private:
        winrt::guid m_hostId{};

        winrt::hstring m_name{};
        winrt::hstring m_serviceInstanceName{};
        winrt::hstring m_productInstanceId{};

        bool m_isEnabled{ true };

        bool m_createOnlyUmpEndpoints{ false };
        uint8_t m_fallbackMidi1PortCount{ 1 };

        bool m_useAutomaticPortAllocation{ true };
        winrt::hstring m_manuallyAssignedPort{};
        bool m_allowPortFallback{ true };

        bool m_advertise{ true };

        network::MidiNetworkRemoteClientPolicy m_remoteClientPolicy{ network::MidiNetworkRemoteClientPolicy::AllowAny };

        collections::IVector<network::MidiNetworkKnownRemoteClient> m_knownRemoteClients{ winrt::single_threaded_vector<network::MidiNetworkKnownRemoteClient>() };
    };
}
