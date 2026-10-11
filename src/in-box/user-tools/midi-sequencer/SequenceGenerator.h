// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

// Generator clips: a Euclidean rhythm and a chance melody. Like a pattern, a generator's notes are
// worked out from its settings, the clip's seed and the pass, so jumping back replays what you
// heard and an export writes it. The arpeggiator isn't built yet, and plays nothing.
// Free of pch.h and XAML like the rest of the model, so the unit tests use the same code.

#include <sal.h>

#include <cstdint>
#include <functional>
#include <vector>

#include "SequenceModel.h"

namespace midisequencer
{
    inline constexpr size_t MaximumEuclidRings = 16;
    inline constexpr uint8_t MaximumRingSteps = 64;
    inline constexpr int64_t MaximumGeneratorSteps = 256;

    // A ring's hits: Pulses spread as evenly as they go over Steps, turned later by Rotate.
    std::vector<bool> EuclideanRhythm(_In_ uint8_t pulses, _In_ uint8_t steps, _In_ uint8_t rotate);

    // The notes of a scale inside a range, lowest first. Never empty: a range with no note of the
    // scale in it gives its lowest note.
    std::vector<uint8_t> ScaleNotes(_In_ uint8_t root, _In_ ScaleKind scale, _In_ uint8_t low, _In_ uint8_t high);

    // Three rings on three notes, like comp 4.
    GeneratorSettings MakeEuclideanGenerator();

    // E minor pentatonic from E3 to E4, changing a little each pass, like comp 4.
    GeneratorSettings MakeChanceMelodyGenerator();

    // Counts, notes and ranges in bounds, and low never above high.
    void NormalizeGenerator(_Inout_ GeneratorSettings& generator);

    // Steps in one pass of a generator clip.
    int64_t GeneratorStepCount(_In_ Clip const& clip) noexcept;

    // Each note one pass of a generator plays that starts inside [fromTick, toTick), in step order.
    // With applyChance off, every note a ring could play is there with the ring's chance on it,
    // which is what drawing wants.
    void ForEachGeneratorNote(
        _In_ Clip const& clip,
        _In_ int64_t passStart,
        _In_ int64_t passEnd,
        _In_ uint64_t pass,
        _In_ int64_t fromTick,
        _In_ int64_t toTick,
        _In_ bool applyChance,
        _In_ std::function<void(Note const&)> const& add);

    // After a generator changes: the notes it draws (the first pass).
    void RefreshGeneratorPreview(_Inout_ Clip& clip);

    // An ordinary notes clip that plays what the generator played. A Euclidean rhythm that's the
    // same each pass gives as many passes as its rings take to line up. Anything that changes each
    // pass gives the first `passes` passes. At most maximumTicks, in whole passes.
    Clip MakeNotesFromGenerator(_In_ Clip const& generator, _In_ uint32_t passes, _In_ int64_t maximumTicks);
}
