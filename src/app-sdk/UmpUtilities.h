// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// Further information: https://github.com/microsoft/MIDI/
// ============================================================================

#pragma once
#include "UmpUtilities.g.h"


namespace winrt::Microsoft::Devices::Midi2::implementation
{
    struct UmpUtilities : UmpUtilitiesT<UmpUtilities>
    {
        UmpUtilities() = default;

        static winrt::Microsoft::Devices::Midi2::MidiMessageType GetMessageTypeFromFirstWord(uint32_t firstWord);
        static int16_t GetUmpLengthInWordsFromFirstWord(uint32_t firstWord);
    };
}
namespace winrt::Microsoft::Devices::Midi2::factory_implementation
{
    struct UmpUtilities : UmpUtilitiesT<UmpUtilities, implementation::UmpUtilities>
    {
    };
}
