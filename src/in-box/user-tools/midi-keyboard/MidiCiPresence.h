// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once
// The MIDI-CI presence is shared with MIDI Sequencer, in midi-app-shared. This one says it's
// MIDI Keyboard, with the keyboard's own model number.

#include "../midi-app-shared/MidiCiAppPresence.h"

#include <MidiDefs.h>

namespace midikeyboard
{
    class MidiCiPresence final : public midiapp::MidiCiPresence
    {
    public:
        MidiCiPresence() noexcept : midiapp::MidiCiPresence(MIDI_DEVICE_FAMILY_MODEL_NUMBER_MIDI_KEYBOARD) {}
    };
}