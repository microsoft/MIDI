// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://github.com/microsoft/MIDI/
// ============================================================================

#pragma once
#include "MidiLoopbackEndpointCreationConfiguration.g.h"

namespace winrt::Microsoft::Devices::Midi2::Endpoints::Loopback::implementation
{
    struct MidiLoopbackEndpointCreationConfiguration : MidiLoopbackEndpointCreationConfigurationT<MidiLoopbackEndpointCreationConfiguration>
    {
        MidiLoopbackEndpointCreationConfiguration() = default;

        MidiLoopbackEndpointCreationConfiguration(
            _In_ winrt::guid associationId,
            _In_ loop::MidiLoopbackEndpointDefinition endpointDefinitionA,
            _In_ loop::MidiLoopbackEndpointDefinition endpointDefinitionB
            );

        winrt::guid TransportId() { return loop::MidiLoopbackEndpointManager::AbstractionId(); }
        bool IsFromConfigurationFile() { return false; }
        winrt::hstring GetConfigurationJson();


        winrt::guid AssociationId() { return m_associationId; }
        void AssociationId(_In_ winrt::guid const& value) { m_associationId = value; }

        loop::MidiLoopbackEndpointDefinition EndpointDefinitionA() { return m_definitionA; }
        void EndpointDefinitionA(_In_ loop::MidiLoopbackEndpointDefinition const& value) { m_definitionA = value; }

        loop::MidiLoopbackEndpointDefinition EndpointDefinitionB() { return m_definitionB; }
        void EndpointDefinitionB(_In_ loop::MidiLoopbackEndpointDefinition const& value) { m_definitionB = value; }

    private:
        winrt::guid m_associationId;

        loop::MidiLoopbackEndpointDefinition m_definitionA{};
        loop::MidiLoopbackEndpointDefinition m_definitionB{};

    };
}
namespace winrt::Microsoft::Devices::Midi2::Endpoints::Loopback::factory_implementation
{
    struct MidiLoopbackEndpointCreationConfiguration : MidiLoopbackEndpointCreationConfigurationT<MidiLoopbackEndpointCreationConfiguration, implementation::MidiLoopbackEndpointCreationConfiguration>
    {
    };
}
