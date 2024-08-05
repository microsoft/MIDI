// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App SDK and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

#include "pch.h"
#include "MidiVirtualPatchBaySourceDefinition.h"
#include "MidiVirtualPatchBaySourceDefinition.g.cpp"

namespace winrt::Microsoft::Windows::Devices::Midi2::VirtualPatchBay::implementation
{

    collections::IVector<midi2::MidiMessageType> MidiVirtualPatchBaySourceDefinition::IncludedMessageTypes()
    {
        throw hresult_not_implemented();
    }

    _Use_decl_annotations_
    void MidiVirtualPatchBaySourceDefinition::IncludedMessageTypes(
        collections::IVector<midi2::MidiMessageType> const& value)
    {
        UNREFERENCED_PARAMETER(value);

    }

    collections::IVector<midi2::MidiGroup> MidiVirtualPatchBaySourceDefinition::IncludedGroups()
    {
        throw hresult_not_implemented();
    }

    _Use_decl_annotations_
    void MidiVirtualPatchBaySourceDefinition::IncludedGroups(
        collections::IVector<midi2::MidiGroup> const& value)
    {
        UNREFERENCED_PARAMETER(value);

    }
}
