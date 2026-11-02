// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

// The Key and chords row (Pete's proposal of November 1 2026, revision 2): chord names spelled in
// the key, Roman numerals, the notes a chord plays, keeping the row in order, its undo step, and
// the sequence file. No XAML here, so the tests compile it unchanged.

#include <sal.h>

#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "SequenceModel.h"
#include "SequenceUndo.h"

namespace midisequencer
{
    // Keys and chords a sequence can hold, each.
    inline constexpr size_t MaximumHarmonyEvents = 100000;

    // ---- names ----

    // Whether a key is written with flats (F major, B♭ major, D minor...) rather than sharps.
    bool KeyUsesFlats(_In_ uint8_t root, _In_ ScaleKind scale) noexcept;

    // A Standard MIDI File key signature (FF 59): sharps (positive) or flats (negative), -7 to 7,
    // and major or minor. That event only knows major and minor, so a mode becomes the key
    // signature it shares (D Dorian is no sharps or flats), marked minor when the mode sounds
    // minor. Reading one back gives a major key or a natural minor one.
    struct KeySignature
    {
        int8_t Accidentals{ 0 };
        bool Minor{ false };
    };

    KeySignature KeySignatureOf(_In_ KeyChange const& key) noexcept;
    KeyChange KeyFromSignature(_In_ KeySignature signature, _In_ int64_t tick) noexcept;

    // A note's name, 0 to 11, with ♯ or ♭ (U+266F, U+266D): "C", "C♯", "D♭".
    std::wstring NoteName(_In_ uint8_t pitchClass, _In_ bool flats);

    // What follows the root in a chord's name: "" for major, "m", "7", "maj7", "m7", "dim", "sus4",
    // "sus2", "6", "m6", "9", "maj9", "m9", "add9", "aug", "dim7", "m7♭5", "7sus4", "11", "13".
    std::wstring ChordTypeSuffix(_In_ ChordType type);

    // "Dm7", "C/E", "B♭maj7", spelled in the key, or with flats for E♭, A♭ and B♭ and sharps
    // for F♯ and C♯ when there's no key.
    std::wstring ChordName(_In_ ChordEvent const& chord, _In_opt_ KeyChange const* key);

    // A chord typed or pasted as text: a root (A to G, then ♯, ♭, # or b), a suffix as above
    // (also "maj" for major, "min" or "-" for minor, "M7" for maj7, "°" for dim, "ø" or "m7b5" for
    // m7♭5, "+" for aug), then optionally "/" and a bass note. "N.C." is no chord. Spaces around
    // it are fine. Tick and length are left at their defaults.
    struct ParsedChord
    {
        bool NoChord{ false };
        ChordEvent Chord{};
    };

    std::optional<ParsedChord> ParseChordName(_In_ std::wstring_view text);

    // The chord's place in the key: "I", "ii7", "V7", "vii°", "♭VII", "iv". Upper case for a chord
    // with a major third, lower case for a minor one; "°" for diminished, "ø7" for m7♭5, "+" for
    // augmented, then the rest of the suffix ("7", "maj7", "sus4"). A root outside the scale gets
    // ♭ or ♯ before the numeral. A bass other than the root adds "/" and the bass's numeral
    // degree, which the UI may leave off when space is short.
    std::wstring RomanNumeral(_In_ ChordEvent const& chord, _In_ KeyChange const& key);

    // True when every note of the chord is in the key's scale. Chromatic counts everything in.
    bool ChordIsInKey(_In_ ChordEvent const& chord, _In_ KeyChange const& key);

    // The id each scale has in the sequence file and the AI assistant's format: "major",
    // "naturalMinor", "dorian", "phrygian", "lydian", "mixolydian", "harmonicMinor",
    // "majorPentatonic", "minorPentatonic", "blues", "chromatic".
    std::wstring_view ScaleId(_In_ ScaleKind scale) noexcept;
    std::optional<ScaleKind> ScaleFromId(_In_ std::wstring_view id) noexcept;

    // ---- notes ----

    // The pitch classes of a chord, root first, in the order they're stacked.
    std::vector<uint8_t> ChordPitchClasses(_In_ ChordEvent const& chord);

    // The notes a chord plays when it's heard or turned into a clip: its notes in close position
    // with the root from middle C (60) up to B (71), then the bass (the root, or the chord's own
    // bass) an octave below, in 48 to 59. Sorted low to high, no repeats.
    std::vector<uint8_t> ChordNotes(_In_ ChordEvent const& chord);

    // A sensible first chord on a key's root: minor for minor-sounding scales (natural and
    // harmonic minor, dorian, phrygian, minor pentatonic, blues), major for the rest.
    ChordType TonicChordType(_In_ ScaleKind scale) noexcept;

    // ---- the row ----

    // The key at a tick: the last that starts at or before it, or the first when none does.
    // Nothing when there are no keys.
    KeyChange const* KeyAt(_In_ HarmonyTrack const& harmony, _In_ int64_t tick) noexcept;

    // The chord sounding at a tick, if any.
    ChordEvent const* ChordAt(_In_ HarmonyTrack const& harmony, _In_ int64_t tick) noexcept;

    // Sorted, ticks and lengths in range (a length is at least 1), roots and basses 0 to 11, a
    // bass equal to the root dropped, two keys at one tick become the later one, and chords
    // never overlap (an earlier chord is cut short where a later one starts). Nothing over
    // MaximumHarmonyEvents.
    void NormalizeHarmony(_Inout_ HarmonyTrack& harmony);

    // Puts a chord on the row. Chords it covers are cut short, split around it, or taken out,
    // so the row stays without overlaps. Returns its index after.
    size_t PlaceChord(_Inout_ HarmonyTrack& harmony, _In_ ChordEvent chord);

    // Moves the roots and basses of the chords that start in [fromTick, toTick) by semitones.
    void TransposeChords(_Inout_ HarmonyTrack& harmony, _In_ int64_t fromTick, _In_ int64_t toTick, _In_ int semitones);

    std::unique_ptr<SequenceChange> MakeHarmonyChange(
        _In_ std::optional<HarmonyTrack> before,
        _In_ std::optional<HarmonyTrack> after);

    // ---- the sequence file ----

    inline constexpr wchar_t HarmonyKey[] = L"keyAndChords";

    // The value of "keyAndChords", indented to sit in the sequence file's top-level object:
    // { "hidden": false, "keys": [ { "tick": 0, "root": "A", "scale": "naturalMinor" } ],
    //   "chords": [ { "tick": 0, "length": 3840, "chord": "Am" } ] }
    // Roots and chords are written as names, spelled in the key that holds them.
    std::wstring HarmonyToJsonText(_In_ HarmonyTrack const& harmony);

    // The "keyAndChords" value of a sequence file, if it has one. Anything malformed or over the
    // limits is counted in skipped and left out. Ticks are as written; the caller scales them.
    std::optional<HarmonyTrack> ReadHarmonyJson(
        _In_ winrt::Windows::Data::Json::JsonObject const& root,
        _Inout_ size_t& skipped);
}
