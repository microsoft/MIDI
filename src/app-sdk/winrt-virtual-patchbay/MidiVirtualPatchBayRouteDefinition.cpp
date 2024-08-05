// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App SDK and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

#include "pch.h"
#include "MidiVirtualPatchBayRouteDefinition.h"
#include "MidiVirtualPatchBayRouteDefinition.g.cpp"

namespace winrt::Microsoft::Windows::Devices::Midi2::VirtualPatchBay::implementation
{
    local::MidiVirtualPatchBayRouteDefinition MidiVirtualPatchBayRouteDefinition::CreateFromConfigJson(winrt::hstring const& jsonSection)
    {
        UNREFERENCED_PARAMETER(jsonSection);

        throw hresult_not_implemented();
    }

    collections::IVector<local::MidiVirtualPatchBaySourceDefinition> MidiVirtualPatchBayRouteDefinition::Sources()
    {
        throw hresult_not_implemented();
    }
    void MidiVirtualPatchBayRouteDefinition::Sources(collections::IVector<local::MidiVirtualPatchBaySourceDefinition> const& value)
    {
        UNREFERENCED_PARAMETER(value);

        throw hresult_not_implemented();
    }

    collections::IVector<local::MidiVirtualPatchBayDestinationDefinition> MidiVirtualPatchBayRouteDefinition::Destinations()
    {
        throw hresult_not_implemented();
    }

    void MidiVirtualPatchBayRouteDefinition::Destinations(collections::IVector<local::MidiVirtualPatchBayDestinationDefinition> const& value)
    {
        UNREFERENCED_PARAMETER(value);

        throw hresult_not_implemented();
    }

    winrt::hstring MidiVirtualPatchBayRouteDefinition::GetConfigJson()
    {
        throw hresult_not_implemented();
    }
}
