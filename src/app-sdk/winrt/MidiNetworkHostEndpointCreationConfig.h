// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App SDK and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once
#include "Endpoints.Network.MidiNetworkHostEndpointCreationConfig.g.h"

namespace winrt::Microsoft::Windows::Devices::Midi2::Endpoints::Network::implementation
{
    struct MidiNetworkHostEndpointCreationConfig : MidiNetworkHostEndpointCreationConfigT<MidiNetworkHostEndpointCreationConfig>
    {
        MidiNetworkHostEndpointCreationConfig() = default;

        winrt::guid Identifier() { return m_identifier; }
        void Identifier(_In_ winrt::guid const& value) { m_identifier = value; }

        winrt::hstring Name() { return m_name; }
        void Name(_In_ winrt::hstring const& value) { m_name = internal::TrimmedHStringCopy(value); }

        winrt::hstring HostInstanceName() { return m_hostInstanceName; }
        void HostInstanceName(_In_ winrt::hstring const& value) { m_hostInstanceName = internal::TrimmedHStringCopy(value); }

        winrt::hstring ProductInstanceId() { return m_productInstanceId; }
        void ProductInstanceId(_In_ winrt::hstring const& value) { m_productInstanceId = internal::TrimmedHStringCopy(value); }

        bool UmpOnly() { return m_umpOnly; }
        void UmpOnly(_In_ bool const value) { m_umpOnly = value; }

        bool UseAutomaticPortAllocation() { return m_useAutomaticPortAllocation; }
        void UseAutomaticPortAllocation(_In_ bool const value) { m_useAutomaticPortAllocation = value; }

        winrt::hstring ManuallyAssignedPort() { return m_manuallyAssignedPort; }
        void ManuallyAssignedPort(_In_ winrt::hstring const& value) { m_manuallyAssignedPort = internal::TrimmedHStringCopy(value); }

        bool Advertise() { return m_advertise; }
        void Advertise(_In_ bool const value) { m_advertise = value; }

        collections::IVector<winrt::hstring> AllowedClientConnectionList() { return m_allowedClientConnectionList; }

        network::MidiNetworkAuthenticationType AuthenticationType() { return m_authenticationType; }
        void AuthenticationType(_In_ network::MidiNetworkAuthenticationType const& value) { m_authenticationType = value; }

        winrt::guid TransportId() { return m_transportId; }
        winrt::hstring GetConfigJson();

    private:
        winrt::guid m_identifier{};
        winrt::hstring m_name;
        winrt::hstring m_hostInstanceName;
        winrt::hstring m_productInstanceId;
        bool m_umpOnly{ true };
        bool m_useAutomaticPortAllocation{ true };
        winrt::hstring m_manuallyAssignedPort{};
        bool m_advertise{ true };

        network::MidiNetworkAuthenticationType m_authenticationType{ network::MidiNetworkAuthenticationType::NoAuthentication };

        winrt::guid m_transportId{};

        collections::IVector<winrt::hstring> m_allowedClientConnectionList{
            winrt::multi_threaded_vector<winrt::hstring>() };

    };
}
namespace winrt::Microsoft::Windows::Devices::Midi2::Endpoints::Network::factory_implementation
{
    struct MidiNetworkHostEndpointCreationConfig : MidiNetworkHostEndpointCreationConfigT<MidiNetworkHostEndpointCreationConfig, implementation::MidiNetworkHostEndpointCreationConfig>
    {
    };
}
