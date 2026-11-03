// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

// Help picking chords on the Key and chords row (proposal revision 2, section 2): what could come
// next, a few more colorful choices, and ready-made progressions. Fixed music theory rules, not AI,
// so the same chord always gets the same suggestions. No XAML here, so the tests compile it
// unchanged.

#include <sal.h>

#include <cstdint>
#include <optional>
#include <span>
#include <string_view>
#include <vector>

#include "SequenceModel.h"

namespace midisequencer
{
    // ---- suggestions ----

    struct ChordSuggestion
    {
        uint8_t Root{ 0 };
        ChordType Type{ ChordType::Major };

        // Has notes from outside the key. The picker marks these with a star.
        bool Borrowed{ false };

        bool operator==(ChordSuggestion const&) const = default;
    };

    // What's likely to come after a chord in a key, most likely first, four or five of them, never
    // the chord itself. With no chord before (the start of a song, or after "N.C."), places to
    // start. Each chord in the key has a job: home (I, vi), moving away (ii, IV) and pulling back
    // (V, vii°), and the next chord is usually the next job along. On the last bar of a four-bar
    // phrase V moves to the front, and on the last bar of an eight-bar phrase I does.
    //
    // phraseBar is the new chord's bar, counted from 0; pass a negative number to leave phrases out.
    std::vector<ChordSuggestion> SuggestNextChords(
        _In_opt_ ChordEvent const* previous,
        _In_ KeyChange const& key,
        _In_ int64_t phraseBar);

    // A few choices with more color: chords borrowed from the parallel key (iv and ♭VII in a
    // major key; IV and ♭II in a minor one), the secondary dominant that leads to the first
    // suggestion (V7 of it), and a sus4 on the key's V.
    std::vector<ChordSuggestion> MoreChordSuggestions(
        _In_opt_ ChordEvent const* previous,
        _In_ KeyChange const& key,
        _In_ int64_t phraseBar);

    // ---- progressions ----

    enum class ProgressionId : uint8_t
    {
        PopOneFiveSixFour = 0,      // I–V–vi–IV
        PopSixFourOneFive,          // vi–IV–I–V
        PopFifties,                 // I–vi–IV–V
        JazzTwoFiveOne,             // ii7–V7–Imaj7
        JazzTurnaround,             // Imaj7–vi7–ii7–V7
        TwelveBarBlues,             // I I I I IV IV I I V IV I V
        TwelveBarBluesSevenths,     // the same, all 7ths
        MinorFall,                  // i–VI–III–VII
        MinorOneFourFive,           // i–iv–v
        Andalusian,                 // i–VII–VI–V
        Canon,                      // I–V–vi–iii–IV–I–IV–V
        CircleOfFifths,             // I–IV–vii°–iii–vi–ii–V–I (i–iv–VII–III–VI–ii°–V–i in minor)
    };

    inline constexpr size_t ProgressionCount = static_cast<size_t>(ProgressionId::CircleOfFifths) + 1;

    // How many chords go in each bar when a progression fills the row.
    enum class ProgressionRate : uint8_t
    {
        TwoPerBar = 0,
        OnePerBar,
        OnePerTwoBars,
    };

    // The chords of a progression in a key, in order. Each is one step of the rate long.
    std::vector<ChordSuggestion> ProgressionChords(_In_ ProgressionId id, _In_ KeyChange const& key);

    // A progression's chords on the row from a tick, each one step of the rate long: the chords
    // to place, with their ticks and lengths. The first starts at fromTick even when that isn't
    // on a bar line, and is shortened so the rest land on bar lines (or half bars).
    std::vector<ChordEvent> LayOutProgression(
        _In_ ProgressionId id,
        _In_ KeyChange const& key,
        _In_ std::vector<MeterChange> const& meter,
        _In_ int64_t fromTick,
        _In_ ProgressionRate rate);

    // Suggest on the row header: from fromTick to the next chord on the row (or to the end of
    // the four-bar phrase, when there's none after), one chord per bar, each the first suggestion
    // after the one before. Nothing when there's no room or no key.
    std::vector<ChordEvent> SuggestChordsToFill(
        _In_ HarmonyTrack const& harmony,
        _In_ std::vector<MeterChange> const& meter,
        _In_ int64_t fromTick);
}
