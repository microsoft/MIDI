// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#include "SequenceArpeggiator.h"
#include "SequenceGenerator.h"
#include "SequencePattern.h"

#include <algorithm>

namespace midisequencer
{
    namespace
    {
        // A notes clip is searched back from the tick for notes still sounding. A chord is a
        // handful of notes; this many is far more than a long chord under a busy part needs.
        constexpr size_t MaximumNotesLookedBack = 2048;

        // Make notes stops after this many passes of one placement.
        constexpr int64_t MaximumMadePasses = 100000;

        uint64_t Mix(_In_ uint64_t value) noexcept
        {
            value += 0x9E3779B97F4A7C15ull;
            value = (value ^ (value >> 30)) * 0xBF58476D1CE4E5B9ull;
            value = (value ^ (value >> 27)) * 0x94D049BB133111EBull;
            return value ^ (value >> 31);
        }

        // The same every time for the same seed, pass and step, like the other generators.
        uint64_t Roll(_In_ uint32_t seed, _In_ uint64_t pass, _In_ uint64_t step) noexcept
        {
            return Mix((static_cast<uint64_t>(seed) << 32) ^ Mix(pass * 0x9E3779B97F4A7C15ull + 7) ^ (step * 0xBF58476D1CE4E5B9ull));
        }

        // Steps from passStart to the first step at or after tick, which can be before the pass.
        int64_t StepAtOrAfter(_In_ int64_t tick, _In_ int64_t passStart, _In_ int64_t stepTicks) noexcept
        {
            auto const offset = tick - passStart;
            return offset >= 0 ? (offset + stepTicks - 1) / stepTicks : -((-offset) / stepTicks);
        }
    }

    _Use_decl_annotations_
    void ArpeggiatorInput::Press(uint8_t number, uint16_t velocity)
    {
        number &= 0x7F;
        velocity = std::max<uint16_t>(1, velocity);

        // The first key down starts a new chord, which replaces what was latched.
        if (Held.empty())
        {
            Latched.clear();
            StepsPlayed = 0;
        }

        std::erase_if(Held, [number](HeldNote const& key) { return key.Number == number; });
        std::erase_if(Latched, [number](HeldNote const& key) { return key.Number == number; });

        if (Held.size() < MaximumHeldKeys)
        {
            Held.push_back(HeldNote{ number, velocity });
        }

        if (Latched.size() < MaximumHeldKeys)
        {
            Latched.push_back(HeldNote{ number, velocity });
        }
    }

    _Use_decl_annotations_
    void ArpeggiatorInput::Release(uint8_t number)
    {
        number &= 0x7F;
        std::erase_if(Held, [number](HeldNote const& key) { return key.Number == number; });
    }

    void ArpeggiatorInput::Restart() noexcept
    {
        Latched.clear();
        StepsPlayed = 0;
        Used = false;
    }

    GeneratorSettings MakeArpeggiatorGenerator()
    {
        GeneratorSettings generator{};
        generator.Type = GeneratorType::Arpeggiator;
        generator.StepTicks = TicksPerQuarterNote / 4;
        generator.NoteLength = 60;
        generator.ArpSource = ArpeggiatorSource::Held;
        generator.ArpOrder = ArpeggiatorOrder::Up;
        generator.ArpOctaves = 2;
        return generator;
    }

    _Use_decl_annotations_
    std::vector<HeldNote> ArpeggiatorSequence(std::vector<HeldNote> const& chord, ArpeggiatorOrder order, uint8_t octaves)
    {
        // Each note number once, as loud as the loudest of it.
        std::vector<HeldNote> notes{};

        for (auto const& note : chord)
        {
            auto const same = std::find_if(notes.begin(), notes.end(), [&note](HeldNote const& kept) { return kept.Number == note.Number; });

            if (same == notes.end())
            {
                notes.push_back(HeldNote{ static_cast<uint8_t>(note.Number & 0x7F), note.Velocity });
            }
            else
            {
                same->Velocity = std::max(same->Velocity, note.Velocity);
            }
        }

        if (order != ArpeggiatorOrder::AsHeld)
        {
            std::stable_sort(notes.begin(), notes.end(), [](HeldNote const& a, HeldNote const& b) { return a.Number < b.Number; });
        }

        std::vector<HeldNote> steps{};
        auto const count = std::clamp<uint8_t>(octaves, 1, MaximumArpeggiatorOctaves);

        for (uint8_t octave = 0; octave < count; ++octave)
        {
            for (auto const& note : notes)
            {
                auto const number = note.Number + 12 * octave;

                if (number <= 127)
                {
                    steps.push_back(HeldNote{ static_cast<uint8_t>(number), note.Velocity });
                }
            }
        }

        switch (order)
        {
        case ArpeggiatorOrder::Down:
            std::reverse(steps.begin(), steps.end());
            break;

        case ArpeggiatorOrder::UpAndDown:
            // Back down without playing the top or the bottom twice.
            if (steps.size() > 2)
            {
                for (auto i = steps.size() - 2; i >= 1; --i)
                {
                    auto const back = steps[i];
                    steps.push_back(back);
                }
            }
            break;

        default:
            break;
        }

        return steps;
    }

    _Use_decl_annotations_
    SoundingChord NotesSoundingAt(Sequence const& sequence, Track const& track, int64_t tick)
    {
        // Each note with where it started, from every placement playing at the tick.
        std::vector<std::pair<int64_t, HeldNote>> sounding{};

        for (auto const& placement : track.Timeline)
        {
            auto const clip = FindClip(sequence, placement.ClipId);

            if (clip == nullptr || clip->Length <= 0 || IsArpeggiator(*clip))
            {
                continue;
            }

            auto const length = placement.Length > 0 ? placement.Length : clip->Length;
            auto const end = placement.Tick + (clip->Loop ? length : std::min(length, clip->Length));

            if (tick < placement.Tick || tick >= end)
            {
                continue;
            }

            auto const pass = (tick - placement.Tick) / clip->Length;
            auto const passStart = placement.Tick + pass * clip->Length;
            auto const passEnd = std::min(end, passStart + clip->Length);

            auto const consider = [&](Note const& note, int64_t start)
            {
                auto const stop = std::min(start + std::max<int64_t>(1, note.Length), passEnd);

                if (start <= tick && tick < stop)
                {
                    sounding.emplace_back(start, HeldNote{ static_cast<uint8_t>(note.Number & 0x7F), std::max<uint16_t>(1, note.Velocity) });
                }
            };

            if (clip->Kind == ClipKind::Pattern && clip->Pattern.has_value())
            {
                ForEachPatternNote(*clip, passStart, passEnd, static_cast<uint64_t>(pass), passStart, tick + 1, true,
                    [&consider](Note const& note) { consider(note, note.Tick); });
                continue;
            }

            if (clip->Kind == ClipKind::Generator && clip->Generator.has_value())
            {
                ForEachGeneratorNote(*clip, passStart, passEnd, static_cast<uint64_t>(pass), passStart, tick + 1, true,
                    [&consider](Note const& note) { consider(note, note.Tick); });
                continue;
            }

            auto const local = tick - passStart;
            auto it = std::upper_bound(clip->Notes.begin(), clip->Notes.end(), local,
                [](int64_t value, Note const& note) { return value < note.Tick; });

            for (size_t looked = 0; it != clip->Notes.begin() && looked < MaximumNotesLookedBack; ++looked)
            {
                --it;

                auto const index = static_cast<uint32_t>(std::distance(clip->Notes.begin(), it));

                if (NotePlaysOnPass(*it, clip->Seed, index, static_cast<uint64_t>(pass)))
                {
                    consider(*it, passStart + it->Tick);
                }
            }
        }

        std::stable_sort(sounding.begin(), sounding.end(), [](auto const& a, auto const& b)
        {
            return a.first != b.first ? a.first < b.first : a.second.Number < b.second.Number;
        });

        SoundingChord chord{};

        for (auto const& [start, note] : sounding)
        {
            auto const same = std::find_if(chord.Notes.begin(), chord.Notes.end(), [&note](HeldNote const& kept) { return kept.Number == note.Number; });

            if (same == chord.Notes.end())
            {
                chord.Notes.push_back(note);
            }
            else
            {
                same->Velocity = std::max(same->Velocity, note.Velocity);
            }

            chord.StartTick = std::max(chord.StartTick, start);
        }

        return chord;
    }

    _Use_decl_annotations_
    void ForEachArpeggiatorNote(
        Sequence const& sequence,
        Clip const& clip,
        int64_t passStart,
        int64_t passEnd,
        uint64_t pass,
        int64_t fromTick,
        int64_t toTick,
        ArpeggiatorInput* keys,
        std::function<void(Note const&)> const& add)
    {
        if (!IsArpeggiator(clip))
        {
            return;
        }

        auto const& generator = *clip.Generator;
        auto const held = generator.ArpSource == ArpeggiatorSource::Held;

        // The keys aren't there when exporting, so there's nothing to play.
        if (held && keys == nullptr)
        {
            return;
        }

        if (held)
        {
            keys->Used = true;
        }

        Track const* source{ nullptr };

        if (!held)
        {
            source = FindTrack(sequence, generator.ArpTrackId);

            if (source == nullptr || source->IsFolder)
            {
                return;
            }
        }

        auto const stepTicks = std::max<int64_t>(1, generator.StepTicks);
        auto const steps = GeneratorStepCount(clip);
        auto const windowStart = std::max(fromTick, passStart);
        auto const windowEnd = std::min(toTick, passEnd);

        if (windowStart >= windowEnd)
        {
            return;
        }

        auto const firstStep = std::max<int64_t>(0, (windowStart - passStart + stepTicks - 1) / stepTicks);
        auto const lastStep = std::min<int64_t>(steps - 1, (windowEnd - 1 - passStart) / stepTicks);
        auto const length = std::max<int64_t>(1, stepTicks * generator.NoteLength / 100);

        for (auto step = firstStep; step <= lastStep; ++step)
        {
            auto const tick = passStart + step * stepTicks;

            std::vector<HeldNote> chord{};
            uint64_t index{ 0 };

            if (held)
            {
                chord = generator.ArpLatch ? keys->Latched : keys->Held;

                if (chord.empty())
                {
                    continue;
                }

                index = keys->StepsPlayed++;
            }
            else
            {
                auto sounding = NotesSoundingAt(sequence, *source, tick);

                if (sounding.Notes.empty())
                {
                    continue;
                }

                // Each chord starts at the start of the order, on the first step after it began.
                index = static_cast<uint64_t>(std::max<int64_t>(0, step - StepAtOrAfter(sounding.StartTick, passStart, stepTicks)));
                chord = std::move(sounding.Notes);
            }

            auto const order = ArpeggiatorSequence(chord, generator.ArpOrder, generator.ArpOctaves);

            if (order.empty())
            {
                continue;
            }

            auto const pick = generator.ArpOrder == ArpeggiatorOrder::Random
                ? order[static_cast<size_t>(Roll(clip.Seed, generator.ChangesEachPass ? pass : 0, index) % order.size())]
                : order[static_cast<size_t>(index % order.size())];

            Note note{};
            note.Tick = tick;
            note.Length = length;
            note.Channel = static_cast<uint8_t>(generator.Channel & 0x0F);
            note.Number = pick.Number;
            note.Velocity = std::max<uint16_t>(1, pick.Velocity);
            note.ReleaseVelocity = 0x8000;
            add(note);
        }
    }

    _Use_decl_annotations_
    bool IsArpeggiator(Clip const& clip) noexcept
    {
        return clip.Kind == ClipKind::Generator && clip.Generator.has_value() && clip.Generator->Type == GeneratorType::Arpeggiator;
    }

    _Use_decl_annotations_
    bool PlaysHeldKeys(Clip const& clip) noexcept
    {
        return IsArpeggiator(clip) && clip.Generator->ArpSource == ArpeggiatorSource::Held;
    }

    _Use_decl_annotations_
    bool TrackHasHeldKeysArpeggiator(Sequence const& sequence, Track const& track)
    {
        auto const plays = [&sequence](std::wstring const& clipId)
        {
            auto const clip = clipId.empty() ? nullptr : FindClip(sequence, clipId);
            return clip != nullptr && PlaysHeldKeys(*clip);
        };

        return std::any_of(track.Timeline.begin(), track.Timeline.end(), [&plays](Placement const& placement) { return plays(placement.ClipId); }) ||
            std::any_of(track.Slots.begin(), track.Slots.end(), plays);
    }

    _Use_decl_annotations_
    Clip MakeNotesFromArpeggiator(Sequence const& sequence, Clip const& arpeggiator, Placement const& placement)
    {
        Clip notes{};
        notes.Id = NewId(L"c");
        notes.Kind = ClipKind::Notes;
        notes.Name = arpeggiator.Name;
        notes.Color = arpeggiator.Color;
        notes.Loop = false;
        notes.Seed = arpeggiator.Seed;
        notes.Origin = ClipOrigin::Generated;
        notes.OriginDetail = arpeggiator.OriginDetail;

        auto const passLength = std::max<int64_t>(1, arpeggiator.Length);
        auto const length = placement.Length > 0 ? placement.Length : arpeggiator.Length;
        auto const span = std::max<int64_t>(1, arpeggiator.Loop ? length : std::min(length, arpeggiator.Length));
        notes.Length = span;

        if (!IsArpeggiator(arpeggiator) || PlaysHeldKeys(arpeggiator))
        {
            return notes;
        }

        auto const passes = std::min<int64_t>((span + passLength - 1) / passLength, MaximumMadePasses);

        for (int64_t pass = 0; pass < passes; ++pass)
        {
            auto const passStart = placement.Tick + pass * passLength;
            auto const passEnd = std::min(placement.Tick + span, passStart + passLength);

            ForEachArpeggiatorNote(sequence, arpeggiator, passStart, passEnd, static_cast<uint64_t>(pass), passStart, passEnd, nullptr,
                [&notes, &placement](Note const& note)
                {
                    auto made = note;
                    made.Tick -= placement.Tick;
                    notes.Notes.push_back(made);
                });
        }

        // The clip's lanes go round with each pass, as they did.
        for (auto const& lane : arpeggiator.Lanes)
        {
            auto copy = lane;
            copy.Points.clear();

            for (int64_t pass = 0; pass < passes; ++pass)
            {
                for (auto point : lane.Points)
                {
                    if (point.Tick < passLength && pass * passLength + point.Tick < span)
                    {
                        point.Tick += pass * passLength;
                        copy.Points.push_back(point);
                    }
                }
            }

            notes.Lanes.push_back(std::move(copy));
        }

        SortClip(notes);
        return notes;
    }
}
