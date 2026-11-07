// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

// Glide and vibrato for MIDI 2.0 instruments (design section 36), as MIDI 2.0 Per-Note Pitch Bend
// messages in a clip's events: each note bends on its own, so a chord's other notes stay put.
//
// From M2-104-UM v1.1.2:
// - 7.4.12: Per-Note Pitch Bend is message type 0x4, status 0x6, the channel in the low 4 bits,
//   the note number in byte 3 and byte 4 reserved. The second word is an unsigned bipolar value
//   centered at 0x80000000.
// - 7.4.13: the bend's range, up and down, is Registered Controller bank 0, index 7, shared by
//   every note on the channel, "a 7.25 fixed-point unsigned value that specifies an interval in
//   units of 100 Cents". This code writes it that way: semitones in the top 7 bits. (The example
//   in 7.4.13.3 gives 0x0C000000 as 1200 cents, which would be 8 integer bits. The definition is
//   followed, and sending the range is off unless asked for.)
// - Appendix D: the default translation to MIDI 1.0 doesn't carry Per-Note Pitch Bend, so a MIDI
//   1.0 instrument doesn't hear these.
//
// Free of pch.h and XAML like the rest of the model, so the unit tests use the same code.

#include <sal.h>

#include <cstdint>
#include <optional>
#include <vector>

#include "NoteTools.h"
#include "SequenceModel.h"

namespace midisequencer
{
    inline constexpr uint32_t PitchBendCenter = 0x80000000u;

    // A Per-Note Pitch Bend message. Group 0: the destination decides the group.
    ClipEvent PerNotePitchBend(_In_ int64_t tick, _In_ uint8_t channel, _In_ uint8_t note, _In_ uint32_t value) noexcept;

    // The Registered Controller that sets the bend's range on a channel, in semitones.
    ClipEvent PerNoteBendRange(_In_ int64_t tick, _In_ uint8_t channel, _In_ double semitones) noexcept;

    // The note a Per-Note Pitch Bend message bends, on its channel; nothing for any other message.
    struct BentNote
    {
        uint8_t Channel{ 0 };
        uint8_t Number{ 0 };
    };

    std::optional<BentNote> PerNotePitchBendOf(_In_ ClipEvent const& event) noexcept;

    // The message's value for a bend of so many semitones on an instrument whose range is range
    // semitones each way, held inside the range.
    uint32_t BendValue(_In_ double semitones, _In_ double range) noexcept;

    struct GlideSettings
    {
        double TimeMs{ 90.0 };              // how long a slide into a note takes
        bool OnlyJoined{ true };            // only between notes that touch or overlap, as legato
        double RangeSemitones{ 12.0 };      // the instrument's per-note bend range
        bool SendRange{ false };            // send the range at the clip's start
    };

    struct VibratoSettings
    {
        double DepthCents{ 30.0 };          // each way
        double RateHz{ 5.5 };
        double DelayMs{ 200.0 };            // from the note's start
        double FadeInMs{ 300.0 };
        double ShortestMs{ 300.0 };         // shorter notes get none
        double RangeSemitones{ 2.0 };
        bool SendRange{ false };
    };

    // A clip's events with the change made: the old bends on the notes it touches go, and new ones
    // come in, kept in order by tick.
    struct ExpressionResult
    {
        std::vector<ClipEvent> Events{};
        size_t Added{ 0 };
        size_t Removed{ 0 };
    };

    // Each note slides in from the note before it (the notes in time order, a melody), starting
    // at the note before's pitch and arriving at its own over the glide time. Steps the range
    // can't reach aren't slid.
    ExpressionResult Glide(_In_ std::vector<ClipEvent> const& events, _In_ std::vector<Note> const& notes, _In_ NoteToolContext const& context, _In_ GlideSettings const& settings);

    // A sine wave of bend on each note long enough, after a delay, fading in.
    ExpressionResult Vibrato(_In_ std::vector<ClipEvent> const& events, _In_ std::vector<Note> const& notes, _In_ NoteToolContext const& context, _In_ VibratoSettings const& settings);

    // Takes away the Per-Note Pitch Bend on the notes, from just before each starts to its end.
    ExpressionResult RemovePerNoteBend(_In_ std::vector<ClipEvent> const& events, _In_ std::vector<Note> const& notes);
}
