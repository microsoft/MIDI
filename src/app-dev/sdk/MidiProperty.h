// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App SDK and should be used
// in your Windows application via an official binary distribution.
// Further information: https://github.com/microsoft/MIDI/
// ============================================================================

#pragma once
#include "MidiProperty.g.h"


namespace winrt::Microsoft::Devices::Midi2::implementation
{
    struct MidiProperty : MidiPropertyT<MidiProperty>
    {
        MidiProperty() = default;

        hstring RawJson();
    };
}
namespace winrt::Microsoft::Devices::Midi2::factory_implementation
{
    struct MidiProperty : MidiPropertyT<MidiProperty, implementation::MidiProperty>
    {
    };
}
