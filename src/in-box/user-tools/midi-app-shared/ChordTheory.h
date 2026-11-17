// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

// Chords and keys the way the Windows MIDI tools name them: chord names spelled in a key, Roman
// numerals, the notes a chord plays, and the chords that fit a set of notes. Plain C++ with no
// WinRT, so the unit tests compile it unchanged.
//
// Moved here from the Windows MIDI Sequencer's Key and chords row (Harmony.cpp), so the Sequencer
// and the Windows MIDI Calculator spell, read and voice a chord the same way. A chord typed in one
// can be typed in the other.

#include <sal.h>

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace midiapp
{
    // The value is saved in Sequencer files, so only add.
    enum class ScaleKind : uint8_t
    {
        Major = 0,
        NaturalMinor = 1,
        Dorian = 2,
        Phrygian = 3,
        Lydian = 4,
        Mixolydian = 5,
        HarmonicMinor = 6,
        MajorPentatonic = 7,
        MinorPentatonic = 8,
        Blues = 9,
        Chromatic = 10,
    };

    inline constexpr size_t ScaleKindCount = static_cast<size_t>(ScaleKind::Chromatic) + 1;

    // The kinds of chord the tools know. A chord's name is its root, spelled in the key, then the
    // suffix for its type. The value is saved in Sequencer files, so only add.
    enum class ChordType : uint8_t
    {
        Major = 0,          // C
        Minor = 1,          // Cm
        Seventh = 2,        // C7
        Major7 = 3,         // Cmaj7
        Minor7 = 4,         // Cm7
        Diminished = 5,     // Cdim
        Sus4 = 6,           // Csus4
        Sus2 = 7,           // Csus2
        Sixth = 8,          // C6
        Minor6 = 9,         // Cm6
        Ninth = 10,         // C9
        Major9 = 11,        // Cmaj9
        Minor9 = 12,        // Cm9
        Add9 = 13,          // Cadd9
        Augmented = 14,     // Caug
        Diminished7 = 15,   // Cdim7
        HalfDiminished = 16, // Cm7♭5
        SeventhSus4 = 17,   // C7sus4
        Eleventh = 18,      // C11
        Thirteenth = 19,    // C13
    };

    inline constexpr size_t ChordTypeCount = static_cast<size_t>(ChordType::Thirteenth) + 1;

    // A chord without a place in time: a root, a type, and a bass when it isn't the root (C/E).
    struct ChordSymbol
    {
        uint8_t Root{ 0 };                      // 0 is C
        ChordType Type{ ChordType::Major };
        std::optional<uint8_t> Bass{};

        bool operator==(ChordSymbol const&) const = default;
    };

    struct MusicKey
    {
        uint8_t Root{ 0 };                      // 0 is C
        ScaleKind Scale{ ScaleKind::Major };

        bool operator==(MusicKey const&) const = default;
    };

    // ---- names ----

    // Whether a key is written with flats (F major, B♭ major, D minor...) rather than sharps.
    bool KeyUsesFlats(_In_ uint8_t root, _In_ ScaleKind scale) noexcept;

    // The root of the major key with the same notes: C for A natural minor, C for D Dorian.
    uint8_t ParentMajorRoot(_In_ MusicKey const& key) noexcept;

    // A note's name, 0 to 11, with ♯ or ♭ (U+266F, U+266D): "C", "C♯", "D♭".
    std::wstring NoteName(_In_ uint8_t pitchClass, _In_ bool flats);

    // A note spelled in a key: a note outside the key is spelled the way its numeral is, so ♭II
    // in A minor is B♭ and the raised seventh is G♯. With no key, E♭, A♭ and B♭ are flats and
    // F♯ and C♯ sharps.
    std::wstring NoteNameInKey(_In_ uint8_t pitchClass, _In_opt_ MusicKey const* key);

    // What follows the root in a chord's name: "" for major, "m", "7", "maj7", "m7", "dim", "sus4",
    // "sus2", "6", "m6", "9", "maj9", "m9", "add9", "aug", "dim7", "m7♭5", "7sus4", "11", "13".
    std::wstring ChordTypeSuffix(_In_ ChordType type);

    // "Dm7", "C/E", "B♭maj7", spelled in the key, or as NoteNameInKey spells it with no key.
    std::wstring ChordName(_In_ ChordSymbol const& chord, _In_opt_ MusicKey const* key);

    // A chord typed or pasted as text: a root (A to G, then ♯, ♭, # or b), a suffix as above
    // (also "maj" for major, "min" or "-" for minor, "M7" for maj7, "°" for dim, "ø" or "m7b5" for
    // m7♭5, "+" for aug), then optionally "/" and a bass note. "N.C." is no chord. Spaces around
    // it are fine.
    struct ParsedChordSymbol
    {
        bool NoChord{ false };
        ChordSymbol Chord{};
    };

    std::optional<ParsedChordSymbol> ParseChordName(_In_ std::wstring_view text);

    // A note name on its own, as the bass of a chord is written: "G", "B♭", "F#".
    std::optional<uint8_t> ParseNoteName(_In_ std::wstring_view text) noexcept;

    // The chord's place in the key: "I", "ii7", "V7", "vii°", "♭VII", "iv". Upper case for a chord
    // with a major third, lower case for a minor one; "°" for diminished, "ø7" for m7♭5, "+" for
    // augmented, then the rest of the suffix ("7", "maj7", "sus4"). A root outside the scale gets
    // ♭ or ♯ before the numeral. A bass other than the root adds "/" and the bass's numeral.
    std::wstring RomanNumeral(_In_ ChordSymbol const& chord, _In_ MusicKey const& key);

    // True when every note of the chord is in the key's scale. Chromatic counts everything in.
    bool ChordIsInKey(_In_ ChordSymbol const& chord, _In_ MusicKey const& key);

    // The id each scale has in the Sequencer's file and its AI assistant's format: "major",
    // "naturalMinor", "dorian", "phrygian", "lydian", "mixolydian", "harmonicMinor",
    // "majorPentatonic", "minorPentatonic", "blues", "chromatic".
    std::wstring_view ScaleId(_In_ ScaleKind scale) noexcept;
    std::optional<ScaleKind> ScaleFromId(_In_ std::wstring_view id) noexcept;

    // ---- notes ----

    // Semitones above the root of each note the chord stacks, root first: { 0, 3, 7, 10 } for m7.
    std::vector<uint8_t> ChordIntervals(_In_ ChordType type);

    // The pitch classes of a chord, root first, in the order they're stacked.
    std::vector<uint8_t> ChordPitchClasses(_In_ ChordSymbol const& chord);

    // The notes a chord plays when it's heard or turned into a clip: its notes in close position
    // with the root from middle C (60) up to B (71), then the bass (the root, or the chord's own
    // bass) an octave below, in 48 to 59. Sorted low to high, no repeats.
    std::vector<uint8_t> ChordNotes(_In_ ChordSymbol const& chord);

    // A sensible first chord on a key's root: minor for minor-sounding scales (natural and
    // harmonic minor, dorian, phrygian, minor pentatonic, blues), major for the rest.
    ChordType TonicChordType(_In_ ScaleKind scale) noexcept;

    // The pitch classes of a key's scale, from its root up.
    std::vector<uint8_t> ScalePitchClasses(_In_ MusicKey const& key);

    // A chord built on each degree of the key, stacking every other scale note: triads, or with
    // sevenths a four-note chord where one of the known types fits (harmonic minor's i and III
    // have none, so they stay triads). Pentatonic, blues and chromatic keys use the major or
    // natural minor scale they sound like.
    std::vector<ChordSymbol> ChordsInKey(_In_ MusicKey const& key, _In_ bool sevenths);

    // Every name that fits a set of notes (MIDI note numbers, any order, repeats fine), best
    // first: the chords whose notes are exactly the notes' pitch classes, with the lowest note
    // as the bass. A chord with the lowest note as its root comes before one written over a
    // bass, then fewer notes before more, then the order of ChordType. Nothing for fewer than
    // two different pitch classes.
    std::vector<ChordSymbol> NameChords(_In_ std::vector<uint8_t> const& notes);
}
