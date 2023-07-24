// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App SDK and should be used
// in your Windows application via an official binary distribution.
// Further information: https://github.com/microsoft/MIDI/
// ============================================================================

#include "pch.h"
#include "MidiUmpUtilities.h"
#include "MidiUmpUtilities.g.cpp"

#include "ump_helpers.h"

using namespace ::Microsoft::Devices::Midi2::Internal;


namespace winrt::Microsoft::Devices::Midi2::implementation
{
    winrt::Windows::Devices::Midi2::MidiUmpMessageType MidiUmpUtilities::GetMessageTypeFromFirstWord(uint32_t firstWord)
    {
        return (winrt::Windows::Devices::Midi2::MidiUmpMessageType)(GetUmpMessageTypeFromFirstWord(firstWord));
    }
    int16_t MidiUmpUtilities::GetUmpLengthInWordsFromFirstWord(uint32_t firstWord)
    {
        return (int16_t)(GetUmpLengthInMidiWordsFromFirstWord(firstWord));
    }
}
