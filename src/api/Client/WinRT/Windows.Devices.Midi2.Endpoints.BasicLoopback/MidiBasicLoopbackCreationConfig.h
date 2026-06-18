// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services WinRT API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================


#pragma once
#include "MidiBasicLoopbackCreationConfig.g.h"

namespace winrt::Windows::Devices::Midi2::Endpoints::BasicLoopback::implementation
{
    struct MidiBasicLoopbackCreationConfig : MidiBasicLoopbackCreationConfigT<MidiBasicLoopbackCreationConfig>
    {
        MidiBasicLoopbackCreationConfig() = default;

        MidiBasicLoopbackCreationConfig(
            _In_ winrt::guid const& associationId, 
            _In_ bloop::MidiBasicLoopbackEndpointDefinition const& endpointDefinition);

        winrt::guid TransportId() const noexcept { return bloop::MidiBasicLoopbackManager::TransportId(); }
        json::JsonObject ConfigJson() const noexcept;

        bool IsMuted() const noexcept { return m_isMuted; }
        void IsMuted(bool value) { m_isMuted = value; }

        winrt::guid AssociationId() const noexcept { return m_associationId; }
        void AssociationId(_In_ winrt::guid const& value) { m_associationId = value; }

        bloop::MidiBasicLoopbackEndpointDefinition EndpointDefinition() const noexcept { return m_definition; }
        void EndpointDefinition(_In_ bloop::MidiBasicLoopbackEndpointDefinition const& value) { m_definition = value; }


    private:
        winrt::guid m_associationId{};
        bloop::MidiBasicLoopbackEndpointDefinition m_definition{};
        bool m_isMuted{ false };
    };
}
namespace winrt::Windows::Devices::Midi2::Endpoints::BasicLoopback::factory_implementation
{
    struct MidiBasicLoopbackCreationConfig : MidiBasicLoopbackCreationConfigT<MidiBasicLoopbackCreationConfig, implementation::MidiBasicLoopbackCreationConfig>
    {
    };
}
