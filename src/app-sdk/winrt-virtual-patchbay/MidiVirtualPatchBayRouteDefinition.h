// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App SDK and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================


#pragma once
#include "MidiVirtualPatchBayRouteDefinition.g.h"

namespace winrt::Microsoft::Windows::Devices::Midi2::VirtualPatchBay::implementation
{
    struct MidiVirtualPatchBayRouteDefinition : MidiVirtualPatchBayRouteDefinitionT<MidiVirtualPatchBayRouteDefinition>
    {
        MidiVirtualPatchBayRouteDefinition() = default;

        static local::MidiVirtualPatchBayRouteDefinition CreateFromConfigJson(_In_ winrt::hstring const& jsonSection);

        bool IsEnabled() { return m_isEnabled; }
        void IsEnabled(bool value) { m_isEnabled = value; }
        
        collections::IVector<local::MidiVirtualPatchBaySourceDefinition> Sources();
        void Sources(_In_ collections::IVector<local::MidiVirtualPatchBaySourceDefinition> const& value);

        collections::IVector<local::MidiVirtualPatchBayDestinationDefinition> Destinations();
        void Destinations(_In_ collections::IVector<local::MidiVirtualPatchBayDestinationDefinition> const& value);

        hstring GetConfigJson();

    private:
        bool m_isEnabled{ true };
    };
}
namespace winrt::Microsoft::Windows::Devices::Midi2::VirtualPatchBay::factory_implementation
{
    struct MidiVirtualPatchBayRouteDefinition : MidiVirtualPatchBayRouteDefinitionT<MidiVirtualPatchBayRouteDefinition, implementation::MidiVirtualPatchBayRouteDefinition>
    {
    };
}
