// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// Further information: https://github.com/microsoft/MIDI/
// ============================================================================

#pragma once
#include "MidiGroup.g.h"


namespace winrt::Microsoft::Devices::Midi2::implementation
{
    struct MidiGroup : MidiGroupT<MidiGroup>
    {
        MidiGroup() = default;

        uint8_t GroupIndex();
        uint8_t GroupNumberForDisplay();
        bool IsActive();
    };
}
namespace winrt::Microsoft::Devices::Midi2::factory_implementation
{
    struct MidiGroup : MidiGroupT<MidiGroup, implementation::MidiGroup>
    {
    };
}
