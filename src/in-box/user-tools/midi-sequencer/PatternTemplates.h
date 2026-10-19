// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

// Starter patterns (design section 27): the kind of presets a drum machine comes with, from the
// classics to newer styles, and common melodic lines such as acid basslines. Each is written as
// text so it's easy to read and to change. A template either becomes the pattern or goes on top
// of the steps the pattern already has.
// Free of pch.h and XAML like the rest of the model, so the unit tests use the same code.

#include <sal.h>

#include <cstdint>
#include <span>
#include <string>
#include <string_view>

#include "SequenceModel.h"

namespace midisequencer
{
    // How the Templates menu groups them.
    enum class PatternTemplateGroup : uint8_t
    {
        DrumMachine = 0,    // drum machine and band staples
        Electronic = 1,     // house, techno and their relatives
        HipHop = 2,
        World = 3,          // Latin, Caribbean and African
        Basslines = 4,
        Sequences = 5,      // arpeggios and sequencer lines
    };

    struct DrumTemplateLane
    {
        uint8_t Note{ 36 };

        // One character a step: "." rests, "x" plays, "X" plays accented, "o" plays softly, "g"
        // is a ghost note, "?" plays about half the time, and "2" to "8" plays that many quick
        // hits in the step (a ratchet).
        std::wstring_view Steps{};
    };

    struct PatternTemplate
    {
        // Stable. The template's resource names end with it.
        std::wstring_view Id{};

        PatternMode Mode{ PatternMode::Drums };
        PatternTemplateGroup Group{ PatternTemplateGroup::DrumMachine };

        // What the style is usually played at. The menu says so; the sequence's tempo stays.
        uint16_t BeatsPerMinute{ 120 };

        int64_t StepTicks{ TicksPerQuarterNote / 4 };
        uint8_t Swing{ 50 };

        // Drums: a lane for each drum, all the same length.
        std::span<DrumTemplateLane const> Lanes{};

        // Melodic: a word a step, with spaces between. "." rests and "-" holds the note before it
        // through one more step. Anything else is semitones from the root ("0", "7", "-5"),
        // followed by "!" for an accent or "~" to slide into the next note.
        std::wstring_view Melody{};

        // Melodic: where C is for the root, as a note number (36 is C1, with C3 = 60), and how
        // long a note is, as a percent of a step.
        uint8_t RootOctaveNote{ 36 };
        uint16_t NoteLength{ 50 };
    };

    // Every template, the drum ones first, in the order the menu lists them.
    std::span<PatternTemplate const> PatternTemplates() noexcept;

    PatternTemplate const* FindPatternTemplate(_In_ std::wstring_view id) noexcept;

    // A melodic template's root: the key's note (0 is C, 11 is B) in the octave nearest the
    // template's C.
    uint8_t TemplateRootNote(_In_ PatternTemplate const& source, _In_ uint8_t key) noexcept;

    // The pattern a template makes, in a key for a melodic one. The channel is the one the
    // pattern had, because a template doesn't know which instrument plays it.
    PatternSettings MakePatternFromTemplate(_In_ PatternTemplate const& source, _In_ uint8_t key, _In_ uint8_t channel);

    // Whether any step is on. Using a template on such a pattern asks whether to replace them.
    bool PatternHasSteps(_In_ PatternSettings const& pattern) noexcept;

    // Puts another pattern's steps on top of a pattern's, for a template used without replacing.
    // Steps keep their time: they move to the pattern's step size, to the nearest step. Where
    // both have a step, the one put on top wins. A drum lane goes onto the lane with the same
    // note, or becomes a new lane. The pattern gets longer when what goes on top is longer, up to
    // 64 steps. Its step size, swing and channel stay as they were.
    void OverlayPattern(_Inout_ PatternSettings& pattern, _In_ PatternSettings const& added);
}
