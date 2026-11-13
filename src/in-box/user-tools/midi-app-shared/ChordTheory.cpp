// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

// Deliberately free of pch.h, WinRT and XAML, so the unit tests compile it unchanged.

#include "ChordTheory.h"

#include <algorithm>
#include <array>
#include <cstdlib>
#include <utility>

namespace midiapp
{
    namespace
    {
        constexpr wchar_t Sharp = L'\u266F';
        constexpr wchar_t Flat = L'\u266D';

        // The same steps the Sequencer's generators use (SequenceGenerator.cpp).
        struct ScaleSteps
        {
            std::array<uint8_t, 12> Steps{};
            size_t Count{ 0 };
        };

        constexpr std::array<ScaleSteps, ScaleKindCount> Scales{ {
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
        constexpr std::array<uint8_t, ScaleKindCount> ParentMajorOffsets{ 0, 3, 10, 8, 7, 5, 3, 0, 3, 3, 0 };

        constexpr std::array<std::wstring_view, ScaleKindCount> ScaleIds{
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

        // Pentatonic, blues and chromatic keys count degrees like the major or natural minor
        // scale they sound like.
        ScaleKind ReferenceScale(_In_ ScaleKind scale) noexcept
        {
            if (scale == ScaleKind::MajorPentatonic || scale == ScaleKind::Chromatic)
            {
                return ScaleKind::Major;
            }

            if (scale == ScaleKind::MinorPentatonic || scale == ScaleKind::Blues)
            {
                return ScaleKind::NaturalMinor;
            }

            return scale;
        }

        // The numeral for a note in a key, with ♭ or ♯ before it when the note isn't the scale's
        // own degree.
        std::pair<std::wstring, size_t> Degree(_In_ uint8_t pitchClass, _In_ MusicKey const& key)
        {
            auto const& steps = StepsOf(ReferenceScale(key.Scale));
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

        // The type whose notes are exactly these intervals above a root, if one is.
        std::optional<ChordType> TypeOfIntervals(_In_ std::vector<uint8_t> intervals)
        {
            std::sort(intervals.begin(), intervals.end());

            for (size_t i = 0; i < ChordTypeCount; ++i)
            {
                auto const type = static_cast<ChordType>(i);
                auto shape = ChordIntervals(type);

                for (auto& interval : shape)
                {
                    interval %= 12;
                }

                std::sort(shape.begin(), shape.end());
                shape.erase(std::unique(shape.begin(), shape.end()), shape.end());

                if (shape == intervals)
                {
                    return type;
                }
            }

            return std::nullopt;
        }
    }

    // ---- names ----

    _Use_decl_annotations_
    bool KeyUsesFlats(uint8_t root, ScaleKind scale) noexcept
    {
        auto const parent = ParentMajorRoot({ root, scale });

        // F, B♭, E♭, A♭, D♭ and G♭ major.
        return parent == 5 || parent == 10 || parent == 3 || parent == 8 || parent == 1 || parent == 6;
    }

    _Use_decl_annotations_
    uint8_t ParentMajorRoot(MusicKey const& key) noexcept
    {
        auto const index = static_cast<size_t>(key.Scale) < ScaleKindCount ? static_cast<size_t>(key.Scale) : 0;
        return static_cast<uint8_t>((key.Root % 12 + ParentMajorOffsets[index]) % 12);
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
    std::wstring NoteNameInKey(uint8_t pitchClass, MusicKey const* key)
    {
        auto flats = key != nullptr ? KeyUsesFlats(key->Root, key->Scale) : DefaultFlat(pitchClass % 12);

        if (key != nullptr)
        {
            auto const [prefix, degree] = Degree(pitchClass, *key);

            if (!prefix.empty())
            {
                flats = prefix.front() == Flat;
            }
        }

        return NoteName(pitchClass, flats);
    }

    _Use_decl_annotations_
    std::wstring ChordTypeSuffix(ChordType type)
    {
        return std::wstring{ ShapeOf(type).Suffix };
    }

    _Use_decl_annotations_
    std::wstring ChordName(ChordSymbol const& chord, MusicKey const* key)
    {
        auto name = NoteNameInKey(chord.Root, key) + ChordTypeSuffix(chord.Type);

        if (chord.Bass && *chord.Bass % 12 != chord.Root % 12)
        {
            name += L"/";
            name += NoteNameInKey(*chord.Bass, key);
        }

        return name;
    }

    _Use_decl_annotations_
    std::optional<uint8_t> ParseNoteName(std::wstring_view text) noexcept
    {
        text = Trim(text);
        auto const pitch = ReadNoteName(text);

        if (!pitch || !text.empty())
        {
            return std::nullopt;
        }

        return pitch;
    }

    _Use_decl_annotations_
    std::optional<ParsedChordSymbol> ParseChordName(std::wstring_view text)
    {
        text = Trim(text);

        if (text == L"N.C." || text == L"N.C" || text == L"NC")
        {
            return ParsedChordSymbol{ true, ChordSymbol{} };
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

        ParsedChordSymbol parsed{};
        parsed.Chord.Root = *root;
        parsed.Chord.Type = *type;

        if (bass && *bass != *root)
        {
            parsed.Chord.Bass = bass;
        }

        return parsed;
    }

    _Use_decl_annotations_
    std::wstring RomanNumeral(ChordSymbol const& chord, MusicKey const& key)
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
    bool ChordIsInKey(ChordSymbol const& chord, MusicKey const& key)
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
    std::vector<uint8_t> ChordIntervals(ChordType type)
    {
        auto const& shape = ShapeOf(type);
        return std::vector<uint8_t>(shape.Intervals.begin(), shape.Intervals.begin() + static_cast<ptrdiff_t>(shape.Count));
    }

    _Use_decl_annotations_
    std::vector<uint8_t> ChordPitchClasses(ChordSymbol const& chord)
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
    std::vector<uint8_t> ChordNotes(ChordSymbol const& chord)
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

    _Use_decl_annotations_
    std::vector<uint8_t> ScalePitchClasses(MusicKey const& key)
    {
        auto const& steps = StepsOf(key.Scale);
        std::vector<uint8_t> pitchClasses{};

        for (size_t i = 0; i < steps.Count; ++i)
        {
            pitchClasses.push_back(static_cast<uint8_t>((key.Root % 12 + steps.Steps[i]) % 12));
        }

        return pitchClasses;
    }

    _Use_decl_annotations_
    std::vector<ChordSymbol> ChordsInKey(MusicKey const& key, bool sevenths)
    {
        auto const& steps = StepsOf(ReferenceScale(key.Scale));
        std::vector<ChordSymbol> chords{};

        for (size_t degree = 0; degree < steps.Count; ++degree)
        {
            // every other scale note above this one, as semitones above it
            auto const above = [&](size_t count)
            {
                auto const index = degree + count;
                auto const semitones = steps.Steps[index % steps.Count] + (index >= steps.Count ? 12 : 0) - steps.Steps[degree];
                return static_cast<uint8_t>(semitones % 12);
            };

            std::vector<uint8_t> triad{ 0, above(2), above(4) };
            auto type = TypeOfIntervals(triad);

            if (sevenths)
            {
                auto seventh = triad;
                seventh.push_back(above(6));

                if (auto const fourNotes = TypeOfIntervals(seventh))
                {
                    type = fourNotes;
                }
            }

            ChordSymbol chord{};
            chord.Root = static_cast<uint8_t>((key.Root % 12 + steps.Steps[degree]) % 12);
            chord.Type = type.value_or(ChordType::Major);
            chords.push_back(chord);
        }

        return chords;
    }

    _Use_decl_annotations_
    std::vector<ChordSymbol> NameChords(std::vector<uint8_t> const& notes)
    {
        if (notes.empty())
        {
            return {};
        }

        auto const lowest = static_cast<uint8_t>(*std::min_element(notes.begin(), notes.end()) % 12);

        std::vector<uint8_t> pitchClasses{};

        for (auto const note : notes)
        {
            pitchClasses.push_back(static_cast<uint8_t>(note % 12));
        }

        std::sort(pitchClasses.begin(), pitchClasses.end());
        pitchClasses.erase(std::unique(pitchClasses.begin(), pitchClasses.end()), pitchClasses.end());

        if (pitchClasses.size() < 2)
        {
            return {};
        }

        struct Candidate
        {
            ChordSymbol Chord{};
            size_t Notes{ 0 };
        };

        std::vector<Candidate> found{};

        for (uint8_t root = 0; root < 12; ++root)
        {
            for (size_t i = 0; i < ChordTypeCount; ++i)
            {
                ChordSymbol chord{};
                chord.Root = root;
                chord.Type = static_cast<ChordType>(i);

                auto chordClasses = ChordPitchClasses(chord);
                std::sort(chordClasses.begin(), chordClasses.end());

                if (chordClasses != pitchClasses)
                {
                    continue;
                }

                if (lowest != root)
                {
                    chord.Bass = lowest;
                }

                found.push_back({ chord, ChordIntervals(chord.Type).size() });
            }
        }

        std::stable_sort(found.begin(), found.end(), [](Candidate const& a, Candidate const& b)
            {
                auto const aOver = a.Chord.Bass.has_value();
                auto const bOver = b.Chord.Bass.has_value();

                if (aOver != bOver)
                {
                    return !aOver;
                }

                return a.Notes < b.Notes;
            });

        std::vector<ChordSymbol> chords{};

        for (auto const& candidate : found)
        {
            chords.push_back(candidate.Chord);
        }

        return chords;
    }
}
