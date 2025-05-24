// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App SDK and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once
#include "Endpoints.Network.MidiNetworkHostCreationConfig.g.h"

namespace winrt::Microsoft::Windows::Devices::Midi2::Endpoints::Network::implementation
{
    struct MidiNetworkHostCreationConfig : MidiNetworkHostCreationConfigT<MidiNetworkHostCreationConfig>
    {
        MidiNetworkHostCreationConfig() = default;

        winrt::guid TransportId() const noexcept { return internal::StringToGuid(MIDI_NETWORK_TRANSPORT_ID); }
        winrt::hstring GetConfigJson() const noexcept;

        winrt::hstring Id() const noexcept { return m_id; }
        void Id(_In_ winrt::hstring const& value) { m_id = value; }

        winrt::hstring Name() const noexcept { return m_name; }
        void Name(_In_ winrt::hstring const& value) { m_name = internal::TrimmedHStringCopy(value); }

        winrt::hstring HostInstanceName() const noexcept { return m_hostInstanceName; }
        void HostInstanceName(_In_ winrt::hstring const& value) { m_hostInstanceName = internal::TrimmedHStringCopy(value); }

        winrt::hstring ProductInstanceId() const noexcept { return m_productInstanceId; }
        void ProductInstanceId(_In_ winrt::hstring const& value) { m_productInstanceId = internal::TrimmedHStringCopy(value); }

        bool UmpOnly() const noexcept { return m_umpOnly; }
        void UmpOnly(_In_ bool const value) { m_umpOnly = value; }

        bool UseAutomaticPortAllocation() const noexcept { return m_useAutomaticPortAllocation; }
        void UseAutomaticPortAllocation(_In_ bool const value) { m_useAutomaticPortAllocation = value; }

        winrt::hstring ManuallyAssignedPort() const noexcept { return m_manuallyAssignedPort; }
        void ManuallyAssignedPort(_In_ winrt::hstring const& value) { m_manuallyAssignedPort = internal::TrimmedHStringCopy(value); }

        bool Advertise() const noexcept { return m_advertise; }
        void Advertise(_In_ bool const value) { m_advertise = value; }

        collections::IVector<winrt::Windows::Networking::HostName> AllowedClientConnectionList() { return m_allowedClientConnectionList; }

        network::MidiNetworkAuthenticationType AuthenticationType() { return m_authenticationType; }
        void AuthenticationType(_In_ network::MidiNetworkAuthenticationType const& value) { m_authenticationType = value; }


    private:
        winrt::hstring m_id{};
        winrt::hstring m_name;
        winrt::hstring m_hostInstanceName;
        winrt::hstring m_productInstanceId;
        bool m_umpOnly{ true };
        bool m_useAutomaticPortAllocation{ true };
        winrt::hstring m_manuallyAssignedPort{};
        bool m_advertise{ true };

        network::MidiNetworkAuthenticationType m_authenticationType{ network::MidiNetworkAuthenticationType::NoAuthentication };

        collections::IVector<winrt::Windows::Networking::HostName> m_allowedClientConnectionList{
            winrt::multi_threaded_vector<winrt::Windows::Networking::HostName>() };

    };
}
namespace winrt::Microsoft::Windows::Devices::Midi2::Endpoints::Network::factory_implementation
{
    struct MidiNetworkHostCreationConfig : MidiNetworkHostCreationConfigT<MidiNetworkHostCreationConfig, implementation::MidiNetworkHostCreationConfig>
    {
    };
}
