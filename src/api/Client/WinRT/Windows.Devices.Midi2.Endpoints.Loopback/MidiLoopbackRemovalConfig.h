// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services WinRT API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================


#pragma once
#include "MidiLoopbackRemovalConfig.g.h"

namespace winrt::Windows::Devices::Midi2::Endpoints::Loopback::implementation
{
    struct MidiLoopbackRemovalConfig : MidiLoopbackRemovalConfigT<MidiLoopbackRemovalConfig>
    {
        MidiLoopbackRemovalConfig() = default;
        MidiLoopbackRemovalConfig(_In_ winrt::guid const& associationId);

        winrt::guid TransportId() { return loop::MidiLoopbackManager::TransportId(); }
        bool IsFromCurrentConfigFile() { return false; }
        json::JsonObject ConfigJson();

        winrt::guid AssociationId() { return m_associationId; }
        void AssociationId(_In_ winrt::guid const& value) { m_associationId = value; }

    private:
        winrt::guid m_associationId;

    };
}
namespace winrt::Windows::Devices::Midi2::Endpoints::Loopback::factory_implementation
{
    struct MidiLoopbackRemovalConfig : MidiLoopbackRemovalConfigT<MidiLoopbackRemovalConfig, implementation::MidiLoopbackRemovalConfig>
    {
    };
}
