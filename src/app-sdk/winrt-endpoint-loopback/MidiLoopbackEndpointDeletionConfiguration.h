// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once
#include "MidiLoopbackEndpointDeletionConfiguration.g.h"

namespace winrt::Microsoft::Devices::Midi2::Endpoints::Loopback::implementation
{
    struct MidiLoopbackEndpointDeletionConfiguration : MidiLoopbackEndpointDeletionConfigurationT<MidiLoopbackEndpointDeletionConfiguration>
    {
        MidiLoopbackEndpointDeletionConfiguration() = default;
        MidiLoopbackEndpointDeletionConfiguration(_In_ winrt::guid const& associationId);

        winrt::guid TransportId() { return loop::MidiLoopbackEndpointManager::AbstractionId(); }
        bool IsFromConfigurationFile() { return false; }
        winrt::hstring GetConfigurationJson();

        winrt::guid AssociationId() { return m_associationId; }
        void AssociationId(_In_ winrt::guid const& value) { m_associationId = value; }

    private:
        winrt::guid m_associationId;

    };
}
namespace winrt::Microsoft::Devices::Midi2::Endpoints::Loopback::factory_implementation
{
    struct MidiLoopbackEndpointDeletionConfiguration : MidiLoopbackEndpointDeletionConfigurationT<MidiLoopbackEndpointDeletionConfiguration, implementation::MidiLoopbackEndpointDeletionConfiguration>
    {
    };
}
