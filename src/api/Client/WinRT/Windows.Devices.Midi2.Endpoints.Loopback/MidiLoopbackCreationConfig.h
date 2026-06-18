// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services WinRT API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================


#pragma once
#include "MidiLoopbackCreationConfig.g.h"

namespace winrt::Windows::Devices::Midi2::Endpoints::Loopback::implementation
{
    struct MidiLoopbackCreationConfig : MidiLoopbackCreationConfigT<MidiLoopbackCreationConfig>
    {
        MidiLoopbackCreationConfig() = default;

        MidiLoopbackCreationConfig(
            _In_ winrt::guid associationId,
            _In_ loop::MidiLoopbackEndpointDefinition endpointDefinitionA,
            _In_ loop::MidiLoopbackEndpointDefinition endpointDefinitionB
            );

        winrt::guid TransportId() { return loop::MidiLoopbackManager::TransportId(); }
        json::JsonObject ConfigJson();

        bool IsMuted() const noexcept { return m_isMuted; }
        void IsMuted(bool value) { m_isMuted = value; }

        winrt::guid AssociationId() { return m_associationId; }
        void AssociationId(_In_ winrt::guid const& value) { m_associationId = value; }

        loop::MidiLoopbackEndpointDefinition EndpointDefinitionA() { return m_definitionA; }
        void EndpointDefinitionA(_In_ loop::MidiLoopbackEndpointDefinition const& value) { m_definitionA = value; }

        loop::MidiLoopbackEndpointDefinition EndpointDefinitionB() { return m_definitionB; }
        void EndpointDefinitionB(_In_ loop::MidiLoopbackEndpointDefinition const& value) { m_definitionB = value; }

    private:
        winrt::guid m_associationId{};
        bool m_isMuted{ false };
        loop::MidiLoopbackEndpointDefinition m_definitionA{};
        loop::MidiLoopbackEndpointDefinition m_definitionB{};   

    };
}
namespace winrt::Windows::Devices::Midi2::Endpoints::Loopback::factory_implementation
{
    struct MidiLoopbackCreationConfig : MidiLoopbackCreationConfigT<MidiLoopbackCreationConfig, implementation::MidiLoopbackCreationConfig>
    {
    };
}
