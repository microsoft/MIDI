// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#include "HarmonyFollow.h"
#include "Harmony.h"
#include "SequenceGenerator.h"

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

// windows.h renames GetObject, which Windows.Data.Json uses.
#undef GetObject

#include <algorithm>
#include <climits>
#include <cstdlib>

namespace midisequencer
{
    namespace
    {
        // The semitones of a scale above its root, lowest first.
        std::vector<uint8_t> ScaleSteps(ScaleKind scale)
        {
            auto steps = ScaleNotes(0, scale, 0, 11);

            if (steps.empty() || steps.front() != 0)
            {
                steps.insert(steps.begin(), 0);
            }

            return steps;
        }

        bool SameKey(KeyChange const& a, KeyChange const& b) noexcept
        {
            return a.Root % 12 == b.Root % 12 && a.Scale == b.Scale;
        }
    }

    _Use_decl_annotations_
    uint8_t MoveByScaleSteps(uint8_t note, KeyChange const& from, KeyChange const& to) noexcept
    {
        try
        {
            if (SameKey(from, to))
            {
                return note;
            }

            auto const fromSteps = ScaleSteps(from.Scale);
            auto const toSteps = ScaleSteps(to.Scale);

            auto const relative = static_cast<int>(note) - static_cast<int>(from.Root % 12);
            auto const octave = relative >= 0 ? relative / 12 : -((-relative + 11) / 12);
            auto const within = relative - octave * 12;

            // The scale step at or below the note, and how far above it the note is.
            size_t degree{ 0 };

            for (size_t i = 0; i < fromSteps.size(); ++i)
            {
                if (fromSteps[i] <= within)
                {
                    degree = i;
                }
            }

            auto const alteration = within - static_cast<int>(fromSteps[degree]);

            // The same step in the new scale. A scale with fewer notes takes the nearest one at or
            // below the step's own distance from the root, so C–E–G into C major pentatonic stays
            // C–E–G, and F (not in it) goes to E.
            size_t toDegree{ 0 };

            if (toSteps.size() == fromSteps.size())
            {
                toDegree = degree;
            }
            else
            {
                for (size_t i = 0; i < toSteps.size(); ++i)
                {
                    if (toSteps[i] <= fromSteps[degree])
                    {
                        toDegree = i;
                    }
                }
            }

            // The root moves the smaller way round: -6 to +5 semitones.
            auto const rootMove = ((static_cast<int>(to.Root % 12) - static_cast<int>(from.Root % 12)) % 12 + 18) % 12 - 6;

            auto const moved = static_cast<int>(from.Root % 12) + rootMove + octave * 12 + static_cast<int>(toSteps[toDegree]) + alteration;
            return static_cast<uint8_t>(std::clamp(moved, 0, 127));
        }
        catch (...)
        {
            return note;
        }
    }

    _Use_decl_annotations_
    uint8_t NearestChordTone(uint8_t note, ChordEvent const& chord)
    {
        auto tones = ChordPitchClasses(chord);

        if (chord.Bass.has_value())
        {
            tones.push_back(static_cast<uint8_t>(*chord.Bass % 12));
        }

        int best{ note };
        int bestDistance{ INT_MAX };

        for (int candidate = std::max(0, note - 6); candidate <= std::min(127, note + 6); ++candidate)
        {
            if (std::find(tones.begin(), tones.end(), static_cast<uint8_t>(candidate % 12)) == tones.end())
            {
                continue;
            }

            auto const distance = std::abs(candidate - static_cast<int>(note));

            // Lower first, so the lower one wins a tie.
            if (distance < bestDistance)
            {
                best = candidate;
                bestDistance = distance;
            }
        }

        return static_cast<uint8_t>(best);
    }

    _Use_decl_annotations_
    bool StartsBeat(std::vector<MeterChange> const& meter, int64_t tick) noexcept
    {
        return tick >= 0 && BarPositionAtTick(meter, tick).TicksIntoBeat == 0;
    }

    _Use_decl_annotations_
    std::optional<KeyChange> SongKeyAt(Sequence const& sequence, int64_t tick) noexcept
    {
        if (!sequence.Harmony)
        {
            return std::nullopt;
        }

        if (auto const key = KeyAt(*sequence.Harmony, tick); key != nullptr)
        {
            return *key;
        }

        return std::nullopt;
    }

    _Use_decl_annotations_
    bool GeneratorUsesKey(GeneratorType type) noexcept
    {
        switch (type)
        {
        case GeneratorType::ChanceMelody:
        case GeneratorType::Life:
        case GeneratorType::Learn:
        case GeneratorType::Words:
        case GeneratorType::Evolve:
        case GeneratorType::Infinity:
        case GeneratorType::LSystem:
        case GeneratorType::ChangeRinging:
            return true;
        default:
            return false;
        }
    }

    _Use_decl_annotations_
    std::optional<GeneratorSettings> GeneratorInSongKey(Sequence const& sequence, GeneratorSettings const& generator, int64_t tick)
    {
        if (!generator.SongKey || !GeneratorUsesKey(generator.Type))
        {
            return std::nullopt;
        }

        auto const key = SongKeyAt(sequence, tick);

        if (!key || (key->Root % 12 == generator.ScaleRoot % 12 && key->Scale == generator.Scale))
        {
            return std::nullopt;
        }

        auto moved = generator;
        moved.ScaleRoot = static_cast<uint8_t>(key->Root % 12);
        moved.Scale = key->Scale;
        return moved;
    }

    _Use_decl_annotations_
    bool FollowsSongKey(Clip const& clip) noexcept
    {
        if (clip.Kind == ClipKind::Generator && clip.Generator.has_value())
        {
            return clip.Generator->SongKey && GeneratorUsesKey(clip.Generator->Type);
        }

        return ClipFollowsKey(clip);
    }

    _Use_decl_annotations_
    std::optional<Clip> ClipInSongKey(Sequence const& sequence, Clip const& clip, int64_t songTick)
    {
        if (clip.Kind != ClipKind::Generator || !clip.Generator.has_value())
        {
            return std::nullopt;
        }

        auto moved = GeneratorInSongKey(sequence, *clip.Generator, songTick);

        if (!moved)
        {
            return std::nullopt;
        }

        auto copy = clip;
        copy.Generator = std::move(*moved);
        return copy;
    }

    _Use_decl_annotations_
    Note AsPlayed(Sequence const& sequence, Clip const& clip, Note note, int64_t songTick) noexcept
    {
        try
        {
            if (auto const move = FollowKeyMove(sequence, clip, songTick); move.has_value())
            {
                note.Number = MoveByScaleSteps(note.Number, move->first, move->second);
            }
            else if (clip.Kind == ClipKind::Generator && clip.Generator.has_value() && clip.Generator->ChordTones &&
                GeneratorUsesKey(clip.Generator->Type) && sequence.Harmony.has_value() && !sequence.Harmony->Chords.empty() &&
                StartsBeat(sequence.Meter, songTick))
            {
                if (auto const chord = ChordAt(*sequence.Harmony, songTick); chord != nullptr)
                {
                    note.Number = NearestChordTone(note.Number, *chord);
                }
            }
        }
        catch (...)
        {
        }

        return note;
    }

    _Use_decl_annotations_
    void StartInSongKey(Clip& clip, Sequence const& sequence, int64_t tick)
    {
        auto const key = SongKeyAt(sequence, tick);

        if (!key)
        {
            return;
        }

        if (clip.Kind == ClipKind::Generator && clip.Generator.has_value() && GeneratorUsesKey(clip.Generator->Type))
        {
            clip.Generator->SongKey = true;
            clip.Generator->ScaleRoot = static_cast<uint8_t>(key->Root % 12);
            clip.Generator->Scale = key->Scale;
            RefreshGeneratorPreview(clip);
        }
        else if (clip.Kind == ClipKind::Pattern && clip.Pattern.has_value() && clip.Pattern->Mode == PatternMode::Melodic)
        {
            clip.FollowKey = true;
            clip.WrittenRoot = static_cast<uint8_t>(key->Root % 12);
            clip.WrittenScale = key->Scale;
        }
    }

    _Use_decl_annotations_
    Clip ChordsAsNotes(std::vector<ChordEvent> const& chords, uint16_t velocity)
    {
        Clip clip{};
        clip.Kind = ClipKind::Notes;
        clip.Loop = false;
        clip.Length = TicksPerQuarterNote * 4;

        if (chords.empty())
        {
            return clip;
        }

        auto const start = std::min_element(chords.begin(), chords.end(), [](ChordEvent const& a, ChordEvent const& b) { return a.Tick < b.Tick; })->Tick;
        int64_t end{ start + 1 };

        for (auto const& chord : chords)
        {
            for (auto const number : ChordNotes(chord))
            {
                Note note{};
                note.Tick = chord.Tick - start;
                note.Length = std::max<int64_t>(1, chord.Length);
                note.Number = number;
                note.Velocity = velocity;
                clip.Notes.push_back(note);
            }

            end = std::max(end, chord.Tick + std::max<int64_t>(1, chord.Length));
        }

        std::stable_sort(clip.Notes.begin(), clip.Notes.end(), [](Note const& a, Note const& b)
        {
            return a.Tick != b.Tick ? a.Tick < b.Tick : a.Number < b.Number;
        });

        clip.Length = end - start;
        return clip;
    }

    _Use_decl_annotations_
    bool ClipFollowsKey(Clip const& clip) noexcept
    {
        if (!clip.FollowKey)
        {
            return false;
        }

        if (clip.Kind == ClipKind::Notes)
        {
            return true;
        }

        return clip.Kind == ClipKind::Pattern && clip.Pattern.has_value() && clip.Pattern->Mode == PatternMode::Melodic;
    }

    _Use_decl_annotations_
    std::optional<std::pair<KeyChange, KeyChange>> FollowKeyMove(Sequence const& sequence, Clip const& clip, int64_t tick) noexcept
    {
        if (!ClipFollowsKey(clip))
        {
            return std::nullopt;
        }

        auto const key = SongKeyAt(sequence, tick);
        KeyChange const written{ 0, clip.WrittenRoot, clip.WrittenScale };

        if (!key || SameKey(*key, written))
        {
            return std::nullopt;
        }

        return std::pair{ written, *key };
    }
}
