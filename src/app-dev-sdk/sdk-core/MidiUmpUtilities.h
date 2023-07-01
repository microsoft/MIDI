// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App SDK and should be used
// in your Windows application via an official binary distribution.
// Further information: https://github.com/microsoft/MIDI/
// ============================================================================

#pragma once
#include "MidiUmpUtilities.g.h"

namespace winrt::Microsoft::Devices::Midi2::implementation
{
    struct MidiUmpUtilities : MidiUmpUtilitiesT<MidiUmpUtilities>
    {
        MidiUmpUtilities() = default;

        static winrt::Microsoft::Devices::Midi2::MidiUmpMessageType GetMessageTypeFromFirstWord(uint32_t firstWord);
        static int16_t GetUmpLengthInWordsFromFirstWord(uint32_t firstWord);
    };
}
namespace winrt::Microsoft::Devices::Midi2::factory_implementation
{
    struct MidiUmpUtilities : MidiUmpUtilitiesT<MidiUmpUtilities, implementation::MidiUmpUtilities>
    {
    };
}
