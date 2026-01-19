// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App SDK and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

#include "pch.h"
#include "MidiVirtualPatchBaySourceDefinition.h"
#include "VirtualPatchBay.MidiVirtualPatchBaySourceDefinition.g.cpp"

namespace winrt::Microsoft::Windows::Devices::Midi2::VirtualPatchBay::implementation
{

    _Use_decl_annotations_
    vpb::MidiVirtualPatchBaySourceDefinition MidiVirtualPatchBaySourceDefinition::CreateFromConfigJson(
        winrt::hstring const& jsonSection) noexcept
    {
        // TODO

        UNREFERENCED_PARAMETER(jsonSection);

        return nullptr;

    }

    json::JsonObject MidiVirtualPatchBaySourceDefinition::GetConfigJson() const noexcept
    {
        // TODO

        return nullptr;
    }



}
