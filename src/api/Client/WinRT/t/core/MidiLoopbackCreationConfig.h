// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services WinRT API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================


#pragma once
#include "Transports.Loopback.MidiLoopbackCreationConfig.g.h"

namespace winrt::Windows::Devices::Midi2::Transports::Loopback::implementation
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
        void IsMuted(bool value) noexcept { m_isMuted = value; }

        winrt::guid AssociationId() noexcept { return m_associationId; }
        void AssociationId(_In_ winrt::guid const& value) noexcept { m_associationId = value; }

        loop::MidiLoopbackEndpointDefinition EndpointDefinitionA() noexcept { return m_definitionA; }
        void EndpointDefinitionA(_In_ loop::MidiLoopbackEndpointDefinition const& value) noexcept { m_definitionA = value; }

        loop::MidiLoopbackEndpointDefinition EndpointDefinitionB() noexcept { return m_definitionB; }
        void EndpointDefinitionB(_In_ loop::MidiLoopbackEndpointDefinition const& value) noexcept { m_definitionB = value; }

    private:
        winrt::guid m_associationId{};
        bool m_isMuted{ false };
        loop::MidiLoopbackEndpointDefinition m_definitionA{ nullptr };
        loop::MidiLoopbackEndpointDefinition m_definitionB{ nullptr };   

    };
}
namespace winrt::Windows::Devices::Midi2::Transports::Loopback::factory_implementation
{
    struct MidiLoopbackCreationConfig : MidiLoopbackCreationConfigT<MidiLoopbackCreationConfig, implementation::MidiLoopbackCreationConfig>
    {
    };
}
