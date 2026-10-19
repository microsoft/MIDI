// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

// Pattern clips: the step sequencer. A pattern is a clip like any other. Its notes are worked out
// from its steps, and what a step plays depends only on the clip's seed and the pass, so playing
// from the start, jumping back and exporting all give the same hits.
// Free of pch.h and XAML like the rest of the model, so the unit tests use the same code.

#include <sal.h>

#include <cstdint>
#include <functional>
#include <string>
#include <string_view>
#include <vector>

#include "SequenceModel.h"

namespace midisequencer
{
    inline constexpr uint16_t MaximumPatternSteps = 64;
    inline constexpr size_t MaximumPatternLanes = 64;
    inline constexpr uint8_t MaximumRatchet = 8;
    inline constexpr uint16_t MaximumStepLength = 1600;     // percent of a step
    inline constexpr uint8_t MinimumSwing = 50;
    inline constexpr uint8_t MaximumSwing = 75;

    // The step sizes offered, in ticks: 1/32, 1/16 triplet, 1/16, 1/8 triplet, 1/8, 1/4.
    inline constexpr int64_t PatternStepSizes[]{ 120, 160, 240, 320, 480, 960 };

    // A drum pattern with kick, snare, closed and open hi-hat lanes and nothing on yet.
    PatternSettings MakeDrumPattern();

    // A melodic pattern: one lane, 16 sixteenths, on channel 1.
    PatternSettings MakeMelodicPattern();

    PatternLane MakePatternLane(_In_ uint8_t note, _In_ uint16_t stepCount);

    // Counts, notes and steps in range, steps in order and one per index, an id for each lane, and
    // exactly one lane in melodic mode.
    void NormalizePattern(_Inout_ PatternSettings& pattern);

    // How many steps a lane runs before it starts again.
    uint16_t LaneStepCount(_In_ PatternSettings const& pattern, _In_ PatternLane const& lane) noexcept;

    // A new number of steps for the pattern. Lanes that ran the old number follow it, and so does
    // a melodic pattern's lane. A lane with a length of its own keeps it.
    void SetPatternStepCount(_Inout_ PatternSettings& pattern, _In_ uint16_t count) noexcept;

    // Moves a lane to just before the lane now at `before`, or to the end when `before` is the
    // number of lanes, the way dropping a dragged lane does. False when that's where it already is.
    bool MovePatternLane(_Inout_ PatternSettings& pattern, _In_ size_t lane, _In_ size_t before) noexcept;

    PatternStep const* FindStep(_In_ PatternLane const& lane, _In_ uint16_t index) noexcept;
    PatternStep* FindStep(_Inout_ PatternLane& lane, _In_ uint16_t index) noexcept;

    // Turns a step on with the defaults, or off. Returns the step when it's on.
    PatternStep* SetStepOn(_Inout_ PatternLane& lane, _In_ uint16_t index, _In_ bool on, _In_ uint8_t note);

    // Whether a step plays on a pass through its lane: its condition first, then its chance,
    // worked out from the clip's seed, the lane and the pass.
    bool StepPlaysOnPass(
        _In_ PatternStep const& step,
        _In_ uint32_t clipSeed,
        _In_ std::wstring_view laneId,
        _In_ uint64_t lanePass) noexcept;

    // Each note one pass of a pattern plays that starts inside [fromTick, toTick), in step order.
    // The pass starts at passStart and is cut off at passEnd. With applyChance off, every step
    // that could play does, with its chance kept on the note, which is what drawing and Make
    // notes want. Conditions always apply.
    void ForEachPatternNote(
        _In_ Clip const& clip,
        _In_ int64_t passStart,
        _In_ int64_t passEnd,
        _In_ uint64_t pass,
        _In_ int64_t fromTick,
        _In_ int64_t toTick,
        _In_ bool applyChance,
        _In_ std::function<void(Note const&)> const& add);

    // After a pattern changes: its length, and the notes it draws (the first pass).
    void RefreshPatternPreview(_Inout_ Clip& clip);

    // Steps until every lane starts together again, at most limit. With conditions, a lane that
    // plays a step every 2nd pass counts as twice as long.
    uint64_t PatternCycleSteps(_In_ PatternSettings const& pattern, _In_ bool withConditions, _In_ uint64_t limit) noexcept;

    // An ordinary notes clip that plays what the pattern plays, for as many passes as it takes for
    // the lanes to line up again (at most maximumTicks). A step's chance stays on its note. The
    // pattern is left alone.
    Clip MakeNotesFromPattern(_In_ Clip const& pattern, _In_ int64_t maximumTicks);
}
