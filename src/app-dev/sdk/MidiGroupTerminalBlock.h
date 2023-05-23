// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// Further information: https://github.com/microsoft/MIDI/
// ============================================================================

#pragma once
#include "MidiGroupTerminalBlock.g.h"


namespace winrt::Microsoft::Devices::Midi2::implementation
{
    struct MidiGroupTerminalBlock : MidiGroupTerminalBlockT<MidiGroupTerminalBlock>
    {
        MidiGroupTerminalBlock() = default;

    };
}
namespace winrt::Microsoft::Devices::Midi2::factory_implementation
{
    struct MidiGroupTerminalBlock : MidiGroupTerminalBlockT<MidiGroupTerminalBlock, implementation::MidiGroupTerminalBlock>
    {
    };
}
