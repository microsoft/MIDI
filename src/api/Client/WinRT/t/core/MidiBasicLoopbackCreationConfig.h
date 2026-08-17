// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services WinRT API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================


#pragma once
#include "Transports.BasicLoopback.MidiBasicLoopbackCreationConfig.g.h"

namespace winrt::Windows::Devices::Midi2::Transports::BasicLoopback::implementation
{
    struct MidiBasicLoopbackCreationConfig : MidiBasicLoopbackCreationConfigT<MidiBasicLoopbackCreationConfig>
    {
        MidiBasicLoopbackCreationConfig() = default;

        MidiBasicLoopbackCreationConfig(
            _In_ bloop::MidiBasicLoopbackEndpointDefinition const& endpointDefinition);

        winrt::guid TransportId() const noexcept { return bloop::MidiBasicLoopbackManager::TransportId(); }
        json::JsonObject ConfigJson() const noexcept;

        bool IsMuted() const noexcept { return m_isMuted; }
        void IsMuted(_In_ bool value) { m_isMuted = value; }

        winrt::guid AssociationId() const noexcept { return m_associationId; }
        //void AssociationId(_In_ winrt::guid const& value) { m_associationId = value; }

        bloop::MidiBasicLoopbackEndpointDefinition EndpointDefinition() const noexcept { return m_definition; }
        void EndpointDefinition(_In_ bloop::MidiBasicLoopbackEndpointDefinition const& value)
        {
            m_definition = value;
            if (m_definition.UniqueId().empty())
            {
                m_definition.UniqueId(internal::GuidToHexDigitsOnlyString(m_associationId));
            }
        }


    private:
        winrt::guid m_associationId{ foundation::GuidHelper::CreateNewGuid() };
        bloop::MidiBasicLoopbackEndpointDefinition m_definition{};
        bool m_isMuted{ false };
    };
}
namespace winrt::Windows::Devices::Midi2::Transports::BasicLoopback::factory_implementation
{
    struct MidiBasicLoopbackCreationConfig : MidiBasicLoopbackCreationConfigT<MidiBasicLoopbackCreationConfig, implementation::MidiBasicLoopbackCreationConfig>
    {
    };
}
