// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#include "Harmony.h"

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

// windows.h renames GetObject, which Windows.Data.Json uses.
#undef GetObject

#include <algorithm>
#include <array>
#include <cmath>
#include <format>
#include <utility>

namespace midisequencer
{
    namespace
    {
        namespace json = winrt::Windows::Data::Json;

        constexpr wchar_t KeyHidden[] = L"hidden";
        constexpr wchar_t KeyKeys[] = L"keys";
        constexpr wchar_t KeyChords[] = L"chords";
        constexpr wchar_t KeyTick[] = L"tick";
        constexpr wchar_t KeyLength[] = L"length";
        constexpr wchar_t KeyRoot[] = L"root";
        constexpr wchar_t KeyScale[] = L"scale";
        constexpr wchar_t KeyChord[] = L"chord";

        constexpr wchar_t Sharp = L'\u266F';
        constexpr wchar_t Flat = L'\u266D';

        constexpr int64_t MaximumTick = SequenceLimits{}.MaximumTick;

        constexpr size_t ScaleCount = static_cast<size_t>(ScaleKind::Chromatic) + 1;
        constexpr size_t ChordTypeCount = static_cast<size_t>(ChordType::Thirteenth) + 1;

        // The same steps the generators use (SequenceGenerator.cpp).
        struct ScaleSteps
        {
            std::array<uint8_t, 12> Steps{};
            size_t Count{ 0 };
        };

        constexpr std::array<ScaleSteps, ScaleCount> Scales{ {
            { { 0, 2, 4, 5, 7, 9, 11 }, 7 },                        // major
            { { 0, 2, 3, 5, 7, 8, 10 }, 7 },                        // natural minor
            { { 0, 2, 3, 5, 7, 9, 10 }, 7 },                        // dorian
            { { 0, 1, 3, 5, 7, 8, 10 }, 7 },                        // phrygian
            { { 0, 2, 4, 6, 7, 9, 11 }, 7 },                        // lydian
            { { 0, 2, 4, 5, 7, 9, 10 }, 7 },                        // mixolydian
            { { 0, 2, 3, 5, 7, 8, 11 }, 7 },                        // harmonic minor
            { { 0, 2, 4, 7, 9 }, 5 },                               // major pentatonic
            { { 0, 3, 5, 7, 10 }, 5 },                              // minor pentatonic
            { { 0, 3, 5, 6, 7, 10 }, 6 },                           // blues
            { { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11 }, 12 },       // chromatic
        } };

        // Semitones from a key's root up to the root of the major key that shares its notes.
        constexpr std::array<uint8_t, ScaleCount> ParentMajorOffsets{ 0, 3, 10, 8, 7, 5, 3, 0, 3, 3, 0 };

        constexpr std::array<std::wstring_view, ScaleCount> ScaleIds{
            L"major", L"naturalMinor", L"dorian", L"phrygian", L"lydian", L"mixolydian",
            L"harmonicMinor", L"majorPentatonic", L"minorPentatonic", L"blues", L"chromatic" };

        struct ChordShape
        {
            std::array<uint8_t, 6> Intervals{};
            size_t Count{ 0 };
            std::wstring_view Suffix{};
            std::wstring_view NumeralSuffix{};
            bool MinorThird{ false };
        };

        constexpr std::array<ChordShape, ChordTypeCount> Shapes{ {
            { { 0, 4, 7 }, 3, L"", L"", false },
            { { 0, 3, 7 }, 3, L"m", L"", true },
            { { 0, 4, 7, 10 }, 4, L"7", L"7", false },
            { { 0, 4, 7, 11 }, 4, L"maj7", L"maj7", false },
            { { 0, 3, 7, 10 }, 4, L"m7", L"7", true },
            { { 0, 3, 6 }, 3, L"dim", L"\u00B0", true },
            { { 0, 5, 7 }, 3, L"sus4", L"sus4", false },
            { { 0, 2, 7 }, 3, L"sus2", L"sus2", false },
            { { 0, 4, 7, 9 }, 4, L"6", L"6", false },
            { { 0, 3, 7, 9 }, 4, L"m6", L"6", true },
            { { 0, 4, 7, 10, 14 }, 5, L"9", L"9", false },
            { { 0, 4, 7, 11, 14 }, 5, L"maj9", L"maj9", false },
            { { 0, 3, 7, 10, 14 }, 5, L"m9", L"9", true },
            { { 0, 4, 7, 14 }, 4, L"add9", L"add9", false },
            { { 0, 4, 8 }, 3, L"aug", L"+", false },
            { { 0, 3, 6, 9 }, 4, L"dim7", L"\u00B07", true },
            { { 0, 3, 6, 10 }, 4, L"m7\u266D5", L"\u00F87", true },
            { { 0, 5, 7, 10 }, 4, L"7sus4", L"7sus4", false },
            // An eleventh leaves out the third, which clashes with the eleventh.
            { { 0, 7, 10, 14, 17 }, 5, L"11", L"11", false },
            { { 0, 4, 7, 10, 14, 21 }, 6, L"13", L"13", false },
        } };

        // Everything a chord's suffix can be typed as, the names it's written with included.
        struct SuffixAlias
        {
            std::wstring_view Text{};
            ChordType Type{ ChordType::Major };
        };

        constexpr std::array SuffixAliases{
            SuffixAlias{ L"", ChordType::Major },
            SuffixAlias{ L"maj", ChordType::Major },
            SuffixAlias{ L"M", ChordType::Major },
            SuffixAlias{ L"m", ChordType::Minor },
            SuffixAlias{ L"min", ChordType::Minor },
            SuffixAlias{ L"-", ChordType::Minor },
            SuffixAlias{ L"7", ChordType::Seventh },
            SuffixAlias{ L"maj7", ChordType::Major7 },
            SuffixAlias{ L"M7", ChordType::Major7 },
            SuffixAlias{ L"m7", ChordType::Minor7 },
            SuffixAlias{ L"min7", ChordType::Minor7 },
            SuffixAlias{ L"-7", ChordType::Minor7 },
            SuffixAlias{ L"dim", ChordType::Diminished },
            SuffixAlias{ L"\u00B0", ChordType::Diminished },
            SuffixAlias{ L"sus4", ChordType::Sus4 },
            SuffixAlias{ L"sus", ChordType::Sus4 },
            SuffixAlias{ L"sus2", ChordType::Sus2 },
            SuffixAlias{ L"6", ChordType::Sixth },
            SuffixAlias{ L"m6", ChordType::Minor6 },
            SuffixAlias{ L"min6", ChordType::Minor6 },
            SuffixAlias{ L"-6", ChordType::Minor6 },
            SuffixAlias{ L"9", ChordType::Ninth },
            SuffixAlias{ L"maj9", ChordType::Major9 },
            SuffixAlias{ L"M9", ChordType::Major9 },
            SuffixAlias{ L"m9", ChordType::Minor9 },
            SuffixAlias{ L"min9", ChordType::Minor9 },
            SuffixAlias{ L"-9", ChordType::Minor9 },
            SuffixAlias{ L"add9", ChordType::Add9 },
            SuffixAlias{ L"aug", ChordType::Augmented },
            SuffixAlias{ L"+", ChordType::Augmented },
            SuffixAlias{ L"dim7", ChordType::Diminished7 },
            SuffixAlias{ L"\u00B07", ChordType::Diminished7 },
            SuffixAlias{ L"m7\u266D5", ChordType::HalfDiminished },
            SuffixAlias{ L"m7b5", ChordType::HalfDiminished },
            SuffixAlias{ L"min7b5", ChordType::HalfDiminished },
            SuffixAlias{ L"-7b5", ChordType::HalfDiminished },
            SuffixAlias{ L"\u00F8", ChordType::HalfDiminished },
            SuffixAlias{ L"\u00F87", ChordType::HalfDiminished },
            SuffixAlias{ L"7sus4", ChordType::SeventhSus4 },
            SuffixAlias{ L"7sus", ChordType::SeventhSus4 },
            SuffixAlias{ L"11", ChordType::Eleventh },
            SuffixAlias{ L"13", ChordType::Thirteenth },
        };

        constexpr std::array<std::wstring_view, 7> Numerals{ L"I", L"II", L"III", L"IV", L"V", L"VI", L"VII" };
        constexpr std::array<std::wstring_view, 7> LowerNumerals{ L"i", L"ii", L"iii", L"iv", L"v", L"vi", L"vii" };

        ScaleSteps const& StepsOf(_In_ ScaleKind scale) noexcept
        {
            auto const index = static_cast<size_t>(scale);
            return Scales[index < Scales.size() ? index : 0];
        }

        ChordShape const& ShapeOf(_In_ ChordType type) noexcept
        {
            auto const index = static_cast<size_t>(type);
            return Shapes[index < Shapes.size() ? index : 0];
        }

        bool InSteps(_In_ ScaleSteps const& steps, _In_ uint8_t interval) noexcept
        {
            return std::find(steps.Steps.begin(), steps.Steps.begin() + static_cast<ptrdiff_t>(steps.Count), interval) !=
                steps.Steps.begin() + static_cast<ptrdiff_t>(steps.Count);
        }

        // A black key with no key to spell it in: E♭, A♭ and B♭ are more common than D♯, G♯ and
        // A♯, and F♯ and C♯ more common than G♭ and D♭.
        bool DefaultFlat(_In_ uint8_t pitchClass) noexcept
        {
            return pitchClass == 3 || pitchClass == 8 || pitchClass == 10;
        }

        std::wstring_view Trim(_In_ std::wstring_view text) noexcept
        {
            auto const space = [](wchar_t c) { return c == L' ' || c == L'\t' || c == L'\r' || c == L'\n' || c == L'\u00A0'; };

            while (!text.empty() && space(text.front()))
            {
                text.remove_prefix(1);
            }

            while (!text.empty() && space(text.back()))
            {
                text.remove_suffix(1);
            }

            return text;
        }

        // A note name at the start of the text: an upper case letter A to G, then at most one
        // sharp or flat. Takes what it read off the text.
        std::optional<uint8_t> ReadNoteName(_Inout_ std::wstring_view& text) noexcept
        {
            static constexpr std::array<uint8_t, 7> Letters{ 9, 11, 0, 2, 4, 5, 7 };

            if (text.empty() || text.front() < L'A' || text.front() > L'G')
            {
                return std::nullopt;
            }

            int pitch = Letters[static_cast<size_t>(text.front() - L'A')];
            text.remove_prefix(1);

            if (!text.empty())
            {
                if (text.front() == Sharp || text.front() == L'#')
                {
                    ++pitch;
                    text.remove_prefix(1);
                }
                else if (text.front() == Flat || text.front() == L'b')
                {
                    pitch += 11;
                    text.remove_prefix(1);
                }
            }

            return static_cast<uint8_t>(pitch % 12);
        }

        std::optional<uint8_t> ParseNoteName(_In_ std::wstring_view text) noexcept
        {
            text = Trim(text);
            auto const pitch = ReadNoteName(text);

            if (!pitch || !text.empty())
            {
                return std::nullopt;
            }

            return pitch;
        }

        std::optional<ChordType> ParseSuffix(_In_ std::wstring_view text) noexcept
        {
            for (auto const& alias : SuffixAliases)
            {
                if (alias.Text == text)
                {
                    return alias.Type;
                }
            }

            return std::nullopt;
        }

        // The numeral for a note in a key, with ♭ or ♯ before it when the note isn't the scale's
        // own degree. Pentatonic, blues and chromatic keys count degrees like the major or
        // natural minor scale they sound like.
        std::pair<std::wstring, size_t> Degree(_In_ uint8_t pitchClass, _In_ KeyChange const& key)
        {
            auto reference = key.Scale;

            if (reference == ScaleKind::MajorPentatonic || reference == ScaleKind::Chromatic)
            {
                reference = ScaleKind::Major;
            }
            else if (reference == ScaleKind::MinorPentatonic || reference == ScaleKind::Blues)
            {
                reference = ScaleKind::NaturalMinor;
            }

            auto const& steps = StepsOf(reference);
            auto const interval = static_cast<uint8_t>((pitchClass % 12 + 12 - key.Root % 12) % 12);

            for (size_t i = 0; i < steps.Count; ++i)
            {
                if (steps.Steps[i] == interval)
                {
                    return { std::wstring{}, i };
                }
            }

            // Outside the scale: the major scale's degree for the note, or the degree above it for
            // a note between two, then a sharp or flat for how far it is from this scale's degree.
            auto const& major = StepsOf(ScaleKind::Major);

            auto const step = [&](uint8_t wanted)
            {
                for (size_t i = 0; i < major.Count; ++i)
                {
                    if (major.Steps[i] == wanted)
                    {
                        return std::optional<size_t>{ i };
                    }
                }

                return std::optional<size_t>{};
            };

            auto const exact = step(interval);
            auto const degree = exact ? *exact : step(static_cast<uint8_t>((interval + 1) % 12)).value_or(0);

            auto difference = static_cast<int>(interval) - static_cast<int>(steps.Steps[degree]);

            if (difference > 6)
            {
                difference -= 12;
            }
            else if (difference < -6)
            {
                difference += 12;
            }

            std::wstring prefix(static_cast<size_t>(std::abs(difference)), difference < 0 ? Flat : Sharp);
            return { std::move(prefix), degree };
        }

        std::wstring Quote(_In_ std::wstring_view text)
        {
            return std::wstring{ json::JsonValue::CreateStringValue(text).Stringify() };
        }

        std::optional<double> NumberMember(_In_ json::JsonObject const& object, _In_ std::wstring_view key)
        {
            if (!object.HasKey(key))
            {
                return std::nullopt;
            }

            auto const value = object.Lookup(key);

            if (value == nullptr || value.ValueType() != json::JsonValueType::Number)
            {
                return std::nullopt;
            }

            auto const number = value.GetNumber();

            if (!std::isfinite(number))
            {
                return std::nullopt;
            }

            return number;
        }

        std::optional<std::wstring> StringMember(_In_ json::JsonObject const& object, _In_ std::wstring_view key)
        {
            if (!object.HasKey(key))
            {
                return std::nullopt;
            }

            auto const value = object.Lookup(key);

            if (value == nullptr || value.ValueType() != json::JsonValueType::String)
            {
                return std::nullopt;
            }

            return std::wstring{ value.GetString() };
        }

        int64_t TickFrom(_In_ double number) noexcept
        {
            return static_cast<int64_t>(std::clamp(std::round(number), 0.0, static_cast<double>(MaximumTick)));
        }

        // The array a member holds, or nothing. One that's there and isn't an array is counted.
        json::JsonArray ArrayMember(_In_ json::JsonObject const& object, _In_ std::wstring_view key, _Inout_ size_t& skipped)
        {
            if (!object.HasKey(key))
            {
                return nullptr;
            }

            auto const value = object.Lookup(key);

            if (value == nullptr || value.ValueType() != json::JsonValueType::Array)
            {
                ++skipped;
                return nullptr;
            }

            return value.GetArray();
        }

        void CleanChord(_Inout_ ChordEvent& chord) noexcept
        {
            chord.Tick = std::clamp<int64_t>(chord.Tick, 0, MaximumTick);
            chord.Length = std::clamp<int64_t>(chord.Length, 1, std::max<int64_t>(1, MaximumTick - chord.Tick));
            chord.Root = static_cast<uint8_t>(chord.Root % 12);

            if (static_cast<size_t>(chord.Type) >= ChordTypeCount)
            {
                chord.Type = ChordType::Major;
            }

            if (chord.Bass)
            {
                chord.Bass = static_cast<uint8_t>(*chord.Bass % 12);

                if (*chord.Bass == chord.Root)
                {
                    chord.Bass.reset();
                }
            }
        }
    }

    // ---- names ----

    _Use_decl_annotations_
    bool KeyUsesFlats(uint8_t root, ScaleKind scale) noexcept
    {
        auto const index = static_cast<size_t>(scale) < ScaleCount ? static_cast<size_t>(scale) : 0;
        auto const parent = (root % 12 + ParentMajorOffsets[index]) % 12;

        // F, B♭, E♭, A♭, D♭ and G♭ major.
        return parent == 5 || parent == 10 || parent == 3 || parent == 8 || parent == 1 || parent == 6;
    }

    _Use_decl_annotations_
    KeySignature KeySignatureOf(KeyChange const& key) noexcept
    {
        // Sharps or flats of each major key by its root, spelled as KeyUsesFlats spells them.
        static constexpr std::array<int8_t, 12> MajorAccidentals{ 0, -5, 2, -3, 4, -1, -6, 1, -4, 3, -2, 5 };

        auto const index = static_cast<size_t>(key.Scale) < ScaleCount ? static_cast<size_t>(key.Scale) : 0;
        auto const parent = (key.Root % 12 + ParentMajorOffsets[index]) % 12;

        KeySignature signature{};
        signature.Accidentals = MajorAccidentals[parent];
        signature.Minor = TonicChordType(key.Scale) == ChordType::Minor;
        return signature;
    }

    _Use_decl_annotations_
    KeyChange KeyFromSignature(KeySignature signature, int64_t tick) noexcept
    {
        auto const accidentals = std::clamp<int>(signature.Accidentals, -7, 7);
        auto const majorRoot = static_cast<uint8_t>(((accidentals * 7) % 12 + 12) % 12);

        KeyChange key{};
        key.Tick = std::max<int64_t>(0, tick);
        key.Root = signature.Minor ? static_cast<uint8_t>((majorRoot + 9) % 12) : majorRoot;
        key.Scale = signature.Minor ? ScaleKind::NaturalMinor : ScaleKind::Major;
        return key;
    }

    _Use_decl_annotations_
    std::wstring NoteName(uint8_t pitchClass, bool flats)
    {
        static constexpr std::array<std::wstring_view, 12> SharpNames{
            L"C", L"C\u266F", L"D", L"D\u266F", L"E", L"F", L"F\u266F", L"G", L"G\u266F", L"A", L"A\u266F", L"B" };
        static constexpr std::array<std::wstring_view, 12> FlatNames{
            L"C", L"D\u266D", L"D", L"E\u266D", L"E", L"F", L"G\u266D", L"G", L"A\u266D", L"A", L"B\u266D", L"B" };

        return std::wstring{ (flats ? FlatNames : SharpNames)[pitchClass % 12] };
    }

    _Use_decl_annotations_
    std::wstring ChordTypeSuffix(ChordType type)
    {
        return std::wstring{ ShapeOf(type).Suffix };
    }

    _Use_decl_annotations_
    std::wstring ChordName(ChordEvent const& chord, KeyChange const* key)
    {
        auto const spell = [&](uint8_t pitchClass)
        {
            auto const flats = key != nullptr ? KeyUsesFlats(key->Root, key->Scale) : DefaultFlat(pitchClass % 12);
            return NoteName(pitchClass, flats);
        };

        auto name = spell(chord.Root) + ChordTypeSuffix(chord.Type);

        if (chord.Bass && *chord.Bass % 12 != chord.Root % 12)
        {
            name += L"/";
            name += spell(*chord.Bass);
        }

        return name;
    }

    _Use_decl_annotations_
    std::optional<ParsedChord> ParseChordName(std::wstring_view text)
    {
        text = Trim(text);

        if (text == L"N.C." || text == L"N.C" || text == L"NC")
        {
            return ParsedChord{ true, ChordEvent{} };
        }

        auto const root = ReadNoteName(text);

        if (!root)
        {
            return std::nullopt;
        }

        std::wstring_view suffix{ text };
        std::optional<uint8_t> bass{};

        if (auto const slash = text.find(L'/'); slash != std::wstring_view::npos)
        {
            suffix = text.substr(0, slash);
            bass = ParseNoteName(text.substr(slash + 1));

            if (!bass)
            {
                return std::nullopt;
            }
        }

        auto const type = ParseSuffix(Trim(suffix));

        if (!type)
        {
            return std::nullopt;
        }

        ParsedChord parsed{};
        parsed.Chord.Root = *root;
        parsed.Chord.Type = *type;

        if (bass && *bass != *root)
        {
            parsed.Chord.Bass = bass;
        }

        return parsed;
    }

    _Use_decl_annotations_
    std::wstring RomanNumeral(ChordEvent const& chord, KeyChange const& key)
    {
        auto const& shape = ShapeOf(chord.Type);
        auto [prefix, degree] = Degree(chord.Root, key);

        auto numeral = std::move(prefix);
        numeral += shape.MinorThird ? LowerNumerals[degree] : Numerals[degree];
        numeral += shape.NumeralSuffix;

        if (chord.Bass && *chord.Bass % 12 != chord.Root % 12)
        {
            auto [bassPrefix, bassDegree] = Degree(*chord.Bass, key);
            numeral += L"/";
            numeral += bassPrefix;
            numeral += Numerals[bassDegree];
        }

        return numeral;
    }

    _Use_decl_annotations_
    bool ChordIsInKey(ChordEvent const& chord, KeyChange const& key)
    {
        if (key.Scale == ScaleKind::Chromatic)
        {
            return true;
        }

        auto const& steps = StepsOf(key.Scale);
        auto const inKey = [&](uint8_t pitchClass)
        {
            return InSteps(steps, static_cast<uint8_t>((pitchClass % 12 + 12 - key.Root % 12) % 12));
        };

        for (auto const pitchClass : ChordPitchClasses(chord))
        {
            if (!inKey(pitchClass))
            {
                return false;
            }
        }

        return !chord.Bass || inKey(*chord.Bass);
    }

    _Use_decl_annotations_
    std::wstring_view ScaleId(ScaleKind scale) noexcept
    {
        auto const index = static_cast<size_t>(scale);
        return ScaleIds[index < ScaleIds.size() ? index : 0];
    }

    _Use_decl_annotations_
    std::optional<ScaleKind> ScaleFromId(std::wstring_view id) noexcept
    {
        for (size_t i = 0; i < ScaleIds.size(); ++i)
        {
            if (ScaleIds[i] == id)
            {
                return static_cast<ScaleKind>(i);
            }
        }

        return std::nullopt;
    }

    // ---- notes ----

    _Use_decl_annotations_
    std::vector<uint8_t> ChordPitchClasses(ChordEvent const& chord)
    {
        auto const& shape = ShapeOf(chord.Type);
        std::vector<uint8_t> pitchClasses{};

        for (size_t i = 0; i < shape.Count; ++i)
        {
            auto const pitchClass = static_cast<uint8_t>((chord.Root % 12 + shape.Intervals[i]) % 12);

            if (std::find(pitchClasses.begin(), pitchClasses.end(), pitchClass) == pitchClasses.end())
            {
                pitchClasses.push_back(pitchClass);
            }
        }

        return pitchClasses;
    }

    _Use_decl_annotations_
    std::vector<uint8_t> ChordNotes(ChordEvent const& chord)
    {
        auto const& shape = ShapeOf(chord.Type);
        auto const root = 60 + chord.Root % 12;

        std::vector<uint8_t> notes{};

        for (size_t i = 0; i < shape.Count; ++i)
        {
            notes.push_back(static_cast<uint8_t>(std::min(root + shape.Intervals[i], 127)));
        }

        notes.push_back(static_cast<uint8_t>(48 + (chord.Bass ? *chord.Bass : chord.Root) % 12));

        std::sort(notes.begin(), notes.end());
        notes.erase(std::unique(notes.begin(), notes.end()), notes.end());
        return notes;
    }

    _Use_decl_annotations_
    ChordType TonicChordType(ScaleKind scale) noexcept
    {
        switch (scale)
        {
        case ScaleKind::NaturalMinor:
        case ScaleKind::HarmonicMinor:
        case ScaleKind::Dorian:
        case ScaleKind::Phrygian:
        case ScaleKind::MinorPentatonic:
        case ScaleKind::Blues:
            return ChordType::Minor;
        default:
            return ChordType::Major;
        }
    }

    // ---- the row ----

    _Use_decl_annotations_
    KeyChange const* KeyAt(HarmonyTrack const& harmony, int64_t tick) noexcept
    {
        if (harmony.Keys.empty())
        {
            return nullptr;
        }

        auto const after = std::upper_bound(harmony.Keys.begin(), harmony.Keys.end(), tick,
            [](int64_t t, KeyChange const& key) { return t < key.Tick; });

        return after == harmony.Keys.begin() ? &harmony.Keys.front() : &*(after - 1);
    }

    _Use_decl_annotations_
    ChordEvent const* ChordAt(HarmonyTrack const& harmony, int64_t tick) noexcept
    {
        auto const after = std::upper_bound(harmony.Chords.begin(), harmony.Chords.end(), tick,
            [](int64_t t, ChordEvent const& chord) { return t < chord.Tick; });

        if (after == harmony.Chords.begin())
        {
            return nullptr;
        }

        auto const& chord = *(after - 1);
        return tick - chord.Tick < chord.Length ? &chord : nullptr;
    }

    _Use_decl_annotations_
    void NormalizeHarmony(HarmonyTrack& harmony)
    {
        if (harmony.Keys.size() > MaximumHarmonyEvents)
        {
            harmony.Keys.resize(MaximumHarmonyEvents);
        }

        if (harmony.Chords.size() > MaximumHarmonyEvents)
        {
            harmony.Chords.resize(MaximumHarmonyEvents);
        }

        for (auto& key : harmony.Keys)
        {
            key.Tick = std::clamp<int64_t>(key.Tick, 0, MaximumTick);
            key.Root = static_cast<uint8_t>(key.Root % 12);

            if (static_cast<size_t>(key.Scale) >= ScaleCount)
            {
                key.Scale = ScaleKind::Major;
            }
        }

        std::stable_sort(harmony.Keys.begin(), harmony.Keys.end(),
            [](KeyChange const& a, KeyChange const& b) { return a.Tick < b.Tick; });

        std::vector<KeyChange> keys{};
        keys.reserve(harmony.Keys.size());

        for (auto const& key : harmony.Keys)
        {
            if (!keys.empty() && keys.back().Tick == key.Tick)
            {
                keys.back() = key;
            }
            else
            {
                keys.push_back(key);
            }
        }

        harmony.Keys = std::move(keys);

        for (auto& chord : harmony.Chords)
        {
            CleanChord(chord);
        }

        std::stable_sort(harmony.Chords.begin(), harmony.Chords.end(),
            [](ChordEvent const& a, ChordEvent const& b) { return a.Tick < b.Tick; });

        std::vector<ChordEvent> chords{};
        chords.reserve(harmony.Chords.size());

        for (auto const& chord : harmony.Chords)
        {
            if (!chords.empty() && chords.back().Tick + chords.back().Length > chord.Tick)
            {
                if (chords.back().Tick == chord.Tick)
                {
                    chords.pop_back();
                }
                else
                {
                    chords.back().Length = chord.Tick - chords.back().Tick;
                }
            }

            chords.push_back(chord);
        }

        harmony.Chords = std::move(chords);
    }

    _Use_decl_annotations_
    size_t PlaceChord(HarmonyTrack& harmony, ChordEvent chord)
    {
        CleanChord(chord);

        auto const start = chord.Tick;
        auto const end = chord.Tick + chord.Length;

        std::vector<ChordEvent> chords{};
        chords.reserve(harmony.Chords.size() + 2);

        for (auto const& existing : harmony.Chords)
        {
            auto const existingEnd = existing.Tick + existing.Length;

            if (existingEnd <= start || existing.Tick >= end)
            {
                chords.push_back(existing);
            }
            else if (existing.Tick < start && existingEnd > end)
            {
                auto left = existing;
                left.Length = start - existing.Tick;
                chords.push_back(left);

                auto right = existing;
                right.Tick = end;
                right.Length = existingEnd - end;
                chords.push_back(right);
            }
            else if (existing.Tick < start)
            {
                auto left = existing;
                left.Length = start - existing.Tick;
                chords.push_back(left);
            }
            else if (existingEnd > end)
            {
                auto right = existing;
                right.Tick = end;
                right.Length = existingEnd - end;
                chords.push_back(right);
            }
        }

        auto const at = std::upper_bound(chords.begin(), chords.end(), start,
            [](int64_t t, ChordEvent const& c) { return t < c.Tick; });

        auto const index = static_cast<size_t>(at - chords.begin());
        chords.insert(at, chord);

        harmony.Chords = std::move(chords);
        return index;
    }

    _Use_decl_annotations_
    void TransposeChords(HarmonyTrack& harmony, int64_t fromTick, int64_t toTick, int semitones)
    {
        auto const shift = ((semitones % 12) + 12) % 12;

        for (auto& chord : harmony.Chords)
        {
            if (chord.Tick < fromTick || chord.Tick >= toTick)
            {
                continue;
            }

            chord.Root = static_cast<uint8_t>((chord.Root % 12 + shift) % 12);

            if (chord.Bass)
            {
                chord.Bass = static_cast<uint8_t>((*chord.Bass % 12 + shift) % 12);
            }
        }
    }

    _Use_decl_annotations_
    std::unique_ptr<SequenceChange> MakeHarmonyChange(std::optional<HarmonyTrack> before, std::optional<HarmonyTrack> after)
    {
        auto const cost = [](std::optional<HarmonyTrack> const& harmony)
        {
            size_t bytes{ sizeof(HarmonyTrack) };

            if (harmony)
            {
                bytes += harmony->Keys.size() * sizeof(KeyChange) + harmony->Chords.size() * sizeof(ChordEvent);
            }

            return bytes;
        };

        auto const total = cost(before) + cost(after);

        return std::make_unique<SnapshotChange<std::optional<HarmonyTrack>>>(
            [](Sequence& sequence) { return &sequence.Harmony; },
            std::move(before),
            std::move(after),
            total);
    }

    // ---- the sequence file ----

    _Use_decl_annotations_
    std::wstring HarmonyToJsonText(HarmonyTrack const& harmony)
    {
        std::wstring out{ L"{" };
        bool first{ true };

        auto const member = [&](std::wstring_view key)
        {
            out += first ? L"\n    " : L",\n    ";
            first = false;
            out += Quote(key);
            out += L": ";
        };

        if (harmony.Hidden)
        {
            member(KeyHidden);
            out += L"true";
        }

        member(KeyKeys);
        out += L"[";

        for (size_t i = 0; i < harmony.Keys.size(); ++i)
        {
            auto const& key = harmony.Keys[i];

            out += i == 0 ? L"\n      " : L",\n      ";
            out += std::format(L"{{ {}: {}, {}: {}, {}: {} }}",
                Quote(KeyTick), key.Tick,
                Quote(KeyRoot), Quote(NoteName(key.Root, KeyUsesFlats(key.Root, key.Scale))),
                Quote(KeyScale), Quote(ScaleId(key.Scale)));
        }

        out += harmony.Keys.empty() ? L"]" : L"\n    ]";

        member(KeyChords);
        out += L"[";

        for (size_t i = 0; i < harmony.Chords.size(); ++i)
        {
            auto const& chord = harmony.Chords[i];

            out += i == 0 ? L"\n      " : L",\n      ";
            out += std::format(L"{{ {}: {}, {}: {}, {}: {} }}",
                Quote(KeyTick), chord.Tick,
                Quote(KeyLength), chord.Length,
                Quote(KeyChord), Quote(ChordName(chord, KeyAt(harmony, chord.Tick))));
        }

        out += harmony.Chords.empty() ? L"]" : L"\n    ]";

        if (harmony.Unknown != nullptr)
        {
            std::vector<std::pair<std::wstring, std::wstring>> pairs{};

            for (auto const& pair : harmony.Unknown)
            {
                pairs.emplace_back(std::wstring{ pair.Key() }, std::wstring{ pair.Value().Stringify() });
            }

            std::sort(pairs.begin(), pairs.end());

            for (auto const& [key, value] : pairs)
            {
                member(key);
                out += value;
            }
        }

        out += L"\n  }";
        return out;
    }

    _Use_decl_annotations_
    std::optional<HarmonyTrack> ReadHarmonyJson(json::JsonObject const& root, size_t& skipped)
    {
        if (root == nullptr || !root.HasKey(HarmonyKey))
        {
            return std::nullopt;
        }

        auto const value = root.Lookup(HarmonyKey);

        if (value == nullptr || value.ValueType() != json::JsonValueType::Object)
        {
            ++skipped;
            return std::nullopt;
        }

        auto const object = value.GetObject();

        HarmonyTrack harmony{};

        if (object.HasKey(KeyHidden))
        {
            auto const hidden = object.Lookup(KeyHidden);

            if (hidden != nullptr && hidden.ValueType() == json::JsonValueType::Boolean)
            {
                harmony.Hidden = hidden.GetBoolean();
            }
            else
            {
                ++skipped;
            }
        }

        if (auto const keys = ArrayMember(object, KeyKeys, skipped); keys != nullptr)
        {
            for (auto const& entry : keys)
            {
                if (harmony.Keys.size() >= MaximumHarmonyEvents || entry.ValueType() != json::JsonValueType::Object)
                {
                    ++skipped;
                    continue;
                }

                auto const fields = entry.GetObject();
                auto const tick = NumberMember(fields, KeyTick);
                auto const rootName = StringMember(fields, KeyRoot);
                auto const scaleId = StringMember(fields, KeyScale);
                auto const pitch = rootName ? ParseNoteName(*rootName) : std::nullopt;
                auto const scale = scaleId ? ScaleFromId(*scaleId) : std::nullopt;

                if (!tick || !pitch || !scale)
                {
                    ++skipped;
                    continue;
                }

                harmony.Keys.push_back(KeyChange{ TickFrom(*tick), *pitch, *scale });
            }
        }

        if (auto const chords = ArrayMember(object, KeyChords, skipped); chords != nullptr)
        {
            for (auto const& entry : chords)
            {
                if (harmony.Chords.size() >= MaximumHarmonyEvents || entry.ValueType() != json::JsonValueType::Object)
                {
                    ++skipped;
                    continue;
                }

                auto const fields = entry.GetObject();
                auto const tick = NumberMember(fields, KeyTick);
                auto const length = NumberMember(fields, KeyLength);
                auto const name = StringMember(fields, KeyChord);
                auto const parsed = name ? ParseChordName(*name) : std::nullopt;

                if (!tick || !length || !parsed || parsed->NoChord)
                {
                    ++skipped;
                    continue;
                }

                auto chord = parsed->Chord;
                chord.Tick = TickFrom(*tick);
                chord.Length = std::max<int64_t>(1, TickFrom(*length));
                harmony.Chords.push_back(chord);
            }
        }

        for (auto const& pair : object)
        {
            std::wstring_view const key{ pair.Key() };

            if (key == KeyHidden || key == KeyKeys || key == KeyChords)
            {
                continue;
            }

            if (harmony.Unknown == nullptr)
            {
                harmony.Unknown = json::JsonObject{};
            }

            harmony.Unknown.SetNamedValue(pair.Key(), pair.Value());
        }

        NormalizeHarmony(harmony);
        return harmony;
    }
}
