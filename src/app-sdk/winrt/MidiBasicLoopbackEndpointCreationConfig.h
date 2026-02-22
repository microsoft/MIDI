// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App SDK and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================


#pragma once
#include "Endpoints.BasicLoopback.MidiBasicLoopbackEndpointCreationConfig.g.h"

namespace winrt::Microsoft::Windows::Devices::Midi2::Endpoints::BasicLoopback::implementation
{
    struct MidiBasicLoopbackEndpointCreationConfig : MidiBasicLoopbackEndpointCreationConfigT<MidiBasicLoopbackEndpointCreationConfig>
    {
        MidiBasicLoopbackEndpointCreationConfig() = default;

        MidiBasicLoopbackEndpointCreationConfig(
            _In_ winrt::guid const& associationId, 
            _In_ bloop::MidiBasicLoopbackEndpointDefinition const& endpointDefinition);

        winrt::guid TransportId() { return bloop::MidiBasicLoopbackEndpointManager::TransportId(); }
        bool IsFromCurrentConfigFile() { return false; }
        json::JsonObject GetConfigJson();


        winrt::guid AssociationId() { return m_associationId; }
        void AssociationId(_In_ winrt::guid const& value) { m_associationId = value; }

        bloop::MidiBasicLoopbackEndpointDefinition EndpointDefinition() { return m_definition; }
        void EndpointDefinition(_In_ bloop::MidiBasicLoopbackEndpointDefinition const& value) { m_definition = value; }


    private:
        winrt::guid m_associationId{};

        bloop::MidiBasicLoopbackEndpointDefinition m_definition{};
    };
}
namespace winrt::Microsoft::Windows::Devices::Midi2::Endpoints::BasicLoopback::factory_implementation
{
    struct MidiBasicLoopbackEndpointCreationConfig : MidiBasicLoopbackEndpointCreationConfigT<MidiBasicLoopbackEndpointCreationConfig, implementation::MidiBasicLoopbackEndpointCreationConfig>
    {
    };
}
