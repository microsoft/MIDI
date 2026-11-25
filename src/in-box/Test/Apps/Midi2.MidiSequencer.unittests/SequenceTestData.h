// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

#include <cstddef>
#include <cstdint>

#include "SequenceModel.h"

namespace testdata
{
    midisequencer::Note MakeNote(
        int64_t tick,
        int64_t length,
        uint8_t number,
        uint16_t velocity = 0xC000,
        uint8_t channel = 0);

    midisequencer::ClipEvent MakeEvent(int64_t tick, uint32_t word0, uint32_t word1 = 0);

    // Shaped like the comps: tags, a tempo ramp, a meter change, a folder with two tracks, a pinned
    // track, a clip placed twice, a looping clip, scenes and slots, and start-up messages.
    midisequencer::Sequence SampleSequence();

    // One track and one clip with this many notes and controller messages, spread over bars the
    // way a dense recording would be.
    midisequencer::Sequence LargeSequence(size_t noteCount, size_t eventCount);
}
