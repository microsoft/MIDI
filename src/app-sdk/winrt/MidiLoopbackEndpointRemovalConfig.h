// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App SDK and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================


#pragma once
#include "Endpoints.Loopback.MidiLoopbackEndpointRemovalConfig.g.h"

namespace winrt::Microsoft::Windows::Devices::Midi2::Endpoints::Loopback::implementation
{
    struct MidiLoopbackEndpointRemovalConfig : MidiLoopbackEndpointRemovalConfigT<MidiLoopbackEndpointRemovalConfig>
    {
        MidiLoopbackEndpointRemovalConfig() = default;
        MidiLoopbackEndpointRemovalConfig(_In_ winrt::guid const& associationId);

        winrt::guid TransportId() { return loop::MidiLoopbackEndpointManager::TransportId(); }
        bool IsFromCurrentConfigFile() { return false; }
        winrt::hstring GetConfigJson();

        winrt::guid AssociationId() { return m_associationId; }
        void AssociationId(_In_ winrt::guid const& value) { m_associationId = value; }

    private:
        winrt::guid m_associationId;

    };
}
namespace winrt::Microsoft::Windows::Devices::Midi2::Endpoints::Loopback::factory_implementation
{
    struct MidiLoopbackEndpointRemovalConfig : MidiLoopbackEndpointRemovalConfigT<MidiLoopbackEndpointRemovalConfig, implementation::MidiLoopbackEndpointRemovalConfig>
    {
    };
}
