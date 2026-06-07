// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services WinRT API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once
#include "MidiLoopbackEndpointDefinition.g.h"

namespace winrt::Windows::Devices::Midi2::Endpoints::Loopback::implementation
{
    struct MidiLoopbackEndpointDefinition : MidiLoopbackEndpointDefinitionT<MidiLoopbackEndpointDefinition>
    {
        MidiLoopbackEndpointDefinition() = default;

        MidiLoopbackEndpointDefinition(hstring const& name, hstring const& uniqueId);
        MidiLoopbackEndpointDefinition(hstring const& name, hstring const& uniqueId, hstring const& description);
        hstring Name();
        void Name(hstring const& value);
        hstring UniqueId();
        void UniqueId(hstring const& value);
        hstring Description();
        void Description(hstring const& value);
    };
}
namespace winrt::Windows::Devices::Midi2::Endpoints::Loopback::factory_implementation
{
    struct MidiLoopbackEndpointDefinition : MidiLoopbackEndpointDefinitionT<MidiLoopbackEndpointDefinition, implementation::MidiLoopbackEndpointDefinition>
    {
    };
}
