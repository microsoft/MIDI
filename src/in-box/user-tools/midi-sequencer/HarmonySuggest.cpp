// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#include "HarmonySuggest.h"
#include "Harmony.h"

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

// windows.h renames GetObject, which Windows.Data.Json uses.
#undef GetObject

#include <algorithm>
#include <array>

namespace midisequencer
{
    namespace
    {
        // A chord by where its root sits above the key's root, in semitones.
        struct Degree
        {
            uint8_t Offset{ 0 };
            ChordType Type{ ChordType::Major };
        };

        constexpr size_t MaximumSuggestions = 5;

        // Major keys.
        constexpr Degree MajorI{ 0, ChordType::Major };
        constexpr Degree MajorII{ 2, ChordType::Minor };
        constexpr Degree MajorIII{ 4, ChordType::Minor };
        constexpr Degree MajorIV{ 5, ChordType::Major };
        constexpr Degree MajorV{ 7, ChordType::Major };
        constexpr Degree MajorVI{ 9, ChordType::Minor };
        constexpr Degree MajorVII{ 11, ChordType::Diminished };

        // Minor keys, with the major V7 that minor keys borrow from harmonic minor.
        constexpr Degree MinorI{ 0, ChordType::Minor };
        constexpr Degree MinorII{ 2, ChordType::Diminished };
        constexpr Degree MinorIII{ 3, ChordType::Major };
        constexpr Degree MinorIV{ 5, ChordType::Minor };
        constexpr Degree MinorV{ 7, ChordType::Minor };
        constexpr Degree MinorV7{ 7, ChordType::Seventh };
        constexpr Degree MinorVI{ 8, ChordType::Major };
        constexpr Degree MinorVII{ 10, ChordType::Major };

        struct NextList
        {
            uint8_t After{ 0 };                 // the offset of the chord before
            std::array<Degree, 5> Next{};
            size_t Count{ 0 };
        };

        // After each chord in a major key: the next job along first.
        constexpr std::array<NextList, 7> MajorNext{ {
            { 0, { MajorIV, MajorV, MajorVI, MajorII }, 4 },
            { 2, { MajorV, MajorVII, MajorIV }, 3 },
            { 4, { MajorVI, MajorIV }, 2 },
            { 5, { MajorV, MajorI, MajorII }, 3 },
            { 7, { MajorI, MajorVI, MajorIV }, 3 },
            { 9, { MajorIV, MajorII, MajorV }, 3 },
            { 11, { MajorI, MajorVI }, 2 },
        } };

        constexpr std::array<NextList, 7> MinorNext{ {
            { 0, { MinorIV, MinorVI, MinorVII, MinorV7, MinorIII }, 5 },
            { 2, { MinorV7, MinorV, MinorI }, 3 },
            { 3, { MinorVI, MinorIV, MinorVII }, 3 },
            { 5, { MinorV7, MinorI, MinorVII }, 3 },
            { 7, { MinorI, MinorVI }, 2 },
            { 8, { MinorVII, MinorIV, MinorII, MinorIII }, 4 },
            { 10, { MinorIII, MinorI }, 2 },
        } };

        // Places to start, and what tops up a short list.
        constexpr std::array<Degree, 5> MajorStart{ MajorI, MajorIV, MajorV, MajorVI, MajorII };
        constexpr std::array<Degree, 5> MinorStart{ MinorI, MinorIV, MinorVI, MinorVII, MinorV7 };

        bool IsMinorKey(KeyChange const& key) noexcept
        {
            return TonicChordType(key.Scale) == ChordType::Minor;
        }

        // Pentatonic and blues scales leave out notes that the chords around them still use, so a
        // chord is judged against the full scale they come from.
        KeyChange KeyForBorrowing(KeyChange key) noexcept
        {
            switch (key.Scale)
            {
            case ScaleKind::MajorPentatonic:
                key.Scale = ScaleKind::Major;
                break;
            case ScaleKind::MinorPentatonic:
            case ScaleKind::Blues:
                key.Scale = ScaleKind::NaturalMinor;
                break;
            default:
                break;
            }

            return key;
        }

        uint8_t OffsetOf(uint8_t root, KeyChange const& key) noexcept
        {
            return static_cast<uint8_t>(((root % 12) + 12 - (key.Root % 12)) % 12);
        }

        // The chord a key has on a degree, with the major V in a minor key.
        std::optional<ChordType> DiatonicType(uint8_t offset, bool minor) noexcept
        {
            for (auto const degree : minor
                ? std::array<Degree, 7>{ MinorI, MinorII, MinorIII, MinorIV, Degree{ 7, ChordType::Major }, MinorVI, MinorVII }
                : std::array<Degree, 7>{ MajorI, MajorII, MajorIII, MajorIV, MajorV, MajorVI, MajorVII })
            {
                if (degree.Offset == offset)
                {
                    return degree.Type;
                }
            }

            return std::nullopt;
        }

        ChordSuggestion MakeSuggestion(uint8_t root, ChordType type, KeyChange const& key)
        {
            ChordSuggestion suggestion{};
            suggestion.Root = static_cast<uint8_t>(root % 12);
            suggestion.Type = type;

            ChordEvent chord{};
            chord.Root = suggestion.Root;
            chord.Type = type;
            suggestion.Borrowed = !ChordIsInKey(chord, KeyForBorrowing(key));
            return suggestion;
        }

        ChordSuggestion FromDegree(Degree degree, KeyChange const& key)
        {
            return MakeSuggestion(static_cast<uint8_t>(key.Root + degree.Offset), degree.Type, key);
        }

        bool Same(ChordSuggestion const& suggestion, ChordEvent const* chord) noexcept
        {
            return chord != nullptr && suggestion.Root == chord->Root % 12 && suggestion.Type == chord->Type;
        }

        void AddUnique(std::vector<ChordSuggestion>& list, ChordSuggestion const& suggestion, ChordEvent const* previous)
        {
            if (Same(suggestion, previous))
            {
                return;
            }

            if (std::none_of(list.begin(), list.end(), [&](ChordSuggestion const& s) { return s.Root == suggestion.Root && s.Type == suggestion.Type; }))
            {
                list.push_back(suggestion);
            }
        }

        // Puts a chord at the front, adding it when it isn't there.
        void MoveToFront(std::vector<ChordSuggestion>& list, ChordSuggestion const& suggestion, ChordEvent const* previous)
        {
            if (Same(suggestion, previous))
            {
                return;
            }

            auto found = std::find_if(list.begin(), list.end(), [&](ChordSuggestion const& s) { return s.Root == suggestion.Root && s.Type == suggestion.Type; });

            if (found != list.end())
            {
                list.erase(found);
            }

            list.insert(list.begin(), suggestion);
        }

        struct Progression
        {
            std::array<Degree, 12> Major{};
            std::array<Degree, 12> Minor{};
            size_t Count{ 0 };
        };

        constexpr ChordType Maj = ChordType::Major;
        constexpr ChordType Min = ChordType::Minor;
        constexpr ChordType Dom7 = ChordType::Seventh;
        constexpr ChordType Maj7 = ChordType::Major7;
        constexpr ChordType Min7 = ChordType::Minor7;
        constexpr ChordType Dim = ChordType::Diminished;
        constexpr ChordType HalfDim = ChordType::HalfDiminished;

        // Each progression as it goes in a major key and in a minor key. Most are the same in both:
        // a progression that's written for one kind of key keeps its sound in the other, with the
        // borrowed chords marked. Jazz and the circle of fifths have a minor form of their own.
        constexpr std::array<Progression, ProgressionCount> Progressions{ {
            // I–V–vi–IV
            { { { { 0, Maj }, { 7, Maj }, { 9, Min }, { 5, Maj } } },
              { { { 0, Maj }, { 7, Maj }, { 9, Min }, { 5, Maj } } }, 4 },
            // vi–IV–I–V
            { { { { 9, Min }, { 5, Maj }, { 0, Maj }, { 7, Maj } } },
              { { { 9, Min }, { 5, Maj }, { 0, Maj }, { 7, Maj } } }, 4 },
            // I–vi–IV–V
            { { { { 0, Maj }, { 9, Min }, { 5, Maj }, { 7, Maj } } },
              { { { 0, Maj }, { 9, Min }, { 5, Maj }, { 7, Maj } } }, 4 },
            // ii7–V7–Imaj7, and iiø7–V7–im7
            { { { { 2, Min7 }, { 7, Dom7 }, { 0, Maj7 } } },
              { { { 2, HalfDim }, { 7, Dom7 }, { 0, Min7 } } }, 3 },
            // Imaj7–vi7–ii7–V7, and im7–VImaj7–iiø7–V7
            { { { { 0, Maj7 }, { 9, Min7 }, { 2, Min7 }, { 7, Dom7 } } },
              { { { 0, Min7 }, { 8, Maj7 }, { 2, HalfDim }, { 7, Dom7 } } }, 4 },
            // 12-bar blues
            { { { { 0, Maj }, { 0, Maj }, { 0, Maj }, { 0, Maj }, { 5, Maj }, { 5, Maj }, { 0, Maj }, { 0, Maj }, { 7, Maj }, { 5, Maj }, { 0, Maj }, { 7, Maj } } },
              { { { 0, Min }, { 0, Min }, { 0, Min }, { 0, Min }, { 5, Min }, { 5, Min }, { 0, Min }, { 0, Min }, { 7, Dom7 }, { 5, Min }, { 0, Min }, { 7, Dom7 } } }, 12 },
            // 12-bar blues, all 7ths
            { { { { 0, Dom7 }, { 0, Dom7 }, { 0, Dom7 }, { 0, Dom7 }, { 5, Dom7 }, { 5, Dom7 }, { 0, Dom7 }, { 0, Dom7 }, { 7, Dom7 }, { 5, Dom7 }, { 0, Dom7 }, { 7, Dom7 } } },
              { { { 0, Min7 }, { 0, Min7 }, { 0, Min7 }, { 0, Min7 }, { 5, Min7 }, { 5, Min7 }, { 0, Min7 }, { 0, Min7 }, { 7, Dom7 }, { 5, Min7 }, { 0, Min7 }, { 7, Dom7 } } }, 12 },
            // i–VI–III–VII
            { { { { 0, Min }, { 8, Maj }, { 3, Maj }, { 10, Maj } } },
              { { { 0, Min }, { 8, Maj }, { 3, Maj }, { 10, Maj } } }, 4 },
            // i–iv–v
            { { { { 0, Min }, { 5, Min }, { 7, Min } } },
              { { { 0, Min }, { 5, Min }, { 7, Min } } }, 3 },
            // Andalusian: i–VII–VI–V
            { { { { 0, Min }, { 10, Maj }, { 8, Maj }, { 7, Maj } } },
              { { { 0, Min }, { 10, Maj }, { 8, Maj }, { 7, Maj } } }, 4 },
            // Canon: I–V–vi–iii–IV–I–IV–V
            { { { { 0, Maj }, { 7, Maj }, { 9, Min }, { 4, Min }, { 5, Maj }, { 0, Maj }, { 5, Maj }, { 7, Maj } } },
              { { { 0, Maj }, { 7, Maj }, { 9, Min }, { 4, Min }, { 5, Maj }, { 0, Maj }, { 5, Maj }, { 7, Maj } } }, 8 },
            // Circle of fifths: I–IV–vii°–iii–vi–ii–V–I, and i–iv–VII–III–VI–ii°–V–i
            { { { { 0, Maj }, { 5, Maj }, { 11, Dim }, { 4, Min }, { 9, Min }, { 2, Min }, { 7, Maj }, { 0, Maj } } },
              { { { 0, Min }, { 5, Min }, { 10, Maj }, { 3, Maj }, { 8, Maj }, { 2, Dim }, { 7, Maj }, { 0, Min } } }, 8 },
        } };

        int64_t StepTicks(ProgressionRate rate, int64_t bar) noexcept
        {
            switch (rate)
            {
            case ProgressionRate::TwoPerBar:
                return std::max<int64_t>(1, bar / 2);
            case ProgressionRate::OnePerTwoBars:
                return bar * 2;
            case ProgressionRate::OnePerBar:
            default:
                return bar;
            }
        }

        int64_t BarStart(std::vector<MeterChange> const& meter, int64_t tick) noexcept
        {
            return TickAtBar(meter, BarPositionAtTick(meter, tick).Bar);
        }

        int64_t BarLength(std::vector<MeterChange> const& meter, int64_t tick) noexcept
        {
            return std::max<int64_t>(1, TicksPerBar(MeterAtTick(meter, tick)));
        }

        // Counted from 0, for phrases.
        int64_t PhraseBar(std::vector<MeterChange> const& meter, int64_t tick) noexcept
        {
            return BarPositionAtTick(meter, tick).Bar - 1;
        }
    }

    _Use_decl_annotations_
    std::vector<ChordSuggestion> SuggestNextChords(ChordEvent const* previous, KeyChange const& key, int64_t phraseBar)
    {
        auto const minor = IsMinorKey(key);
        auto const& lists = minor ? MinorNext : MajorNext;
        auto const& start = minor ? MinorStart : MajorStart;

        std::vector<ChordSuggestion> result{};

        if (previous != nullptr)
        {
            // A dominant chord (V7, or a secondary dominant like A7 in C) pulls to the chord a
            // fifth below it, as the key has it on that degree. That goes first, whatever degree
            // the dominant itself is on.
            if (previous->Type == ChordType::Seventh || previous->Type == ChordType::Ninth || previous->Type == ChordType::Thirteenth || previous->Type == ChordType::SeventhSus4)
            {
                auto const target = static_cast<uint8_t>((previous->Root + 5) % 12);

                if (auto const type = DiatonicType(OffsetOf(target, key), minor); type.has_value())
                {
                    AddUnique(result, MakeSuggestion(target, *type, key), previous);
                }
            }

            auto const offset = OffsetOf(previous->Root, key);
            auto const found = std::find_if(lists.begin(), lists.end(), [offset](NextList const& list) { return list.After == offset; });

            if (found != lists.end())
            {
                for (size_t i = 0; i < found->Count; ++i)
                {
                    AddUnique(result, FromDegree(found->Next[i], key), previous);
                }
            }
        }

        for (auto const& degree : start)
        {
            if (result.size() >= MaximumSuggestions)
            {
                break;
            }

            AddUnique(result, FromDegree(degree, key), previous);
        }

        if (phraseBar >= 0)
        {
            auto const tonic = FromDegree(minor ? MinorI : MajorI, key);
            auto const dominant = FromDegree(minor ? MinorV7 : MajorV, key);

            if (phraseBar % 8 == 7)
            {
                MoveToFront(result, dominant, previous);
                MoveToFront(result, tonic, previous);
            }
            else if (phraseBar % 4 == 3)
            {
                MoveToFront(result, tonic, previous);
                MoveToFront(result, dominant, previous);
            }
        }

        if (result.size() > MaximumSuggestions)
        {
            result.resize(MaximumSuggestions);
        }

        return result;
    }

    _Use_decl_annotations_
    std::vector<ChordSuggestion> MoreChordSuggestions(ChordEvent const* previous, KeyChange const& key, int64_t phraseBar)
    {
        auto const minor = IsMinorKey(key);
        auto const next = SuggestNextChords(previous, key, phraseBar);

        std::vector<ChordSuggestion> result{};

        if (minor)
        {
            AddUnique(result, FromDegree(Degree{ 5, ChordType::Major }, key), previous);
            AddUnique(result, FromDegree(Degree{ 1, ChordType::Major }, key), previous);
        }
        else
        {
            AddUnique(result, FromDegree(Degree{ 5, ChordType::Minor }, key), previous);
            AddUnique(result, FromDegree(Degree{ 10, ChordType::Major }, key), previous);
        }

        // V7 of the first suggestion, when that isn't already the key's V7.
        if (!next.empty())
        {
            auto const secondary = MakeSuggestion(static_cast<uint8_t>(next.front().Root + 7), ChordType::Seventh, key);

            if (OffsetOf(next.front().Root, key) != 0)
            {
                AddUnique(result, secondary, previous);
            }
        }

        AddUnique(result, FromDegree(Degree{ 7, ChordType::Sus4 }, key), previous);
        return result;
    }

    _Use_decl_annotations_
    std::vector<ChordSuggestion> ProgressionChords(ProgressionId id, KeyChange const& key)
    {
        auto const index = static_cast<size_t>(id);

        if (index >= Progressions.size())
        {
            return {};
        }

        auto const& progression = Progressions[index];
        auto const& degrees = IsMinorKey(key) ? progression.Minor : progression.Major;

        std::vector<ChordSuggestion> result{};
        result.reserve(progression.Count);

        for (size_t i = 0; i < progression.Count; ++i)
        {
            result.push_back(FromDegree(degrees[i], key));
        }

        return result;
    }

    _Use_decl_annotations_
    std::vector<ChordEvent> LayOutProgression(
        ProgressionId id,
        KeyChange const& key,
        std::vector<MeterChange> const& meter,
        int64_t fromTick,
        ProgressionRate rate)
    {
        std::vector<ChordEvent> result{};
        auto tick = std::max<int64_t>(0, fromTick);

        for (auto const& suggestion : ProgressionChords(id, key))
        {
            auto const barStart = BarStart(meter, tick);
            auto const step = StepTicks(rate, BarLength(meter, tick));

            // The next step boundary after tick, counted from the start of its bar.
            auto const end = barStart + ((tick - barStart) / step + 1) * step;

            ChordEvent chord{};
            chord.Tick = tick;
            chord.Length = std::max<int64_t>(1, end - tick);
            chord.Root = suggestion.Root;
            chord.Type = suggestion.Type;
            result.push_back(chord);

            tick = end;
        }

        return result;
    }

    _Use_decl_annotations_
    std::vector<ChordEvent> SuggestChordsToFill(
        HarmonyTrack const& harmony,
        std::vector<MeterChange> const& meter,
        int64_t fromTick)
    {
        constexpr size_t MaximumFill = 512;

        auto tick = std::max<int64_t>(0, fromTick);
        std::optional<ChordEvent> previous{};

        // Starting inside a chord starts after it.
        for (auto const& chord : harmony.Chords)
        {
            if (chord.Tick <= tick && tick < chord.Tick + chord.Length)
            {
                tick = chord.Tick + chord.Length;
                previous = chord;
            }
            else if (chord.Tick + chord.Length <= tick)
            {
                previous = chord;
            }
        }

        if (KeyAt(harmony, tick) == nullptr)
        {
            return {};
        }

        // Up to the next chord, or the end of this four-bar phrase.
        int64_t limit{ INT64_MAX };

        for (auto const& chord : harmony.Chords)
        {
            if (chord.Tick >= tick)
            {
                limit = chord.Tick;
                break;
            }
        }

        if (limit == INT64_MAX)
        {
            auto const bar = PhraseBar(meter, tick);
            limit = TickAtBar(meter, (bar / 4 + 1) * 4 + 1);
        }

        std::vector<ChordEvent> result{};

        while (tick < limit && result.size() < MaximumFill)
        {
            auto const key = KeyAt(harmony, tick);
            auto const end = std::min(limit, BarStart(meter, tick) + BarLength(meter, tick));

            if (key == nullptr || end <= tick)
            {
                break;
            }

            auto const suggestions = SuggestNextChords(previous ? &*previous : nullptr, *key, PhraseBar(meter, tick));

            if (suggestions.empty())
            {
                break;
            }

            ChordEvent chord{};
            chord.Tick = tick;
            chord.Length = end - tick;
            chord.Root = suggestions.front().Root;
            chord.Type = suggestions.front().Type;
            result.push_back(chord);

            previous = chord;
            tick = end;
        }

        return result;
    }
}
