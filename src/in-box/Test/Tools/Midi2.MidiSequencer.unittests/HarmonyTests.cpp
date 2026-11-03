// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#include "HarmonyTests.h"

#include "Harmony.h"
#include "SequenceSerializer.h"

#include <algorithm>
#include <string>
#include <string_view>
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

    std::wstring const SharpSign{ L"\u266F" };
    std::wstring const FlatSign{ L"\u266D" };

    constexpr size_t TypeCount = static_cast<size_t>(ChordType::Thirteenth) + 1;

    ChordEvent Chord(uint8_t root, ChordType type, std::optional<uint8_t> bass = std::nullopt, int64_t tick = 0, int64_t length = Bar)
    {
        ChordEvent chord{};
        chord.Tick = tick;
        chord.Length = length;
        chord.Root = root;
        chord.Type = type;
        chord.Bass = bass;
        return chord;
    }

    ChordEvent Chord(uint8_t root, ChordType type, int bass, int64_t tick = 0, int64_t length = Bar)
    {
        return Chord(root, type, std::optional<uint8_t>{ static_cast<uint8_t>(bass) }, tick, length);
    }

    KeyChange Key(uint8_t root, ScaleKind scale, int64_t tick = 0)
    {
        return KeyChange{ tick, root, scale };
    }

    void VerifyParsed(std::wstring_view text, uint8_t root, ChordType type, std::optional<uint8_t> bass = std::nullopt)
    {
        auto const parsed = ParseChordName(text);
        VERIFY_IS_TRUE(parsed.has_value(), std::wstring{ text }.c_str());
        VERIFY_IS_FALSE(parsed->NoChord);
        VERIFY_ARE_EQUAL(root, parsed->Chord.Root, std::wstring{ text }.c_str());
        VERIFY_IS_TRUE(type == parsed->Chord.Type, std::wstring{ text }.c_str());
        VERIFY_IS_TRUE(bass == parsed->Chord.Bass, std::wstring{ text }.c_str());
    }
}

void HarmonyTests::NamesAreSpelledInTheKey()
{
    VERIFY_ARE_EQUAL(std::wstring{ L"C" }, NoteName(0, true));
    VERIFY_ARE_EQUAL(L"B" + FlatSign, NoteName(10, true));
    VERIFY_ARE_EQUAL(L"A" + SharpSign, NoteName(10, false));
    VERIFY_ARE_EQUAL(L"C" + SharpSign, NoteName(13, false));

    // Flats when the major key with the same notes is F, B♭, E♭, A♭, D♭ or G♭.
    VERIFY_IS_TRUE(KeyUsesFlats(F, ScaleKind::Major));
    VERIFY_IS_TRUE(KeyUsesFlats(3, ScaleKind::Major));
    VERIFY_IS_TRUE(KeyUsesFlats(D, ScaleKind::NaturalMinor));
    VERIFY_IS_TRUE(KeyUsesFlats(G, ScaleKind::Dorian));
    VERIFY_IS_TRUE(KeyUsesFlats(C, ScaleKind::Mixolydian));
    VERIFY_IS_TRUE(KeyUsesFlats(C, ScaleKind::Blues));
    VERIFY_IS_FALSE(KeyUsesFlats(C, ScaleKind::Major));
    VERIFY_IS_FALSE(KeyUsesFlats(B, ScaleKind::Major));
    VERIFY_IS_FALSE(KeyUsesFlats(A, ScaleKind::NaturalMinor));
    VERIFY_IS_FALSE(KeyUsesFlats(E, ScaleKind::Phrygian));
    VERIFY_IS_FALSE(KeyUsesFlats(F, ScaleKind::Lydian));
    VERIFY_IS_FALSE(KeyUsesFlats(C, ScaleKind::Chromatic));

    auto const fMajor = Key(F, ScaleKind::Major);
    auto const bMajor = Key(B, ScaleKind::Major);

    VERIFY_ARE_EQUAL(L"B" + FlatSign, ChordName(Chord(10, ChordType::Major), &fMajor));
    VERIFY_ARE_EQUAL(L"A" + SharpSign, ChordName(Chord(10, ChordType::Major), &bMajor));
    VERIFY_ARE_EQUAL(L"B" + FlatSign + L"maj7", ChordName(Chord(10, ChordType::Major7), &fMajor));
    VERIFY_ARE_EQUAL(L"D" + SharpSign + L"m7", ChordName(Chord(3, ChordType::Minor7), &bMajor));
    VERIFY_ARE_EQUAL(std::wstring{ L"Dm7" }, ChordName(Chord(D, ChordType::Minor7), &fMajor));
    VERIFY_ARE_EQUAL(L"C7/B" + FlatSign, ChordName(Chord(C, ChordType::Seventh, 10), &fMajor));

    // With no key, each black key gets its everyday name.
    VERIFY_ARE_EQUAL(L"E" + FlatSign, ChordName(Chord(3, ChordType::Major), nullptr));
    VERIFY_ARE_EQUAL(L"A" + FlatSign + L"m", ChordName(Chord(8, ChordType::Minor), nullptr));
    VERIFY_ARE_EQUAL(L"B" + FlatSign + L"7", ChordName(Chord(10, ChordType::Seventh), nullptr));
    VERIFY_ARE_EQUAL(L"F" + SharpSign + L"m", ChordName(Chord(6, ChordType::Minor), nullptr));
    VERIFY_ARE_EQUAL(L"C" + SharpSign + L"dim", ChordName(Chord(1, ChordType::Diminished), nullptr));
    VERIFY_ARE_EQUAL(L"D/F" + SharpSign, ChordName(Chord(D, ChordType::Major, 6), nullptr));
    VERIFY_ARE_EQUAL(std::wstring{ L"C/E" }, ChordName(Chord(C, ChordType::Major, E), nullptr));

    // A bass equal to the root isn't written.
    VERIFY_ARE_EQUAL(std::wstring{ L"C" }, ChordName(Chord(C, ChordType::Major, C), nullptr));
}

void HarmonyTests::EveryTypeHasItsSuffix()
{
    std::vector<std::wstring> const expected{
        L"", L"m", L"7", L"maj7", L"m7", L"dim", L"sus4", L"sus2", L"6", L"m6",
        L"9", L"maj9", L"m9", L"add9", L"aug", L"dim7", L"m7" + FlatSign + L"5", L"7sus4", L"11", L"13" };

    VERIFY_ARE_EQUAL(TypeCount, expected.size());

    for (size_t i = 0; i < expected.size(); ++i)
    {
        VERIFY_ARE_EQUAL(expected[i], ChordTypeSuffix(static_cast<ChordType>(i)));
    }
}

void HarmonyTests::EveryNameReadsBack()
{
    for (size_t type = 0; type < TypeCount; ++type)
    {
        for (uint8_t root = 0; root < 12; ++root)
        {
            for (int bass = -1; bass < 12; bass += 5)
            {
                auto chord = Chord(root, static_cast<ChordType>(type));

                if (bass >= 0 && bass != root)
                {
                    chord.Bass = static_cast<uint8_t>(bass);
                }

                for (bool const flats : { false, true })
                {
                    auto const key = Key(flats ? F : B, ScaleKind::Major);
                    auto const name = ChordName(chord, &key);
                    VerifyParsed(name, chord.Root, chord.Type, chord.Bass);
                }

                VerifyParsed(ChordName(chord, nullptr), chord.Root, chord.Type, chord.Bass);
            }
        }
    }
}

void HarmonyTests::TypedChordsAndAliases()
{
    VerifyParsed(L"Cmaj", C, ChordType::Major);
    VerifyParsed(L"CM", C, ChordType::Major);
    VerifyParsed(L"Cmin", C, ChordType::Minor);
    VerifyParsed(L"C-", C, ChordType::Minor);
    VerifyParsed(L"CM7", C, ChordType::Major7);
    VerifyParsed(L"Cm7", C, ChordType::Minor7);
    VerifyParsed(L"C-7", C, ChordType::Minor7);
    VerifyParsed(L"C\u00B0", C, ChordType::Diminished);
    VerifyParsed(L"C\u00B07", C, ChordType::Diminished7);
    VerifyParsed(L"C\u00F8", C, ChordType::HalfDiminished);
    VerifyParsed(L"Cm7b5", C, ChordType::HalfDiminished);
    VerifyParsed(L"C+", C, ChordType::Augmented);
    VerifyParsed(L"Csus", C, ChordType::Sus4);
    VerifyParsed(L"CM9", C, ChordType::Major9);

    // Sharps and flats typed either way.
    VerifyParsed(L"F#m", 6, ChordType::Minor);
    VerifyParsed(L"F\u266Fm", 6, ChordType::Minor);
    VerifyParsed(L"Bbm7b5", 10, ChordType::HalfDiminished);
    VerifyParsed(L"B\u266D7", 10, ChordType::Seventh);
    VerifyParsed(L"Cb", B, ChordType::Major);
    VerifyParsed(L"E#", F, ChordType::Major);

    // Slash chords, and spaces around the name.
    VerifyParsed(L"C/E", C, ChordType::Major, E);
    VerifyParsed(L"  Dm7/C  ", D, ChordType::Minor7, C);
    VerifyParsed(L"Ab/Eb", 8, ChordType::Major, uint8_t{ 3 });
    VerifyParsed(L"C/C", C, ChordType::Major);

    auto const noChord = ParseChordName(L"N.C.");
    VERIFY_IS_TRUE(noChord.has_value());
    VERIFY_IS_TRUE(noChord->NoChord);
    VERIFY_IS_TRUE(ParseChordName(L" NC ").has_value());

    for (auto const text : { L"", L"c", L"H", L"Cfoo", L"C/", L"C/x", L"C/e", L"Cmaj7/E/G", L"C##", L"/E", L"7" })
    {
        VERIFY_IS_FALSE(ParseChordName(text).has_value(), text);
    }
}

void HarmonyTests::RomanNumerals()
{
    auto const cMajor = Key(C, ScaleKind::Major);
    VERIFY_ARE_EQUAL(std::wstring{ L"I" }, RomanNumeral(Chord(C, ChordType::Major), cMajor));
    VERIFY_ARE_EQUAL(std::wstring{ L"ii7" }, RomanNumeral(Chord(D, ChordType::Minor7), cMajor));
    VERIFY_ARE_EQUAL(std::wstring{ L"V7" }, RomanNumeral(Chord(G, ChordType::Seventh), cMajor));
    VERIFY_ARE_EQUAL(std::wstring{ L"vii\u00B0" }, RomanNumeral(Chord(B, ChordType::Diminished), cMajor));
    VERIFY_ARE_EQUAL(std::wstring{ L"vii\u00F87" }, RomanNumeral(Chord(B, ChordType::HalfDiminished), cMajor));
    VERIFY_ARE_EQUAL(std::wstring{ L"vii\u00B07" }, RomanNumeral(Chord(B, ChordType::Diminished7), cMajor));
    VERIFY_ARE_EQUAL(FlatSign + L"VII", RomanNumeral(Chord(10, ChordType::Major), cMajor));
    VERIFY_ARE_EQUAL(FlatSign + L"III+", RomanNumeral(Chord(3, ChordType::Augmented), cMajor));
    VERIFY_ARE_EQUAL(std::wstring{ L"IVmaj7" }, RomanNumeral(Chord(F, ChordType::Major7), cMajor));
    VERIFY_ARE_EQUAL(std::wstring{ L"Vsus4" }, RomanNumeral(Chord(G, ChordType::Sus4), cMajor));
    VERIFY_ARE_EQUAL(std::wstring{ L"I/III" }, RomanNumeral(Chord(C, ChordType::Major, E), cMajor));
    VERIFY_ARE_EQUAL(L"I7/" + FlatSign + L"VII", RomanNumeral(Chord(C, ChordType::Seventh, 10), cMajor));

    auto const aMinor = Key(A, ScaleKind::NaturalMinor);
    VERIFY_ARE_EQUAL(std::wstring{ L"i" }, RomanNumeral(Chord(A, ChordType::Minor), aMinor));
    VERIFY_ARE_EQUAL(std::wstring{ L"V7" }, RomanNumeral(Chord(E, ChordType::Seventh), aMinor));
    VERIFY_ARE_EQUAL(std::wstring{ L"VII" }, RomanNumeral(Chord(G, ChordType::Major), aMinor));
    VERIFY_ARE_EQUAL(std::wstring{ L"III" }, RomanNumeral(Chord(C, ChordType::Major), aMinor));
    VERIFY_ARE_EQUAL(SharpSign + L"III", RomanNumeral(Chord(1, ChordType::Major), aMinor));
    VERIFY_ARE_EQUAL(SharpSign + L"vii\u00B0", RomanNumeral(Chord(8, ChordType::Diminished), aMinor));

    // Blues counts like the natural minor it sounds like, and chromatic like major.
    VERIFY_ARE_EQUAL(std::wstring{ L"iv7" }, RomanNumeral(Chord(D, ChordType::Minor7), Key(A, ScaleKind::Blues)));
    VERIFY_ARE_EQUAL(std::wstring{ L"V7" }, RomanNumeral(Chord(G, ChordType::Seventh), Key(C, ScaleKind::Chromatic)));
}

void HarmonyTests::ChordsInAndOutOfTheKey()
{
    auto const cMajor = Key(C, ScaleKind::Major);
    VERIFY_IS_TRUE(ChordIsInKey(Chord(D, ChordType::Minor7), cMajor));
    VERIFY_IS_TRUE(ChordIsInKey(Chord(G, ChordType::Seventh), cMajor));
    VERIFY_IS_TRUE(ChordIsInKey(Chord(C, ChordType::Major, E), cMajor));
    VERIFY_IS_FALSE(ChordIsInKey(Chord(10, ChordType::Major), cMajor));
    VERIFY_IS_FALSE(ChordIsInKey(Chord(C, ChordType::Seventh), cMajor));
    VERIFY_IS_FALSE(ChordIsInKey(Chord(C, ChordType::Major, 10), cMajor));

    VERIFY_IS_FALSE(ChordIsInKey(Chord(E, ChordType::Seventh), Key(A, ScaleKind::NaturalMinor)));
    VERIFY_IS_TRUE(ChordIsInKey(Chord(E, ChordType::Seventh), Key(A, ScaleKind::HarmonicMinor)));
    VERIFY_IS_TRUE(ChordIsInKey(Chord(1, ChordType::Diminished7), Key(C, ScaleKind::Chromatic)));
}

void HarmonyTests::ChordNotesAndPitchClasses()
{
    VERIFY_IS_TRUE((std::vector<uint8_t>{ 48, 60, 64, 67 }) == ChordNotes(Chord(C, ChordType::Major)));
    VERIFY_IS_TRUE((std::vector<uint8_t>{ 52, 60, 64, 67 }) == ChordNotes(Chord(C, ChordType::Major, E)));
    VERIFY_IS_TRUE((std::vector<uint8_t>{ 55, 67, 71, 74, 77, 81 }) == ChordNotes(Chord(G, ChordType::Ninth)));

    VERIFY_IS_TRUE((std::vector<uint8_t>{ 0, 4, 7, 10, 2, 9 }) == ChordPitchClasses(Chord(C, ChordType::Thirteenth)));
    VERIFY_IS_TRUE((std::vector<uint8_t>{ 0, 7, 10, 2, 5 }) == ChordPitchClasses(Chord(C, ChordType::Eleventh)));
    VERIFY_IS_TRUE((std::vector<uint8_t>{ 9, 0, 4 }) == ChordPitchClasses(Chord(A, ChordType::Minor)));

    for (size_t type = 0; type < TypeCount; ++type)
    {
        for (uint8_t root = 0; root < 12; ++root)
        {
            auto const chord = Chord(root, static_cast<ChordType>(type), static_cast<uint8_t>((root + 4) % 12));
            auto const notes = ChordNotes(chord);

            VERIFY_IS_TRUE(notes.size() >= 3);
            VERIFY_ARE_EQUAL(static_cast<uint8_t>(48 + (root + 4) % 12), notes.front());
            VERIFY_IS_TRUE(notes.back() <= 71 + 21);
            VERIFY_IS_TRUE(std::is_sorted(notes.begin(), notes.end()));
            VERIFY_IS_TRUE(std::adjacent_find(notes.begin(), notes.end()) == notes.end());
            VERIFY_IS_TRUE(std::find(notes.begin(), notes.end(), static_cast<uint8_t>(60 + root)) != notes.end());
        }
    }

    VERIFY_IS_TRUE(ChordType::Minor == TonicChordType(ScaleKind::Dorian));
    VERIFY_IS_TRUE(ChordType::Minor == TonicChordType(ScaleKind::Blues));
    VERIFY_IS_TRUE(ChordType::Major == TonicChordType(ScaleKind::Mixolydian));
    VERIFY_IS_TRUE(ChordType::Major == TonicChordType(ScaleKind::Chromatic));

    for (size_t scale = 0; scale <= static_cast<size_t>(ScaleKind::Chromatic); ++scale)
    {
        auto const kind = static_cast<ScaleKind>(scale);
        auto const back = ScaleFromId(ScaleId(kind));
        VERIFY_IS_TRUE(back.has_value() && *back == kind);
    }

    VERIFY_IS_FALSE(ScaleFromId(L"Major").has_value());
}

void HarmonyTests::KeyAndChordAtATick()
{
    HarmonyTrack harmony{};
    VERIFY_IS_NULL(KeyAt(harmony, 0));
    VERIFY_IS_NULL(ChordAt(harmony, 0));

    harmony.Keys = { Key(C, ScaleKind::Major, Bar), Key(A, ScaleKind::NaturalMinor, Bar * 4) };
    harmony.Chords = { Chord(C, ChordType::Major, std::nullopt, Bar, Bar), Chord(G, ChordType::Major, std::nullopt, Bar * 3, Bar) };

    // Before the first key, the first key.
    VERIFY_IS_TRUE(&harmony.Keys[0] == KeyAt(harmony, 0));
    VERIFY_IS_TRUE(&harmony.Keys[0] == KeyAt(harmony, Bar * 4 - 1));
    VERIFY_IS_TRUE(&harmony.Keys[1] == KeyAt(harmony, Bar * 4));
    VERIFY_IS_TRUE(&harmony.Keys[1] == KeyAt(harmony, Bar * 100));

    VERIFY_IS_NULL(ChordAt(harmony, Bar - 1));
    VERIFY_IS_TRUE(&harmony.Chords[0] == ChordAt(harmony, Bar));
    VERIFY_IS_TRUE(&harmony.Chords[0] == ChordAt(harmony, Bar * 2 - 1));
    VERIFY_IS_NULL(ChordAt(harmony, Bar * 2));
    VERIFY_IS_TRUE(&harmony.Chords[1] == ChordAt(harmony, Bar * 3));
    VERIFY_IS_NULL(ChordAt(harmony, Bar * 4));
}

void HarmonyTests::PlacingAChordCutsWhatItCovers()
{
    // Covered completely: taken out.
    {
        HarmonyTrack harmony{};
        harmony.Chords = { Chord(C, ChordType::Major, std::nullopt, Bar, Bar) };
        VERIFY_ARE_EQUAL(size_t{ 0 }, PlaceChord(harmony, Chord(D, ChordType::Minor, std::nullopt, 0, Bar * 3)));
        VERIFY_ARE_EQUAL(size_t{ 1 }, harmony.Chords.size());
        VERIFY_ARE_EQUAL(D, harmony.Chords[0].Root);
    }

    // Covered at its start: it starts where the new one ends.
    {
        HarmonyTrack harmony{};
        harmony.Chords = { Chord(C, ChordType::Major, E, Bar, Bar * 2) };
        VERIFY_ARE_EQUAL(size_t{ 0 }, PlaceChord(harmony, Chord(D, ChordType::Minor, std::nullopt, 0, Bar * 2)));
        VERIFY_ARE_EQUAL(size_t{ 2 }, harmony.Chords.size());
        VERIFY_IS_TRUE(Chord(C, ChordType::Major, E, Bar * 2, Bar) == harmony.Chords[1]);
    }

    // Covered at its end: cut short.
    {
        HarmonyTrack harmony{};
        harmony.Chords = { Chord(C, ChordType::Major, std::nullopt, 0, Bar * 2) };
        VERIFY_ARE_EQUAL(size_t{ 1 }, PlaceChord(harmony, Chord(G, ChordType::Seventh, std::nullopt, Bar, Bar * 2)));
        VERIFY_ARE_EQUAL(size_t{ 2 }, harmony.Chords.size());
        VERIFY_IS_TRUE(Chord(C, ChordType::Major, std::nullopt, 0, Bar) == harmony.Chords[0]);
        VERIFY_IS_TRUE(Chord(G, ChordType::Seventh, std::nullopt, Bar, Bar * 2) == harmony.Chords[1]);
    }

    // Inside another: split around it.
    {
        HarmonyTrack harmony{};
        harmony.Chords = { Chord(A, ChordType::Minor7, C, 0, Bar * 4), Chord(F, ChordType::Major, std::nullopt, Bar * 5, Bar) };
        VERIFY_ARE_EQUAL(size_t{ 1 }, PlaceChord(harmony, Chord(E, ChordType::Seventh, std::nullopt, Bar, Bar)));
        VERIFY_ARE_EQUAL(size_t{ 4 }, harmony.Chords.size());
        VERIFY_IS_TRUE(Chord(A, ChordType::Minor7, C, 0, Bar) == harmony.Chords[0]);
        VERIFY_IS_TRUE(Chord(E, ChordType::Seventh, std::nullopt, Bar, Bar) == harmony.Chords[1]);
        VERIFY_IS_TRUE(Chord(A, ChordType::Minor7, C, Bar * 2, Bar * 2) == harmony.Chords[2]);
        VERIFY_ARE_EQUAL(F, harmony.Chords[3].Root);
    }

    // In a gap, nothing else changes, and a bad chord is cleaned on the way in.
    {
        HarmonyTrack harmony{};
        harmony.Chords = { Chord(C, ChordType::Major, std::nullopt, 0, Bar), Chord(G, ChordType::Major, std::nullopt, Bar * 3, Bar) };
        VERIFY_ARE_EQUAL(size_t{ 1 }, PlaceChord(harmony, Chord(14, ChordType::Minor, 26, Bar, 0)));
        VERIFY_ARE_EQUAL(size_t{ 3 }, harmony.Chords.size());
        VERIFY_IS_TRUE(Chord(D, ChordType::Minor, std::nullopt, Bar, 1) == harmony.Chords[1]);
        VERIFY_ARE_EQUAL(Bar, harmony.Chords[0].Length);
        VERIFY_ARE_EQUAL(Bar * 3, harmony.Chords[2].Tick);
    }
}

void HarmonyTests::NormalizeSortsAndCleans()
{
    HarmonyTrack harmony{};
    harmony.Keys = {
        Key(G, ScaleKind::Major, Bar),
        Key(14, static_cast<ScaleKind>(40), -5),
        Key(E, ScaleKind::NaturalMinor, Bar),
    };
    harmony.Chords = {
        Chord(G, ChordType::Major, std::nullopt, Bar * 2, Bar),
        Chord(C, ChordType::Major, std::nullopt, 0, Bar * 4),
        Chord(15, static_cast<ChordType>(200), 27, Bar * 2, -3),
        Chord(F, ChordType::Major, F, Bar * 5, Bar),
    };

    NormalizeHarmony(harmony);

    // The later of two keys at one tick wins.
    VERIFY_ARE_EQUAL(size_t{ 2 }, harmony.Keys.size());
    VERIFY_IS_TRUE(Key(D, ScaleKind::Major, 0) == harmony.Keys[0]);
    VERIFY_IS_TRUE(Key(E, ScaleKind::NaturalMinor, Bar) == harmony.Keys[1]);

    // The C is cut where the next chord starts, and the later of two chords at one tick wins.
    VERIFY_ARE_EQUAL(size_t{ 3 }, harmony.Chords.size());
    VERIFY_IS_TRUE(Chord(C, ChordType::Major, std::nullopt, 0, Bar * 2) == harmony.Chords[0]);
    VERIFY_IS_TRUE(Chord(3, ChordType::Major, std::nullopt, Bar * 2, 1) == harmony.Chords[1]);
    VERIFY_IS_TRUE(Chord(F, ChordType::Major, std::nullopt, Bar * 5, Bar) == harmony.Chords[2]);

    for (size_t i = 1; i < harmony.Chords.size(); ++i)
    {
        VERIFY_IS_TRUE(harmony.Chords[i - 1].Tick + harmony.Chords[i - 1].Length <= harmony.Chords[i].Tick);
    }
}

void HarmonyTests::TransposeMovesOnlyTheRange()
{
    HarmonyTrack harmony{};
    harmony.Chords = {
        Chord(C, ChordType::Major, E, 0, Bar),
        Chord(B, ChordType::Minor, std::nullopt, Bar, Bar),
        Chord(G, ChordType::Major, std::nullopt, Bar * 2, Bar),
    };

    TransposeChords(harmony, Bar, Bar * 2, 2);
    VERIFY_ARE_EQUAL(C, harmony.Chords[0].Root);
    VERIFY_IS_TRUE(harmony.Chords[0].Bass == std::optional<uint8_t>{ E });
    VERIFY_ARE_EQUAL(uint8_t{ 1 }, harmony.Chords[1].Root);
    VERIFY_ARE_EQUAL(G, harmony.Chords[2].Root);

    TransposeChords(harmony, 0, Bar * 10, -13);
    VERIFY_ARE_EQUAL(B, harmony.Chords[0].Root);
    VERIFY_IS_TRUE(harmony.Chords[0].Bass == std::optional<uint8_t>{ uint8_t{ 3 } });
    VERIFY_ARE_EQUAL(uint8_t{ 0 }, harmony.Chords[1].Root);
    VERIFY_ARE_EQUAL(uint8_t{ 6 }, harmony.Chords[2].Root);
}

void HarmonyTests::UndoPutsTheRowBack()
{
    Sequence sequence{};

    HarmonyTrack after{};
    after.Keys = { Key(A, ScaleKind::NaturalMinor) };
    after.Chords = { Chord(A, ChordType::Minor) };

    auto change = MakeHarmonyChange(std::nullopt, after);
    VERIFY_IS_TRUE(change->Cost() >= sizeof(HarmonyTrack) * 2);

    change->Apply(sequence);
    VERIFY_IS_TRUE(sequence.Harmony.has_value());
    VERIFY_IS_TRUE(after.Chords == sequence.Harmony->Chords);
    VERIFY_IS_TRUE(after.Keys == sequence.Harmony->Keys);

    change->Revert(sequence);
    VERIFY_IS_FALSE(sequence.Harmony.has_value());

    auto hide = after;
    hide.Hidden = true;
    auto second = MakeHarmonyChange(after, hide);
    sequence.Harmony = after;
    second->Apply(sequence);
    VERIFY_IS_TRUE(sequence.Harmony->Hidden);
    second->Revert(sequence);
    VERIFY_IS_FALSE(sequence.Harmony->Hidden);
}

void HarmonyTests::SequenceFileRoundTrip()
{
    Sequence original{};
    original.Name = L"Harmony";

    HarmonyTrack harmony{};
    harmony.Hidden = true;
    harmony.Keys = { Key(A, ScaleKind::NaturalMinor), Key(3, ScaleKind::Major, Bar * 4) };
    harmony.Chords = {
        Chord(A, ChordType::Minor, std::nullopt, 0, Bar),
        Chord(E, ChordType::Seventh, 8, Bar, Bar),
        Chord(10, ChordType::HalfDiminished, std::nullopt, Bar * 4, Bar * 2),
    };

    winrt::Windows::Data::Json::JsonObject unknown{};
    unknown.SetNamedValue(L"future", winrt::Windows::Data::Json::JsonValue::CreateStringValue(L"kept"));
    harmony.Unknown = unknown;
    original.Harmony = harmony;

    auto const text = WriteSequenceJson(original);

    // Names spelled in the key that holds them: G♯ in A minor, B♭ in E♭ major.
    VERIFY_IS_TRUE(text.find(L"\"keyAndChords\": {\n    \"hidden\": true,\n    \"keys\": [\n      { \"tick\": 0, \"root\": \"A\", \"scale\": \"naturalMinor\" },") != std::wstring::npos);
    VERIFY_IS_TRUE(text.find(L"{ \"tick\": 15360, \"root\": \"E" + FlatSign + L"\", \"scale\": \"major\" }") != std::wstring::npos);
    VERIFY_IS_TRUE(text.find(L"{ \"tick\": 3840, \"length\": 3840, \"chord\": \"E7/G" + SharpSign + L"\" }") != std::wstring::npos);
    VERIFY_IS_TRUE(text.find(L"\"chord\": \"B" + FlatSign + L"m7" + FlatSign + L"5\"") != std::wstring::npos);

    Sequence read{};
    auto const result = ReadSequenceJson(text, read);
    VERIFY_IS_TRUE(result.Succeeded());
    VERIFY_ARE_EQUAL(size_t{ 0 }, result.SkippedItems);
    VERIFY_IS_TRUE(read.Harmony.has_value());
    VERIFY_IS_TRUE(read.Harmony->Hidden);
    VERIFY_IS_TRUE(harmony.Keys == read.Harmony->Keys);
    VERIFY_IS_TRUE(harmony.Chords == read.Harmony->Chords);
    VERIFY_IS_TRUE(read.Harmony->Unknown != nullptr && read.Harmony->Unknown.HasKey(L"future"));
    VERIFY_IS_TRUE(read.Unknown == nullptr || !read.Unknown.HasKey(HarmonyKey));
    VERIFY_ARE_EQUAL(text, WriteSequenceJson(read));

    // An empty row that's shown still writes both lists, and no "hidden".
    original.Harmony = HarmonyTrack{};
    auto const empty = WriteSequenceJson(original);
    VERIFY_IS_TRUE(empty.find(L"\"keyAndChords\": {\n    \"keys\": [],\n    \"chords\": []\n  }") != std::wstring::npos);

    Sequence emptyRead{};
    VERIFY_IS_TRUE(ReadSequenceJson(empty, emptyRead).Succeeded());
    VERIFY_IS_TRUE(emptyRead.Harmony.has_value());
    VERIFY_IS_FALSE(emptyRead.Harmony->Hidden);
    VERIFY_IS_TRUE(emptyRead.Harmony->Keys.empty());
}

void HarmonyTests::SequenceFileWithoutKeyAndChords()
{
    Sequence original{};
    original.Name = L"No harmony";

    auto const text = WriteSequenceJson(original);
    VERIFY_IS_TRUE(text.find(HarmonyKey) == std::wstring::npos);

    Sequence read{};
    auto const result = ReadSequenceJson(text, read);
    VERIFY_IS_TRUE(result.Succeeded());
    VERIFY_IS_FALSE(read.Harmony.has_value());
}

void HarmonyTests::DamagedEntriesAreSkipped()
{
    std::wstring const text = LR"({
  "fileVersion": 1,
  "keyAndChords": {
    "hidden": "yes",
    "keys": [
      { "tick": 0, "root": "C", "scale": "major" },
      { "tick": 960, "root": "H", "scale": "major" },
      { "tick": 960, "root": "D", "scale": "klingon" },
      { "root": "D", "scale": "major" },
      "junk"
    ],
    "chords": [
      { "tick": 0, "length": 3840, "chord": "C" },
      { "tick": 3840, "length": 3840, "chord": "N.C." },
      { "tick": 3840, "length": 3840, "chord": "Cfoo" },
      { "tick": 3840, "chord": "G" },
      { "tick": -50, "length": 0, "chord": "Dm" },
      7
    ],
    "extra": [1, 2]
  },
  "tracks": [],
  "clips": []
})";

    Sequence read{};
    auto const result = ReadSequenceJson(text, read);
    VERIFY_IS_TRUE(result.Succeeded());

    // "hidden", three keys and the junk, then N.C., Cfoo, the chord with no length and the 7.
    VERIFY_ARE_EQUAL(size_t{ 9 }, result.SkippedItems);
    VERIFY_IS_TRUE(read.Harmony.has_value());
    VERIFY_IS_FALSE(read.Harmony->Hidden);
    VERIFY_ARE_EQUAL(size_t{ 1 }, read.Harmony->Keys.size());

    // The Dm at -50 lands at 0 with a length of 1, and the C that was there gives way to it.
    VERIFY_ARE_EQUAL(size_t{ 1 }, read.Harmony->Chords.size());
    VERIFY_IS_TRUE(Chord(D, ChordType::Minor, std::nullopt, 0, 1) == read.Harmony->Chords[0]);
    VERIFY_IS_TRUE(read.Harmony->Unknown != nullptr && read.Harmony->Unknown.HasKey(L"extra"));

    // A value that isn't an object is skipped and leaves no row.
    Sequence notObject{};
    auto const second = ReadSequenceJson(LR"({ "fileVersion": 1, "keyAndChords": [], "tracks": [], "clips": [] })", notObject);
    VERIFY_IS_TRUE(second.Succeeded());
    VERIFY_ARE_EQUAL(size_t{ 1 }, second.SkippedItems);
    VERIFY_IS_FALSE(notObject.Harmony.has_value());
}

void HarmonyTests::OlderTicksAreScaled()
{
    std::wstring const text = LR"({
  "fileVersion": 1,
  "ticksPerQuarterNote": 480,
  "keyAndChords": {
    "keys": [ { "tick": 1920, "root": "G", "scale": "mixolydian" } ],
    "chords": [ { "tick": 480, "length": 1440, "chord": "G7sus4" } ]
  },
  "tracks": [],
  "clips": []
})";

    Sequence read{};
    auto const result = ReadSequenceJson(text, read);
    VERIFY_IS_TRUE(result.Succeeded());
    VERIFY_ARE_EQUAL(size_t{ 0 }, result.SkippedItems);
    VERIFY_IS_TRUE(read.Harmony.has_value());
    VERIFY_IS_TRUE(Key(G, ScaleKind::Mixolydian, Bar) == read.Harmony->Keys[0]);
    VERIFY_IS_TRUE(Chord(G, ChordType::SeventhSus4, std::nullopt, TicksPerQuarterNote, TicksPerQuarterNote * 3) == read.Harmony->Chords[0]);
}
