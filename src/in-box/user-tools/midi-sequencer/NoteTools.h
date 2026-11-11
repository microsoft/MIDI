// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

// The piano roll's note tools (design section 34): edits that act on the selected notes, or on
// the whole clip when nothing is selected. Timing (quantize with strength and swing, humanize,
// groove from another clip, time feel, ratchet), length (legato, fixed, percent), clean up
// (overlaps of the same note, duplicates, very short notes), pitch (transpose in the key, fit to
// the chords, voicing, voice leading, mirror, reverse), values (velocity, release velocity,
// chance) and selecting (top or bottom note of each chord, every Nth, on or off the beat, ...).
//
// Every tool is a pure function of the notes and its settings. It hands back the notes it took
// out and the notes it put in, which the window shows as a preview and then applies as one undo
// step. Anything random comes from a seed kept with the settings, so the same settings always
// give the same result. Free of pch.h and XAML like the rest of the model, so the unit tests use
// the same code.

#include <sal.h>

#include <array>
#include <cstdint>
#include <optional>
#include <vector>

#include "SequenceModel.h"

namespace midisequencer
{
    // What a tool changed.
    struct NoteToolResult
    {
        std::vector<Note> Removed{};
        std::vector<Note> Added{};

        bool Empty() const noexcept { return Removed.empty() && Added.empty(); }
    };

    // Where the notes are in the song, which the tools need for beats, milliseconds and harmony.
    struct NoteToolContext
    {
        std::vector<MeterChange> const* Meter{ nullptr };   // song ticks; 4/4 when absent
        TempoMap const* Tempo{ nullptr };                    // 120 BPM when absent
        int64_t ClipStart{ 0 };                              // where the clip plays in the song

        // The song's key, from the Key and chords row, and the chords under the clip, in clip
        // ticks. Empty when the song has no key.
        std::optional<std::array<bool, 12>> InKey{};

        struct Chord
        {
            int64_t Start{ 0 };
            int64_t End{ 0 };
            std::array<bool, 12> Tones{};
        };

        std::vector<Chord> Chords{};
    };

    // ---- curves ----

    // A shape from 0 to 1 that starts at 0 and ends at 1. Amount, from 0 to 1, is how strongly it
    // bends; 0 is a straight line for every shape.
    enum class CurveShape : uint8_t
    {
        Straight = 0,
        FastFirst = 1,      // rises quickly, then levels off
        SlowFirst = 2,      // rises slowly, then quickly
        SCurve = 3,         // slow, quick, slow
    };

    double ShapeCurve(_In_ CurveShape shape, _In_ double x, _In_ double amount) noexcept;

    // ---- chords ----

    // Notes that start within windowMs of the first note of their group, ordered by start, then
    // note number. A note on its own is a group of one.
    std::vector<std::vector<Note>> FindChords(_In_ std::vector<Note> const& notes, _In_ NoteToolContext const& context, _In_ double windowMs);

    // ---- timing ----

    struct QuantizeSettings
    {
        int64_t Grid{ 240 };
        double Strength{ 1.0 };             // 0 to 1
        double Swing{ 0.5 };                // 0.5 is straight, 0.75 is the most
        double Keep{ 0.0 };                 // 0 to 0.5 of the grid: notes this close stay put
        bool Ends{ false };                 // also put the ends on the grid
    };

    NoteToolResult Quantize(_In_ std::vector<Note> const& notes, _In_ NoteToolContext const& context, _In_ QuantizeSettings const& settings);

    struct HumanizeSettings
    {
        double TimingMs{ 10.0 };
        double VelocityPercent{ 8.0 };      // of the note's own velocity
        double LengthPercent{ 0.0 };
        bool TighterOnBeat{ true };
        uint32_t Seed{ 1 };
    };

    NoteToolResult Humanize(_In_ std::vector<Note> const& notes, _In_ NoteToolContext const& context, _In_ HumanizeSettings const& settings);

    // The feel of a clip: for each grid step in a bar, how far its notes sit from the grid and how
    // loud they are next to the clip's average.
    struct Groove
    {
        int64_t Grid{ 240 };
        std::vector<double> Offsets{};          // ticks, one for each step of a bar
        std::vector<double> Accents{};          // 1 is average
    };

    Groove ExtractGroove(_In_ std::vector<Note> const& source, _In_ NoteToolContext const& sourceContext, _In_ int64_t grid);

    struct GrooveSettings
    {
        double TimingStrength{ 1.0 };
        double VelocityStrength{ 0.5 };
    };

    NoteToolResult ApplyGroove(_In_ std::vector<Note> const& notes, _In_ NoteToolContext const& context, _In_ Groove const& groove, _In_ GrooveSettings const& settings);

    // Speeds up or slows down across the notes. The first start and the last end stay put.
    struct TimeFeelSettings
    {
        bool SpeedUp{ true };
        double Amount{ 0.5 };
    };

    NoteToolResult TimeFeel(_In_ std::vector<Note> const& notes, _In_ TimeFeelSettings const& settings);

    // Each note becomes several, inside its own length.
    struct RatchetSettings
    {
        uint32_t Count{ 4 };                    // 2 to 32
        CurveShape Spacing{ CurveShape::Straight };
        double SpacingAmount{ 0.5 };
        double EndVelocity{ 1.0 };              // the last repeat's velocity next to the first
        double Gate{ 0.5 };                     // how much of each repeat's space it sounds
    };

    NoteToolResult Ratchet(_In_ std::vector<Note> const& notes, _In_ RatchetSettings const& settings);

    // ---- length ----

    enum class LengthMode : uint8_t
    {
        Legato = 0,         // to the next start, less the gap
        Fixed = 1,
        Percent = 2,
    };

    struct LengthSettings
    {
        LengthMode Mode{ LengthMode::Legato };
        int64_t Ticks{ 240 };                   // the gap for Legato, the length for Fixed
        double Percent{ 50.0 };
    };

    NoteToolResult SetLengths(_In_ std::vector<Note> const& notes, _In_ LengthSettings const& settings);

    // ---- clean up ----

    struct CleanUpSettings
    {
        bool Overlaps{ true };          // a note shortened where the same note starts again
        bool Duplicates{ true };        // the same note at the same time: the loudest stays
        bool Short{ false };
        double ShortMs{ 15.0 };
    };

    NoteToolResult CleanUp(_In_ std::vector<Note> const& notes, _In_ NoteToolContext const& context, _In_ CleanUpSettings const& settings);

    // How many notes overlap a later note of the same number on the same channel.
    size_t CountSameNoteOverlaps(_In_ std::vector<Note> const& notes);

    // ---- pitch ----

    // Up or down by notes of the key. Notes outside the key move with the key note just below.
    // Without a key, by semitones.
    NoteToolResult TransposeInKey(_In_ std::vector<Note> const& notes, _In_ NoteToolContext const& context, _In_ int32_t steps);

    enum class FitWhich : uint8_t
    {
        All = 0,
        OnBeats = 1,
        Long = 2,           // a beat long or more
    };

    // Notes that aren't in the chord at their time move to its nearest note. Where there's no
    // chord, to the nearest note of the key.
    NoteToolResult FitToChords(_In_ std::vector<Note> const& notes, _In_ NoteToolContext const& context, _In_ FitWhich which);

    enum class VoicingKind : uint8_t
    {
        InversionUp = 0,
        InversionDown = 1,
        Close = 2,
        Open = 3,
        Drop2 = 4,
        Drop3 = 5,
        OctaveBelow = 6,
        SmoothLeading = 7,
    };

    NoteToolResult Voice(_In_ std::vector<Note> const& notes, _In_ NoteToolContext const& context, _In_ VoicingKind kind);

    // Upside down around a note. In the key, by notes of the key.
    NoteToolResult Mirror(_In_ std::vector<Note> const& notes, _In_ NoteToolContext const& context, _In_ uint8_t around, _In_ bool inKey);

    // Back to front, inside the time the notes take.
    NoteToolResult Reverse(_In_ std::vector<Note> const& notes);

    // ---- strum (the strum proposal; SequenceStrum.h has the shapes) ----

    enum class StrumDirection : uint8_t
    {
        Down = 0,           // lowest note first
        Up = 1,             // highest note first
        ByBeat = 2,         // down on the beat, up off it, the way a hand moves
        Random = 3,
    };

    struct StrumSettings
    {
        double SpreadMs{ 35.0 };            // first note to last
        bool InTicks{ false };              // so the spread changes with the tempo
        int64_t SpreadTicks{ 60 };
        StrumShape Shape{ StrumShape::SpeedingUp };
        double Bend{ 0.5 };                 // 0 to 1
        StrumDirection Direction{ StrumDirection::Down };
        StrumLands Lands{ StrumLands::First };
        bool KeepBass{ false };             // the lowest note stays on the beat
        double Tilt{ 0.0 };                 // -0.5 to 0.5: positive makes the first notes louder
        double Loose{ 0.0 };                // 0 to 1: small differences from strum to strum
        uint32_t Seed{ 1 };
    };

    // Spreads the notes of each chord (notes that start within 30 ms of each other) along the
    // shape. It starts from where the notes were before any earlier strum, so strumming again
    // replaces the last strum rather than adding to it. Every note's end stays where it was. Each
    // note keeps how far its start moved in StrumOffset, which Remove strum uses.
    NoteToolResult Strum(_In_ std::vector<Note> const& notes, _In_ NoteToolContext const& context, _In_ StrumSettings const& settings);

    // Puts strummed notes back where they were before the strum. Velocity changes stay.
    NoteToolResult RemoveStrum(_In_ std::vector<Note> const& notes);

    // How many of the notes a strum moved.
    size_t CountStrummed(_In_ std::vector<Note> const& notes) noexcept;

    // ---- velocity, release velocity and chance ----

    enum class ValueTarget : uint8_t
    {
        Velocity = 0,
        ReleaseVelocity = 1,
        Chance = 2,
    };

    enum class ValueMode : uint8_t
    {
        Set = 0,            // First
        Change = 1,         // plus or minus First percent
        Scale = 2,          // around the average, First percent (below 100 squeezes, above spreads)
        Ramp = 3,           // First to Second along the curve
        Accent = 4,         // First percent louder on beats, twice that on the first beat of a bar
        Random = 5,         // plus or minus First percent
    };

    struct ValueSettings
    {
        ValueTarget Target{ ValueTarget::Velocity };
        ValueMode Mode{ ValueMode::Set };
        double First{ 75.0 };               // percent
        double Second{ 100.0 };
        CurveShape Curve{ CurveShape::Straight };
        double CurveAmount{ 0.5 };
        uint32_t Seed{ 1 };
    };

    NoteToolResult SetValues(_In_ std::vector<Note> const& notes, _In_ NoteToolContext const& context, _In_ ValueSettings const& settings);

    // ---- selecting ----

    enum class SelectKind : uint8_t
    {
        TopOfChords = 0,
        BottomOfChords = 1,
        SameNotes = 2,          // every note with a number the selection has
        EveryNth = 3,
        OnBeats = 4,
        OffBeats = 5,
        VelocityRange = 6,
        OutsideKey = 7,
        Invert = 8,
    };

    struct SelectSettings
    {
        SelectKind Kind{ SelectKind::TopOfChords };
        uint32_t Every{ 2 };            // for EveryNth
        uint32_t Offset{ 0 };
        double Low{ 0.0 };              // percent, for VelocityRange
        double High{ 50.0 };
    };

    // Picks from the selection, or from the whole clip when the selection is empty. SameNotes and
    // Invert always look at the whole clip.
    std::vector<Note> SelectNotes(_In_ std::vector<Note> const& all, _In_ std::vector<Note> const& selection, _In_ NoteToolContext const& context, _In_ SelectSettings const& settings);
}
