// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#include "SequenceGenerator.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <numeric>

namespace midisequencer
{
    namespace
    {
        // A chance melody that keeps changing is worked out pass by pass from the first, so this
        // caps the work: about a day of a one-bar clip.
        constexpr uint64_t MaximumMelodyPasses = 100000;

        uint64_t Mix(_In_ uint64_t value) noexcept
        {
            value += 0x9E3779B97F4A7C15ull;
            value = (value ^ (value >> 30)) * 0xBF58476D1CE4E5B9ull;
            value = (value ^ (value >> 27)) * 0x94D049BB133111EBull;
            return value ^ (value >> 31);
        }

        // One roll of the dice, the same every time for the same seed, pass, step and purpose.
        uint64_t Roll(_In_ uint32_t seed, _In_ uint64_t pass, _In_ uint64_t step, _In_ uint64_t purpose) noexcept
        {
            return Mix((static_cast<uint64_t>(seed) << 32) ^ Mix(pass * 0x9E3779B97F4A7C15ull + purpose) ^ (step * 0xBF58476D1CE4E5B9ull));
        }

        std::array<uint8_t, 12> const* ScaleSteps(_In_ ScaleKind scale, _Out_ size_t& count) noexcept
        {
            static constexpr std::array<std::array<uint8_t, 12>, 11> Scales{ {
                { 0, 2, 4, 5, 7, 9, 11 },               // major
                { 0, 2, 3, 5, 7, 8, 10 },               // natural minor
                { 0, 2, 3, 5, 7, 9, 10 },               // dorian
                { 0, 1, 3, 5, 7, 8, 10 },               // phrygian
                { 0, 2, 4, 6, 7, 9, 11 },               // lydian
                { 0, 2, 4, 5, 7, 9, 10 },               // mixolydian
                { 0, 2, 3, 5, 7, 8, 11 },               // harmonic minor
                { 0, 2, 4, 7, 9 },                      // major pentatonic
                { 0, 3, 5, 7, 10 },                     // minor pentatonic
                { 0, 3, 5, 6, 7, 10 },                  // blues
                { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11 }, // chromatic
            } };

            static constexpr std::array<size_t, 11> Counts{ 7, 7, 7, 7, 7, 7, 7, 5, 5, 6, 12 };

            auto const index = static_cast<size_t>(scale) < Scales.size() ? static_cast<size_t>(scale) : size_t{ 8 };
            count = Counts[index];
            return &Scales[index];
        }

        struct MelodyStep
        {
            bool Plays{ false };
            uint8_t Note{ 60 };
            uint16_t Velocity{ 0 };
        };

        // What each step of a chance melody plays on a pass.
        std::vector<MelodyStep> MelodyPass(_In_ GeneratorSettings const& generator, _In_ uint32_t seed, _In_ int64_t steps, _In_ uint64_t pass)
        {
            auto const notes = ScaleNotes(generator.ScaleRoot, generator.Scale, generator.LowNote, generator.HighNote);
            auto const low = std::min(generator.VelocityLow, generator.VelocityHigh);
            auto const span = static_cast<uint64_t>(std::max(generator.VelocityLow, generator.VelocityHigh) - low) + 1;

            auto const fresh = [&](uint64_t on, uint64_t step)
            {
                MelodyStep result{};
                result.Plays = Roll(seed, on, step, 1) % 100 < generator.Busy;
                result.Note = notes[static_cast<size_t>(Roll(seed, on, step, 2) % notes.size())];
                result.Velocity = static_cast<uint16_t>(low + Roll(seed, on, step, 3) % span);
                return result;
            };

            std::vector<MelodyStep> current(static_cast<size_t>(steps));

            for (int64_t step = 0; step < steps; ++step)
            {
                current[static_cast<size_t>(step)] = fresh(0, static_cast<uint64_t>(step));
            }

            if (!generator.ChangesEachPass)
            {
                return current;
            }

            // Each pass keeps some steps from the one before and picks the rest again.
            auto const last = std::min(pass, MaximumMelodyPasses);

            for (uint64_t on = 1; on <= last; ++on)
            {
                for (int64_t step = 0; step < steps; ++step)
                {
                    if (Roll(seed, on, static_cast<uint64_t>(step), 4) % 100 >= generator.Keep)
                    {
                        current[static_cast<size_t>(step)] = fresh(on, static_cast<uint64_t>(step));
                    }
                }
            }

            return current;
        }

        // The least common multiple, or more than limit when it would pass it.
        uint64_t CappedLcm(_In_ uint64_t a, _In_ uint64_t b, _In_ uint64_t limit) noexcept
        {
            if (a == 0 || b == 0)
            {
                return std::max(a, b);
            }

            auto const reduced = a / std::gcd(a, b);
            return reduced > limit / b ? limit + 1 : reduced * b;
        }
    }

    _Use_decl_annotations_
    std::vector<bool> EuclideanRhythm(uint8_t pulses, uint8_t steps, uint8_t rotate)
    {
        steps = std::clamp<uint8_t>(steps, 1, MaximumRingSteps);
        pulses = std::min(pulses, steps);

        std::vector<bool> hits(steps, false);

        // Step i is a hit when the pulses' running total passes a whole number there, which
        // spreads them as evenly as they go.
        for (uint32_t i = 0; i < steps; ++i)
        {
            if ((i * pulses) % steps < pulses)
            {
                hits[(i + rotate) % steps] = true;
            }
        }

        return hits;
    }

    _Use_decl_annotations_
    std::vector<uint8_t> ScaleNotes(uint8_t root, ScaleKind scale, uint8_t low, uint8_t high)
    {
        low &= 0x7F;
        high &= 0x7F;

        if (high < low)
        {
            std::swap(low, high);
        }

        size_t count{ 0 };
        auto const steps = ScaleSteps(scale, count);

        std::vector<uint8_t> notes{};

        for (uint32_t note = low; note <= high; ++note)
        {
            auto const degree = static_cast<uint8_t>((note + 12 - (root % 12)) % 12);

            if (std::find(steps->begin(), steps->begin() + static_cast<ptrdiff_t>(count), degree) != steps->begin() + static_cast<ptrdiff_t>(count))
            {
                notes.push_back(static_cast<uint8_t>(note));
            }
        }

        if (notes.empty())
        {
            notes.push_back(low);
        }

        return notes;
    }

    GeneratorSettings MakeEuclideanGenerator()
    {
        GeneratorSettings generator{};
        generator.Type = GeneratorType::Euclidean;

        auto const ring = [](uint8_t note, uint8_t pulses, uint8_t rotate, double velocity)
        {
            EuclidRing made{};
            made.Note = note;
            made.Pulses = pulses;
            made.Steps = 16;
            made.Rotate = rotate;
            made.Velocity = static_cast<uint16_t>(std::lround(velocity * 65535.0));
            return made;
        };

        generator.Rings = { ring(52, 5, 0, 0.9), ring(59, 7, 2, 0.7), ring(67, 3, 1, 0.8) };
        return generator;
    }

    GeneratorSettings MakeChanceMelodyGenerator()
    {
        GeneratorSettings generator{};
        generator.Type = GeneratorType::ChanceMelody;
        generator.ChangesEachPass = true;
        generator.NoteLength = 80;
        generator.ScaleRoot = 4;
        generator.Scale = ScaleKind::MinorPentatonic;
        generator.LowNote = 64;
        generator.HighNote = 76;
        generator.Busy = 50;
        generator.Keep = 75;
        return generator;
    }

    _Use_decl_annotations_
    void NormalizeGenerator(GeneratorSettings& generator)
    {
        if (generator.Type > GeneratorType::Arpeggiator)
        {
            generator.Type = GeneratorType::Euclidean;
        }

        if (generator.Scale > ScaleKind::Chromatic)
        {
            generator.Scale = ScaleKind::MinorPentatonic;
        }

        generator.StepTicks = std::clamp<int64_t>(generator.StepTicks, 1, TicksPerQuarterNote * 4);
        generator.Channel &= 0x0F;
        generator.NoteLength = std::clamp<uint16_t>(generator.NoteLength, 1, 1600);
        generator.ScaleRoot %= 12;
        generator.LowNote &= 0x7F;
        generator.HighNote &= 0x7F;
        generator.Busy = std::min<uint8_t>(generator.Busy, 100);
        generator.Keep = std::min<uint8_t>(generator.Keep, 100);

        if (generator.HighNote < generator.LowNote)
        {
            std::swap(generator.LowNote, generator.HighNote);
        }

        if (generator.VelocityHigh < generator.VelocityLow)
        {
            std::swap(generator.VelocityLow, generator.VelocityHigh);
        }

        if (generator.Rings.size() > MaximumEuclidRings)
        {
            generator.Rings.resize(MaximumEuclidRings);
        }

        for (auto& ring : generator.Rings)
        {
            ring.Note &= 0x7F;
            ring.Steps = std::clamp<uint8_t>(ring.Steps, 1, MaximumRingSteps);
            ring.Pulses = std::min(ring.Pulses, ring.Steps);
            ring.Rotate = static_cast<uint8_t>(ring.Rotate % ring.Steps);
            ring.Chance = std::min<uint8_t>(ring.Chance, 100);
        }
    }

    _Use_decl_annotations_
    int64_t GeneratorStepCount(Clip const& clip) noexcept
    {
        if (!clip.Generator.has_value())
        {
            return 1;
        }

        return std::clamp<int64_t>(clip.Length / std::max<int64_t>(1, clip.Generator->StepTicks), 1, MaximumGeneratorSteps);
    }

    _Use_decl_annotations_
    void ForEachGeneratorNote(
        Clip const& clip,
        int64_t passStart,
        int64_t passEnd,
        uint64_t pass,
        int64_t fromTick,
        int64_t toTick,
        bool applyChance,
        std::function<void(Note const&)> const& add)
    {
        if (!clip.Generator.has_value())
        {
            return;
        }

        auto const& generator = *clip.Generator;
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

        auto const emit = [&](int64_t step, uint8_t number, uint16_t velocity, uint8_t chance)
        {
            Note note{};
            note.Tick = passStart + step * stepTicks;
            note.Length = length;
            note.Channel = static_cast<uint8_t>(generator.Channel & 0x0F);
            note.Number = static_cast<uint8_t>(number & 0x7F);
            note.Velocity = std::max<uint16_t>(1, velocity);
            note.ReleaseVelocity = 0x8000;
            note.Chance = chance;
            add(note);
        };

        switch (generator.Type)
        {
        case GeneratorType::Euclidean:
        {
            std::vector<std::vector<bool>> hits{};
            hits.reserve(generator.Rings.size());

            for (auto const& ring : generator.Rings)
            {
                hits.push_back(EuclideanRhythm(ring.Pulses, ring.Steps, ring.Rotate));
            }

            for (auto step = firstStep; step <= lastStep; ++step)
            {
                auto const global = pass * static_cast<uint64_t>(steps) + static_cast<uint64_t>(step);

                for (size_t r = 0; r < generator.Rings.size(); ++r)
                {
                    auto const& ring = generator.Rings[r];
                    auto const ringSteps = static_cast<uint64_t>(std::max<uint8_t>(1, ring.Steps));
                    auto const position = global % ringSteps;

                    if (!hits[r][static_cast<size_t>(position)])
                    {
                        continue;
                    }

                    if (applyChance && ring.Chance < 100)
                    {
                        // The same every time round, unless the clip changes each pass.
                        auto const ringPass = generator.ChangesEachPass ? global / ringSteps : 0;

                        if (ring.Chance == 0 || Roll(clip.Seed, ringPass, position, 16 + r) % 100 >= ring.Chance)
                        {
                            continue;
                        }
                    }

                    emit(step, ring.Note, ring.Velocity, applyChance ? uint8_t{ 100 } : ring.Chance);
                }
            }

            break;
        }

        case GeneratorType::ChanceMelody:
        {
            auto const state = MelodyPass(generator, clip.Seed, steps, pass);

            for (auto step = firstStep; step <= lastStep; ++step)
            {
                auto const& chosen = state[static_cast<size_t>(step)];

                if (chosen.Plays)
                {
                    emit(step, chosen.Note, chosen.Velocity, 100);
                }
            }

            break;
        }

        case GeneratorType::Arpeggiator:
        default:
            break;
        }
    }

    _Use_decl_annotations_
    void RefreshGeneratorPreview(Clip& clip)
    {
        if (clip.Kind != ClipKind::Generator || !clip.Generator.has_value())
        {
            return;
        }

        std::vector<Note> notes{};
        ForEachGeneratorNote(clip, 0, clip.Length, 0, 0, clip.Length, false, [&notes](Note const& note) { notes.push_back(note); });

        std::stable_sort(notes.begin(), notes.end(), [](Note const& a, Note const& b)
        {
            return a.Tick != b.Tick ? a.Tick < b.Tick : a.Number < b.Number;
        });

        clip.Notes = std::move(notes);
    }

    _Use_decl_annotations_
    Clip MakeNotesFromGenerator(Clip const& generator, uint32_t passes, int64_t maximumTicks)
    {
        Clip notes{};
        notes.Id = NewId(L"c");
        notes.Kind = ClipKind::Notes;
        notes.Name = generator.Name;
        notes.Color = generator.Color;
        notes.Length = generator.Length;
        notes.Loop = generator.Loop;
        notes.Seed = generator.Seed;
        notes.Origin = ClipOrigin::Generated;
        notes.OriginDetail = generator.OriginDetail;

        if (!generator.Generator.has_value() || generator.Length <= 0)
        {
            return notes;
        }

        auto const& settings = *generator.Generator;
        auto const passLength = generator.Length;
        auto const steps = static_cast<uint64_t>(GeneratorStepCount(generator));
        auto const passLimit = static_cast<uint64_t>(std::max<int64_t>(1, maximumTicks / passLength));

        uint64_t count{ std::max<uint32_t>(1, passes) };

        auto const changes = settings.ChangesEachPass && (settings.Type == GeneratorType::ChanceMelody ||
            std::any_of(settings.Rings.begin(), settings.Rings.end(), [](EuclidRing const& ring) { return ring.Chance < 100; }));

        if (!changes)
        {
            if (settings.Type == GeneratorType::Euclidean)
            {
                // Until the rings line up with the clip again.
                auto cycle = steps;

                for (auto const& ring : settings.Rings)
                {
                    cycle = CappedLcm(cycle, std::max<uint8_t>(1, ring.Steps), passLimit * steps);
                }

                count = (cycle + steps - 1) / steps;
            }
            else
            {
                count = 1;
            }
        }

        count = std::clamp<uint64_t>(count, 1, passLimit);
        notes.Length = static_cast<int64_t>(count) * passLength;

        for (uint64_t pass = 0; pass < count; ++pass)
        {
            auto const start = static_cast<int64_t>(pass) * passLength;
            ForEachGeneratorNote(generator, start, start + passLength, pass, start, start + passLength, true,
                [&notes](Note const& note) { notes.Notes.push_back(note); });
        }

        for (auto const& lane : generator.Lanes)
        {
            auto copy = lane;
            copy.Points.clear();

            for (uint64_t pass = 0; pass < count; ++pass)
            {
                for (auto point : lane.Points)
                {
                    if (point.Tick < passLength)
                    {
                        point.Tick += static_cast<int64_t>(pass) * passLength;
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
