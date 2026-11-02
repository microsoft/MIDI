// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#include "HarmonySuggestTests.h"

#include "Harmony.h"
#include "HarmonySuggest.h"

#include <algorithm>
#include <string>
#include <vector>

using namespace WEX::Common;
using namespace WEX::Logging;
using namespace WEX::TestExecution;
using namespace midisequencer;

namespace
{
    constexpr int64_t Bar = TicksPerQuarterNote * 4;

    constexpr uint8_t C = 0;
    constexpr uint8_t D = 2;
    constexpr uint8_t E = 4;
    constexpr uint8_t F = 5;
    constexpr uint8_t G = 7;
    constexpr uint8_t A = 9;
    constexpr uint8_t B = 11;

    ChordEvent Chord(uint8_t root, ChordType type, int64_t tick = 0, int64_t length = Bar)
    {
        ChordEvent chord{};
        chord.Tick = tick;
        chord.Length = length;
        chord.Root = root;
        chord.Type = type;
        return chord;
    }

    KeyChange Key(uint8_t root, ScaleKind scale, int64_t tick = 0)
    {
        return KeyChange{ tick, root, scale };
    }

    // "C Dm Em" style, for readable failures.
    std::wstring Names(std::vector<ChordSuggestion> const& list, KeyChange const& key)
    {
        std::wstring text{};

        for (auto const& s : list)
        {
            if (!text.empty())
            {
                text += L' ';
            }

            text += ChordName(Chord(s.Root, s.Type), &key);

            if (s.Borrowed)
            {
                text += L'*';
            }
        }

        return text;
    }

    std::wstring Names(std::vector<ChordEvent> const& list, KeyChange const& key)
    {
        std::wstring text{};

        for (auto const& chord : list)
        {
            if (!text.empty())
            {
                text += L' ';
            }

            text += ChordName(chord, &key);
        }

        return text;
    }

    std::vector<MeterChange> FourFour()
    {
        return { MeterChange{} };
    }
}

void HarmonySuggestTests::NextChordsInAMajorKey()
{
    auto const key = Key(C, ScaleKind::Major);

    auto const afterI = Chord(C, ChordType::Major);
    VERIFY_ARE_EQUAL(std::wstring{ L"F G Am Dm" }, Names(SuggestNextChords(&afterI, key, -1), key).substr(0, 9));

    auto const afterII = Chord(D, ChordType::Minor);
    VERIFY_ARE_EQUAL(std::wstring{ L"G Bdim F" }, Names(SuggestNextChords(&afterII, key, -1), key).substr(0, 8));

    auto const afterV = Chord(G, ChordType::Major);
    auto const v = SuggestNextChords(&afterV, key, -1);
    VERIFY_ARE_EQUAL(std::wstring{ L"C Am F" }, Names(v, key).substr(0, 6));
    VERIFY_IS_TRUE(v.size() >= 4 && v.size() <= 5);

    // Every suggestion in a major key is in the key.
    for (auto const& s : v)
    {
        VERIFY_IS_FALSE(s.Borrowed);
    }
}

void HarmonySuggestTests::NextChordsInAMinorKey()
{
    auto const key = Key(A, ScaleKind::NaturalMinor);

    auto const afterI = Chord(A, ChordType::Minor);
    auto const i = SuggestNextChords(&afterI, key, -1);
    VERIFY_ARE_EQUAL(std::wstring{ L"Dm F G E7* C" }, Names(i, key));

    // E7 has G♯, which natural minor doesn't. Harmonic minor has it.
    auto const harmonic = Key(A, ScaleKind::HarmonicMinor);
    auto const afterIv = Chord(D, ChordType::Minor);
    auto const iv = SuggestNextChords(&afterIv, harmonic, -1);
    VERIFY_ARE_EQUAL(E, iv.front().Root);
    VERIFY_IS_TRUE(iv.front().Type == ChordType::Seventh);
    VERIFY_IS_FALSE(iv.front().Borrowed);

    // Blues and pentatonic keys are judged against the scale they come from.
    auto const blues = Key(A, ScaleKind::Blues);
    for (auto const& s : SuggestNextChords(nullptr, blues, -1))
    {
        if (s.Root == A || s.Root == D || s.Root == F)
        {
            VERIFY_IS_FALSE(s.Borrowed);
        }
    }
}

void HarmonySuggestTests::StartingChords()
{
    auto const major = Key(D, ScaleKind::Major);
    auto const start = SuggestNextChords(nullptr, major, -1);
    VERIFY_ARE_EQUAL(static_cast<size_t>(5), start.size());
    VERIFY_ARE_EQUAL(D, start.front().Root);
    VERIFY_IS_TRUE(start.front().Type == ChordType::Major);

    auto const minor = Key(E, ScaleKind::Dorian);
    auto const minorStart = SuggestNextChords(nullptr, minor, -1);
    VERIFY_ARE_EQUAL(E, minorStart.front().Root);
    VERIFY_IS_TRUE(minorStart.front().Type == ChordType::Minor);

    // A chord from outside the key that isn't a dominant (D♯m in D): places to start.
    auto const outside = Chord(static_cast<uint8_t>(3), ChordType::Minor);
    VERIFY_ARE_EQUAL(D, SuggestNextChords(&outside, major, -1).front().Root);
}

void HarmonySuggestTests::PhraseEndsPreferHomeAndPullingBack()
{
    auto const key = Key(C, ScaleKind::Major);
    auto const afterVi = Chord(A, ChordType::Minor);

    // Bar 4 of a phrase (counted from 0: 3): V first, then I.
    auto const four = SuggestNextChords(&afterVi, key, 3);
    VERIFY_ARE_EQUAL(G, four[0].Root);
    VERIFY_ARE_EQUAL(C, four[1].Root);

    // Bar 8: I first, then V.
    auto const eight = SuggestNextChords(&afterVi, key, 7);
    VERIFY_ARE_EQUAL(C, eight[0].Root);
    VERIFY_ARE_EQUAL(G, eight[1].Root);

    // In a minor key, V is the borrowed V7.
    auto const minor = Key(A, ScaleKind::NaturalMinor);
    auto const afterVI = Chord(F, ChordType::Major);
    auto const minorFour = SuggestNextChords(&afterVI, minor, 11);
    VERIFY_ARE_EQUAL(E, minorFour[0].Root);
    VERIFY_IS_TRUE(minorFour[0].Type == ChordType::Seventh);

    // Never more than five.
    VERIFY_IS_TRUE(four.size() <= 5 && eight.size() <= 5);
}

void HarmonySuggestTests::SecondaryDominantsResolve()
{
    auto const key = Key(C, ScaleKind::Major);

    // D7 in C leads to G; A7 to Dm; B7 to Em; G7 home to C.
    auto const d7 = Chord(D, ChordType::Seventh);
    auto const fromD7 = SuggestNextChords(&d7, key, -1);
    VERIFY_ARE_EQUAL(G, fromD7.front().Root);

    auto const a7 = Chord(A, ChordType::Seventh);
    auto const fromA7 = SuggestNextChords(&a7, key, -1);
    VERIFY_ARE_EQUAL(D, fromA7.front().Root);
    VERIFY_IS_TRUE(fromA7.front().Type == ChordType::Minor);

    auto const b7 = Chord(B, ChordType::Seventh);
    VERIFY_ARE_EQUAL(E, SuggestNextChords(&b7, key, -1).front().Root);

    auto const g7 = Chord(G, ChordType::Seventh);
    VERIFY_ARE_EQUAL(C, SuggestNextChords(&g7, key, -1).front().Root);

    // In A minor, D7 leads to G.
    auto const minor = Key(A, ScaleKind::NaturalMinor);
    VERIFY_ARE_EQUAL(G, SuggestNextChords(&d7, minor, -1).front().Root);

    auto const e7 = Chord(E, ChordType::Seventh);
    auto const fromE7 = SuggestNextChords(&e7, key, -1);
    VERIFY_ARE_EQUAL(A, fromE7.front().Root);
    VERIFY_IS_TRUE(fromE7.front().Type == ChordType::Minor);

    // E♭7 (outside, a dominant) leads to A♭, which C major doesn't have a chord on: places to start.
    auto const eFlat7 = Chord(static_cast<uint8_t>(3), ChordType::Seventh);
    VERIFY_ARE_EQUAL(C, SuggestNextChords(&eFlat7, key, -1).front().Root);
}

void HarmonySuggestTests::SuggestionsAreDeterministicAndNeverRepeatTheChord()
{
    for (uint8_t keyRoot = 0; keyRoot < 12; ++keyRoot)
    {
        for (auto const scale : { ScaleKind::Major, ScaleKind::NaturalMinor, ScaleKind::Mixolydian, ScaleKind::Chromatic })
        {
            auto const key = Key(keyRoot, scale);

            for (uint8_t root = 0; root < 12; ++root)
            {
                for (auto const type : { ChordType::Major, ChordType::Minor, ChordType::Seventh, ChordType::Diminished })
                {
                    auto const previous = Chord(root, type);
                    auto const first = SuggestNextChords(&previous, key, 3);
                    auto const second = SuggestNextChords(&previous, key, 3);

                    VERIFY_IS_TRUE(first == second);
                    VERIFY_IS_TRUE(!first.empty() && first.size() <= 5);

                    for (size_t i = 0; i < first.size(); ++i)
                    {
                        VERIFY_IS_FALSE(first[i].Root == root && first[i].Type == type);

                        for (size_t j = i + 1; j < first.size(); ++j)
                        {
                            VERIFY_IS_FALSE(first[i] == first[j]);
                        }
                    }
                }
            }
        }
    }
}

void HarmonySuggestTests::MoreChordsAreBorrowed()
{
    auto const key = Key(C, ScaleKind::Major);
    auto const afterI = Chord(C, ChordType::Major);

    // Fm and B♭ from C minor, C7 (V7 of the first suggestion, F), and Gsus4.
    auto const more = MoreChordSuggestions(&afterI, key, -1);
    VERIFY_ARE_EQUAL(static_cast<size_t>(4), more.size());
    VERIFY_IS_TRUE((more[0] == ChordSuggestion{ F, ChordType::Minor, true }));
    VERIFY_IS_TRUE((more[1] == ChordSuggestion{ static_cast<uint8_t>(10), ChordType::Major, true }));
    VERIFY_IS_TRUE((more[2] == ChordSuggestion{ C, ChordType::Seventh, true }));
    VERIFY_IS_TRUE((more[3] == ChordSuggestion{ G, ChordType::Sus4, false }));

    auto const minor = Key(A, ScaleKind::NaturalMinor);
    auto const afterIm = Chord(A, ChordType::Minor);
    auto const minorMore = MoreChordSuggestions(&afterIm, minor, -1);
    VERIFY_ARE_EQUAL(D, minorMore[0].Root);
    VERIFY_IS_TRUE(minorMore[0].Type == ChordType::Major);
    VERIFY_IS_TRUE(minorMore[0].Borrowed);
    VERIFY_ARE_EQUAL(static_cast<uint8_t>(10), minorMore[1].Root);
}

void HarmonySuggestTests::ProgressionsInMajorAndMinor()
{
    auto const g = Key(G, ScaleKind::Major);
    VERIFY_ARE_EQUAL(std::wstring{ L"G D Em C" }, Names(ProgressionChords(ProgressionId::PopOneFiveSixFour, g), g));
    VERIFY_ARE_EQUAL(std::wstring{ L"Am7 D7 Gmaj7" }, Names(ProgressionChords(ProgressionId::JazzTwoFiveOne, g), g));

    auto const a = Key(A, ScaleKind::NaturalMinor);
    VERIFY_ARE_EQUAL(std::wstring{ L"Am G F E*" }, Names(ProgressionChords(ProgressionId::Andalusian, a), a));
    VERIFY_ARE_EQUAL(std::wstring{ L"Bm7\u266D5 E7* Am7" }, Names(ProgressionChords(ProgressionId::JazzTwoFiveOne, a), a));

    for (size_t i = 0; i < ProgressionCount; ++i)
    {
        auto const id = static_cast<ProgressionId>(i);
        VERIFY_IS_FALSE(ProgressionChords(id, g).empty());
        VERIFY_ARE_EQUAL(ProgressionChords(id, g).size(), ProgressionChords(id, a).size());
    }

    VERIFY_ARE_EQUAL(static_cast<size_t>(12), ProgressionChords(ProgressionId::TwelveBarBlues, g).size());
    VERIFY_IS_TRUE(ProgressionChords(static_cast<ProgressionId>(ProgressionCount), g).empty());
}

void HarmonySuggestTests::ProgressionLayoutFollowsTheRate()
{
    auto const key = Key(C, ScaleKind::Major);

    auto const perBar = LayOutProgression(ProgressionId::PopOneFiveSixFour, key, FourFour(), Bar * 2, ProgressionRate::OnePerBar);
    VERIFY_ARE_EQUAL(static_cast<size_t>(4), perBar.size());
    VERIFY_ARE_EQUAL(Bar * 2, perBar[0].Tick);
    VERIFY_ARE_EQUAL(Bar * 5, perBar[3].Tick);
    VERIFY_ARE_EQUAL(Bar, perBar[3].Length);

    auto const half = LayOutProgression(ProgressionId::PopOneFiveSixFour, key, FourFour(), 0, ProgressionRate::TwoPerBar);
    VERIFY_ARE_EQUAL(Bar / 2, half[1].Tick);
    VERIFY_ARE_EQUAL(Bar / 2, half[1].Length);

    auto const two = LayOutProgression(ProgressionId::PopOneFiveSixFour, key, FourFour(), 0, ProgressionRate::OnePerTwoBars);
    VERIFY_ARE_EQUAL(Bar * 6, two[3].Tick);
    VERIFY_ARE_EQUAL(Bar * 2, two[3].Length);

    // A change to 3/4 at bar 3: the third and fourth chords are 3/4 bars.
    std::vector<MeterChange> meter{ MeterChange{}, MeterChange{ Bar * 2, 3, 4 } };
    auto const changing = LayOutProgression(ProgressionId::PopOneFiveSixFour, key, meter, 0, ProgressionRate::OnePerBar);
    VERIFY_ARE_EQUAL(Bar * 2, changing[2].Tick);
    VERIFY_ARE_EQUAL(TicksPerQuarterNote * 3, changing[2].Length);
    VERIFY_ARE_EQUAL(Bar * 2 + TicksPerQuarterNote * 3, changing[3].Tick);
}

void HarmonySuggestTests::ProgressionLayoutFromMidBar()
{
    auto const key = Key(C, ScaleKind::Major);
    auto const start = Bar + TicksPerQuarterNote;

    auto const chords = LayOutProgression(ProgressionId::PopFifties, key, FourFour(), start, ProgressionRate::OnePerBar);
    VERIFY_ARE_EQUAL(start, chords[0].Tick);
    VERIFY_ARE_EQUAL(TicksPerQuarterNote * 3, chords[0].Length);
    VERIFY_ARE_EQUAL(Bar * 2, chords[1].Tick);
}

void HarmonySuggestTests::SuggestFillsUpToTheNextChord()
{
    auto const key = Key(C, ScaleKind::Major);

    HarmonyTrack harmony{};
    harmony.Keys.push_back(key);
    harmony.Chords.push_back(Chord(C, ChordType::Major, 0, Bar));
    harmony.Chords.push_back(Chord(F, ChordType::Major, Bar * 4, Bar));

    // From inside the first chord: after it, up to F.
    auto const fill = SuggestChordsToFill(harmony, FourFour(), TicksPerQuarterNote);
    VERIFY_ARE_EQUAL(static_cast<size_t>(3), fill.size());
    VERIFY_ARE_EQUAL(Bar, fill[0].Tick);
    VERIFY_ARE_EQUAL(Bar * 3 + Bar, fill[2].Tick + fill[2].Length);

    // Each is the first suggestion after the one before; the last bar of the phrase goes home.
    VERIFY_ARE_EQUAL(std::wstring{ L"F G C" }, Names(fill, key));

    // Nothing to fill when the chords already touch.
    harmony.Chords[1].Tick = Bar;
    VERIFY_IS_TRUE(SuggestChordsToFill(harmony, FourFour(), 0).empty() || SuggestChordsToFill(harmony, FourFour(), 0).front().Tick >= Bar * 2);
}

void HarmonySuggestTests::SuggestFillsToThePhraseEnd()
{
    auto const key = Key(A, ScaleKind::NaturalMinor);

    HarmonyTrack harmony{};
    harmony.Keys.push_back(key);
    harmony.Chords.push_back(Chord(A, ChordType::Minor, 0, Bar));

    auto const fill = SuggestChordsToFill(harmony, FourFour(), Bar);
    VERIFY_ARE_EQUAL(static_cast<size_t>(3), fill.size());
    VERIFY_ARE_EQUAL(Bar * 4, fill.back().Tick + fill.back().Length);

    // From bar 5, to the end of bar 8.
    auto const later = SuggestChordsToFill(harmony, FourFour(), Bar * 4);
    VERIFY_ARE_EQUAL(static_cast<size_t>(4), later.size());
    VERIFY_ARE_EQUAL(Bar * 8, later.back().Tick + later.back().Length);
}

void HarmonySuggestTests::SuggestNeedsAKey()
{
    HarmonyTrack harmony{};
    harmony.Chords.push_back(Chord(C, ChordType::Major, 0, Bar));
    VERIFY_IS_TRUE(SuggestChordsToFill(harmony, FourFour(), Bar).empty());

    harmony.Keys.push_back(Key(C, ScaleKind::Major));
    VERIFY_IS_FALSE(SuggestChordsToFill(harmony, FourFour(), Bar).empty());
}
