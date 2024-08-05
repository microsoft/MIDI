// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App SDK and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

#include "pch.h"
#include "MidiVirtualPatchBayDestinationDefinition.h"
#include "MidiVirtualPatchBayDestinationDefinition.g.cpp"

namespace winrt::Microsoft::Windows::Devices::Midi2::VirtualPatchBay::implementation
{


    collections::IMap<midi2::MidiGroup, midi2::MidiGroup> MidiVirtualPatchBayDestinationDefinition::GroupTransformMap()
    {
        throw hresult_not_implemented();
    }

    void MidiVirtualPatchBayDestinationDefinition::GroupTransformMap(collections::IMap<midi2::MidiGroup, midi2::MidiGroup> const& value)
    {
        UNREFERENCED_PARAMETER(value);

        throw hresult_not_implemented();
    }
}
