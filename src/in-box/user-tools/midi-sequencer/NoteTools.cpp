// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#include "NoteTools.h"

#include <algorithm>
#include <cmath>
#include <iterator>
#include <limits>
#include <map>
#include <numeric>
#include <random>
#include <tuple>

namespace midisequencer
{
    namespace
    {
        // The quietest a note can be and still be a note: 1 in MIDI 1.0's 7 bits. Velocity 0 is
        // a note off to a MIDI 1.0 instrument.
        constexpr uint16_t QuietestVelocity = 512;

        // A note this close to a beat, in parts of the beat, is on it.
        constexpr int64_t OnBeatParts = 12;

        // Notes that start this close together are one chord.
        constexpr double ChordWindowMs = 30.0;

        bool NoteOrder(Note const& a, Note const& b) noexcept
        {
            return std::tie(a.Tick, a.Number, a.Channel, a.Length, a.Velocity) < std::tie(b.Tick, b.Number, b.Channel, b.Length, b.Velocity);
        }

        std::vector<Note> Sorted(std::vector<Note> notes)
        {
            std::stable_sort(notes.begin(), notes.end(), NoteOrder);
            return notes;
        }

        std::vector<MeterChange> const& MeterOf(NoteToolContext const& context)
        {
            static std::vector<MeterChange> const common{ MeterChange{} };
            return context.Meter != nullptr && !context.Meter->empty() ? *context.Meter : common;
        }

        // How many ticks a time takes from a point in the clip, by the tempo there.
        int64_t TicksForMs(NoteToolContext const& context, int64_t clipTick, double ms) noexcept
        {
            auto const songTick = context.ClipStart + clipTick;

            if (context.Tempo == nullptr)
            {
                // 120 BPM.
                return static_cast<int64_t>(std::llround(ms / 1000.0 * 2.0 * static_cast<double>(TicksPerQuarterNote)));
            }

            auto const from = context.Tempo->SecondsAtTick(songTick);
            auto const to = std::max(0.0, from + ms / 1000.0);
            return context.Tempo->TickAtSeconds(to) - songTick;
        }

        int64_t BeatTicksAt(NoteToolContext const& context, int64_t clipTick) noexcept
        {
            return std::max<int64_t>(1, TicksPerBeat(MeterAtTick(MeterOf(context), context.ClipStart + clipTick)));
        }

        // Where a note is in its bar, in ticks from the bar's start.
        int64_t TicksIntoBar(NoteToolContext const& context, int64_t clipTick) noexcept
        {
            auto const songTick = std::max<int64_t>(0, context.ClipStart + clipTick);
            auto const& meter = MeterAtTick(MeterOf(context), songTick);
            auto const bar = std::max<int64_t>(1, TicksPerBar(meter));
            return (songTick - meter.Tick) % bar;
        }

        bool IsOnBeat(NoteToolContext const& context, int64_t clipTick) noexcept
        {
            auto const songTick = std::max<int64_t>(0, context.ClipStart + clipTick);
            auto const& meter = MeterAtTick(MeterOf(context), songTick);
            auto const beat = std::max<int64_t>(1, TicksPerBeat(meter));
            auto const into = (songTick - meter.Tick) % beat;
            return std::min(into, beat - into) <= beat / OnBeatParts;
        }

        bool IsOnDownbeat(NoteToolContext const& context, int64_t clipTick) noexcept
        {
            auto const songTick = std::max<int64_t>(0, context.ClipStart + clipTick);
            auto const& meter = MeterAtTick(MeterOf(context), songTick);
            auto const bar = std::max<int64_t>(1, TicksPerBar(meter));
            auto const beat = std::max<int64_t>(1, TicksPerBeat(meter));
            auto const into = (songTick - meter.Tick) % bar;
            return std::min(into, bar - into) <= beat / OnBeatParts;
        }

        // From -1 to 1, most often near 0, the way a player's small errors are.
        double Wobble(std::mt19937& random) noexcept
        {
            auto const a = static_cast<double>(random()) / static_cast<double>(std::mt19937::max());
            auto const b = static_cast<double>(random()) / static_cast<double>(std::mt19937::max());
            return a + b - 1.0;
        }

        uint16_t ClampVelocity(double value) noexcept
        {
            return static_cast<uint16_t>(std::clamp<double>(std::round(value), QuietestVelocity, 65535.0));
        }

        // The changed notes of a one-for-one edit.
        NoteToolResult Changes(std::vector<Note> const& before, std::vector<Note> const& after)
        {
            NoteToolResult result{};

            for (size_t i = 0; i < before.size() && i < after.size(); ++i)
            {
                if (before[i] != after[i])
                {
                    result.Removed.push_back(before[i]);
                    result.Added.push_back(after[i]);
                }
            }

            return result;
        }

        // The pitch classes of the key, lowest first.
        std::vector<int32_t> KeyDegrees(std::array<bool, 12> const& inKey)
        {
            std::vector<int32_t> degrees{};

            for (int32_t pc = 0; pc < 12; ++pc)
            {
                if (inKey[static_cast<size_t>(pc)])
                {
                    degrees.push_back(pc);
                }
            }

            return degrees;
        }

        // The nearest note whose pitch class is in tones, the lower one when two are as near.
        std::optional<int32_t> NearestTone(int32_t number, std::array<bool, 12> const& tones) noexcept
        {
            for (int32_t distance = 0; distance <= 6; ++distance)
            {
                for (auto const candidate : { number - distance, number + distance })
                {
                    if (candidate >= 0 && candidate <= 127 && tones[static_cast<size_t>(candidate % 12)])
                    {
                        return candidate;
                    }
                }
            }

            return std::nullopt;
        }

        // Puts new note numbers on a chord's notes, lowest number to lowest note.
        void Renumber(std::vector<Note> const& chord, std::vector<int32_t> numbers, std::vector<Note>& before, std::vector<Note>& after)
        {
            std::sort(numbers.begin(), numbers.end());

            if (numbers.front() < 0 || numbers.back() > 127)
            {
                return;
            }

            for (size_t i = 0; i < chord.size(); ++i)
            {
                auto changed = chord[i];
                changed.Number = static_cast<uint8_t>(numbers[i]);
                before.push_back(chord[i]);
                after.push_back(changed);
            }
        }

        // The chord's notes stacked from the lowest, each above the one before: a close voicing
        // starting on the note the rotation picks.
        std::vector<int32_t> Stack(std::vector<int32_t> const& classes, size_t rotation, int32_t lowest)
        {
            std::vector<int32_t> numbers{};
            auto previous = lowest - 1;

            for (size_t i = 0; i < classes.size(); ++i)
            {
                auto const pc = classes[(i + rotation) % classes.size()];
                auto number = previous + 1 + ((pc - (previous + 1)) % 12 + 12) % 12;
                numbers.push_back(number);
                previous = number;
            }

            return numbers;
        }
    }

    // ---------------------------------------------------------------- curves

    _Use_decl_annotations_
    double ShapeCurve(CurveShape shape, double x, double amount) noexcept
    {
        x = std::clamp(x, 0.0, 1.0);
        amount = std::clamp(amount, 0.0, 1.0);
        auto const power = 1.0 + 3.0 * amount;

        switch (shape)
        {
        case CurveShape::FastFirst:
            return 1.0 - std::pow(1.0 - x, power);
        case CurveShape::SlowFirst:
            return std::pow(x, power);
        case CurveShape::SCurve:
        {
            auto const smooth = x * x * (3.0 - 2.0 * x);
            return x + (smooth - x) * amount;
        }
        case CurveShape::Straight:
        default:
            return x;
        }
    }

    // ---------------------------------------------------------------- chords

    _Use_decl_annotations_
    std::vector<std::vector<Note>> FindChords(std::vector<Note> const& notes, NoteToolContext const& context, double windowMs)
    {
        std::vector<std::vector<Note>> chords{};

        for (auto const& note : Sorted(notes))
        {
            if (!chords.empty())
            {
                auto const first = chords.back().front().Tick;

                if (note.Tick - first <= std::max<int64_t>(0, TicksForMs(context, first, windowMs)))
                {
                    chords.back().push_back(note);
                    continue;
                }
            }

            chords.push_back({ note });
        }

        for (auto& chord : chords)
        {
            std::stable_sort(chord.begin(), chord.end(), [](Note const& a, Note const& b) { return a.Number < b.Number; });
        }

        return chords;
    }

    // ---------------------------------------------------------------- timing

    _Use_decl_annotations_
    NoteToolResult Quantize(std::vector<Note> const& notes, NoteToolContext const&, QuantizeSettings const& settings)
    {
        if (settings.Grid <= 0)
        {
            return {};
        }

        auto const grid = settings.Grid;
        auto const swing = std::clamp(settings.Swing, 0.5, 0.75);
        auto const strength = std::clamp(settings.Strength, 0.0, 1.0);
        auto const keep = std::clamp(settings.Keep, 0.0, 0.5) * static_cast<double>(grid);

        // The nearest grid point, with every second one pushed later by the swing.
        auto const nearest = [grid, swing](int64_t tick)
        {
            auto const pair = (tick / (grid * 2)) * grid * 2;
            auto const swung = pair + static_cast<int64_t>(std::llround(static_cast<double>(grid * 2) * swing));
            int64_t best = pair;

            for (auto const candidate : { swung, pair + grid * 2 })
            {
                if (std::llabs(candidate - tick) < std::llabs(best - tick))
                {
                    best = candidate;
                }
            }

            return best;
        };

        auto const toward = [strength, keep](int64_t from, int64_t to)
        {
            auto const distance = static_cast<double>(to - from);
            return std::abs(distance) <= keep ? from : from + static_cast<int64_t>(std::llround(distance * strength));
        };

        std::vector<Note> after = notes;

        for (auto& note : after)
        {
            auto const end = note.Tick + note.Length;
            note.Tick = std::max<int64_t>(0, toward(note.Tick, nearest(note.Tick)));

            if (settings.Ends)
            {
                auto const endTarget = ((end + grid / 2) / grid) * grid;
                auto const newEnd = toward(end, endTarget);
                note.Length = newEnd > note.Tick ? newEnd - note.Tick : std::max<int64_t>(1, std::min(note.Length, grid));
            }
        }

        return Changes(notes, after);
    }

    _Use_decl_annotations_
    NoteToolResult Humanize(std::vector<Note> const& notes, NoteToolContext const& context, HumanizeSettings const& settings)
    {
        auto const before = Sorted(notes);
        std::vector<Note> after = before;
        std::mt19937 random{ settings.Seed };

        for (auto& note : after)
        {
            // Three draws for every note, whichever amounts are on, so turning one amount up
            // doesn't change what the others did.
            auto const timing = Wobble(random);
            auto const velocity = Wobble(random);
            auto const length = Wobble(random);

            auto const tighter = settings.TighterOnBeat && IsOnBeat(context, note.Tick) ? 0.35 : 1.0;
            auto const ms = timing * std::max(0.0, settings.TimingMs) * tighter;
            note.Tick = std::max<int64_t>(0, note.Tick + TicksForMs(context, note.Tick, ms));

            auto const velocityChange = velocity * std::max(0.0, settings.VelocityPercent) / 100.0;
            note.Velocity = ClampVelocity(static_cast<double>(note.Velocity) * (1.0 + velocityChange));

            auto const lengthChange = length * std::max(0.0, settings.LengthPercent) / 100.0;
            note.Length = std::max<int64_t>(1, std::llround(static_cast<double>(note.Length) * (1.0 + lengthChange)));
        }

        return Changes(before, after);
    }

    _Use_decl_annotations_
    Groove ExtractGroove(std::vector<Note> const& source, NoteToolContext const& context, int64_t grid)
    {
        Groove groove{};
        groove.Grid = std::max<int64_t>(1, grid);

        auto const& meter = MeterAtTick(MeterOf(context), context.ClipStart);
        auto const steps = static_cast<size_t>(std::max<int64_t>(1, TicksPerBar(meter) / groove.Grid));

        std::vector<double> offsets(steps, 0.0);
        std::vector<double> loudness(steps, 0.0);
        std::vector<size_t> counts(steps, 0);
        double total{ 0 };

        for (auto const& note : source)
        {
            auto const into = TicksIntoBar(context, note.Tick);
            auto const index = static_cast<int64_t>(std::llround(static_cast<double>(into) / static_cast<double>(groove.Grid)));
            auto const step = static_cast<size_t>(index) % steps;
            offsets[step] += static_cast<double>(into - index * groove.Grid);
            loudness[step] += static_cast<double>(note.Velocity);
            ++counts[step];
            total += static_cast<double>(note.Velocity);
        }

        auto const average = source.empty() ? 0.0 : total / static_cast<double>(source.size());
        groove.Offsets.resize(steps, 0.0);
        groove.Accents.resize(steps, 1.0);

        for (size_t step = 0; step < steps; ++step)
        {
            if (counts[step] > 0)
            {
                groove.Offsets[step] = offsets[step] / static_cast<double>(counts[step]);

                if (average > 0)
                {
                    groove.Accents[step] = loudness[step] / static_cast<double>(counts[step]) / average;
                }
            }
        }

        return groove;
    }

    _Use_decl_annotations_
    NoteToolResult ApplyGroove(std::vector<Note> const& notes, NoteToolContext const& context, Groove const& groove, GrooveSettings const& settings)
    {
        if (groove.Offsets.empty() || groove.Grid <= 0 || groove.Accents.size() != groove.Offsets.size())
        {
            return {};
        }

        auto const timing = std::clamp(settings.TimingStrength, 0.0, 1.0);
        auto const accents = std::clamp(settings.VelocityStrength, 0.0, 1.0);
        std::vector<Note> after = notes;

        for (auto& note : after)
        {
            auto const into = TicksIntoBar(context, note.Tick);
            auto const index = static_cast<int64_t>(std::llround(static_cast<double>(into) / static_cast<double>(groove.Grid)));
            auto const step = static_cast<size_t>(index) % groove.Offsets.size();

            auto const target = static_cast<double>(note.Tick - (into - index * groove.Grid)) + groove.Offsets[step];
            note.Tick = std::max<int64_t>(0, note.Tick + std::llround((target - static_cast<double>(note.Tick)) * timing));
            note.Velocity = ClampVelocity(static_cast<double>(note.Velocity) * (1.0 + (groove.Accents[step] - 1.0) * accents));
        }

        return Changes(notes, after);
    }

    _Use_decl_annotations_
    NoteToolResult TimeFeel(std::vector<Note> const& notes, TimeFeelSettings const& settings)
    {
        if (notes.size() < 2)
        {
            return {};
        }

        int64_t start{ std::numeric_limits<int64_t>::max() };
        int64_t end{ 0 };

        for (auto const& note : notes)
        {
            start = std::min(start, note.Tick);
            end = std::max(end, note.Tick + note.Length);
        }

        auto const span = static_cast<double>(end - start);

        if (span <= 0)
        {
            return {};
        }

        auto const shape = settings.SpeedUp ? CurveShape::FastFirst : CurveShape::SlowFirst;
        auto const warp = [&](int64_t tick)
        {
            return start + static_cast<int64_t>(std::llround(span * ShapeCurve(shape, static_cast<double>(tick - start) / span, settings.Amount)));
        };

        std::vector<Note> after = notes;

        for (auto& note : after)
        {
            auto const newStart = warp(note.Tick);
            auto const newEnd = warp(note.Tick + note.Length);
            note.Tick = newStart;
            note.Length = std::max<int64_t>(1, newEnd - newStart);
        }

        return Changes(notes, after);
    }

    _Use_decl_annotations_
    NoteToolResult Ratchet(std::vector<Note> const& notes, RatchetSettings const& settings)
    {
        auto const count = std::clamp<uint32_t>(settings.Count, 2, 32);
        auto const gate = std::clamp(settings.Gate, 0.05, 1.0);
        auto const endVelocity = std::clamp(settings.EndVelocity, 0.0, 4.0);
        NoteToolResult result{};

        for (auto const& note : Sorted(notes))
        {
            if (note.Length < static_cast<int64_t>(count))
            {
                continue;
            }

            std::vector<int64_t> starts{};

            for (uint32_t k = 0; k <= count; ++k)
            {
                auto const x = static_cast<double>(k) / static_cast<double>(count);
                starts.push_back(note.Tick + std::llround(static_cast<double>(note.Length) * ShapeCurve(settings.Spacing, x, settings.SpacingAmount)));
            }

            result.Removed.push_back(note);

            for (uint32_t k = 0; k < count; ++k)
            {
                auto repeat = note;
                repeat.Tick = starts[k];
                repeat.Length = std::max<int64_t>(1, std::llround(static_cast<double>(starts[k + 1] - starts[k]) * gate));

                auto const along = static_cast<double>(k) / static_cast<double>(count - 1);
                repeat.Velocity = ClampVelocity(static_cast<double>(note.Velocity) * (1.0 + (endVelocity - 1.0) * along));
                result.Added.push_back(repeat);
            }
        }

        return result;
    }

    // ---------------------------------------------------------------- length

    _Use_decl_annotations_
    NoteToolResult SetLengths(std::vector<Note> const& notes, LengthSettings const& settings)
    {
        auto const before = Sorted(notes);
        std::vector<Note> after = before;

        for (size_t i = 0; i < after.size(); ++i)
        {
            auto& note = after[i];

            switch (settings.Mode)
            {
            case LengthMode::Legato:
            {
                auto const next = std::find_if(before.begin() + static_cast<std::ptrdiff_t>(i), before.end(),
                    [&](Note const& other) { return other.Tick > note.Tick; });

                if (next != before.end())
                {
                    note.Length = std::max<int64_t>(1, next->Tick - note.Tick - std::max<int64_t>(0, settings.Ticks));
                }
                break;
            }
            case LengthMode::Fixed:
                note.Length = std::max<int64_t>(1, settings.Ticks);
                break;
            case LengthMode::Percent:
                note.Length = std::max<int64_t>(1, std::llround(static_cast<double>(note.Length) * std::max(0.0, settings.Percent) / 100.0));
                break;
            }
        }

        return Changes(before, after);
    }

    // ---------------------------------------------------------------- clean up

    _Use_decl_annotations_
    NoteToolResult CleanUp(std::vector<Note> const& notes, NoteToolContext const& context, CleanUpSettings const& settings)
    {
        NoteToolResult result{};

        // By channel and note number, then by start, the loudest first among notes that start
        // together.
        std::map<std::pair<uint8_t, uint8_t>, std::vector<Note>> byNote{};

        for (auto const& note : notes)
        {
            if (settings.Short && note.Length < std::max<int64_t>(1, TicksForMs(context, note.Tick, settings.ShortMs)))
            {
                result.Removed.push_back(note);
                continue;
            }

            byNote[{ note.Channel, note.Number }].push_back(note);
        }

        for (auto& [key, list] : byNote)
        {
            std::stable_sort(list.begin(), list.end(), [](Note const& a, Note const& b)
            {
                return a.Tick != b.Tick ? a.Tick < b.Tick : (a.Velocity != b.Velocity ? a.Velocity > b.Velocity : a.Length > b.Length);
            });

            std::vector<Note> kept{};

            for (auto const& note : list)
            {
                if (settings.Duplicates && !kept.empty() && kept.back().Tick == note.Tick)
                {
                    result.Removed.push_back(note);
                    continue;
                }

                kept.push_back(note);
            }

            if (settings.Overlaps)
            {
                for (size_t i = 0; i + 1 < kept.size(); ++i)
                {
                    auto const& note = kept[i];
                    auto const nextStart = kept[i + 1].Tick;

                    if (note.Tick < nextStart && note.Tick + note.Length > nextStart)
                    {
                        auto shortened = note;
                        shortened.Length = nextStart - note.Tick;
                        result.Removed.push_back(note);
                        result.Added.push_back(shortened);
                    }
                }
            }
        }

        return result;
    }

    _Use_decl_annotations_
    size_t CountSameNoteOverlaps(std::vector<Note> const& notes)
    {
        std::map<std::pair<uint8_t, uint8_t>, std::vector<Note>> byNote{};

        for (auto const& note : notes)
        {
            byNote[{ note.Channel, note.Number }].push_back(note);
        }

        size_t count{ 0 };

        for (auto& [key, list] : byNote)
        {
            std::sort(list.begin(), list.end(), NoteOrder);

            for (size_t i = 0; i + 1 < list.size(); ++i)
            {
                if (list[i].Tick + list[i].Length > list[i + 1].Tick)
                {
                    ++count;
                }
            }
        }

        return count;
    }

    // ---------------------------------------------------------------- pitch

    _Use_decl_annotations_
    NoteToolResult TransposeInKey(std::vector<Note> const& notes, NoteToolContext const& context, int32_t steps)
    {
        std::vector<Note> after = notes;
        auto const degrees = context.InKey ? KeyDegrees(*context.InKey) : std::vector<int32_t>{};

        for (auto& note : after)
        {
            int32_t number{ note.Number };

            if (degrees.empty())
            {
                number += steps;
            }
            else
            {
                auto const size = static_cast<int32_t>(degrees.size());
                auto octave = number / 12;
                auto const pc = number % 12;

                // The key note at or below, and how far above it this note is.
                auto index = size - 1;

                while (index >= 0 && degrees[static_cast<size_t>(index)] > pc)
                {
                    --index;
                }

                auto remainder{ 0 };

                if (index < 0)
                {
                    index = size - 1;
                    --octave;
                    remainder = pc + 12 - degrees[static_cast<size_t>(index)];
                }
                else
                {
                    remainder = pc - degrees[static_cast<size_t>(index)];
                }

                auto const moved = octave * size + index + steps;
                auto const newOctave = moved >= 0 ? moved / size : -((-moved + size - 1) / size);
                auto const newIndex = moved - newOctave * size;
                number = newOctave * 12 + degrees[static_cast<size_t>(newIndex)] + remainder;
            }

            if (number >= 0 && number <= 127)
            {
                note.Number = static_cast<uint8_t>(number);
            }
        }

        return Changes(notes, after);
    }

    _Use_decl_annotations_
    NoteToolResult FitToChords(std::vector<Note> const& notes, NoteToolContext const& context, FitWhich which)
    {
        std::vector<Note> after = notes;

        for (auto& note : after)
        {
            if (which == FitWhich::OnBeats && !IsOnBeat(context, note.Tick))
            {
                continue;
            }

            if (which == FitWhich::Long && note.Length < BeatTicksAt(context, note.Tick))
            {
                continue;
            }

            std::array<bool, 12> const* tones{ nullptr };

            for (auto const& chord : context.Chords)
            {
                if (note.Tick >= chord.Start && note.Tick < chord.End)
                {
                    tones = &chord.Tones;
                    break;
                }
            }

            if (tones == nullptr && context.InKey)
            {
                tones = &*context.InKey;
            }

            if (tones == nullptr)
            {
                continue;
            }

            if (auto const nearest = NearestTone(note.Number, *tones); nearest.has_value())
            {
                note.Number = static_cast<uint8_t>(*nearest);
            }
        }

        return Changes(notes, after);
    }

    _Use_decl_annotations_
    NoteToolResult Voice(std::vector<Note> const& notes, NoteToolContext const& context, VoicingKind kind)
    {
        std::vector<Note> before{};
        std::vector<Note> after{};
        NoteToolResult result{};
        std::vector<int32_t> previous{};

        for (auto const& chord : FindChords(notes, context, ChordWindowMs))
        {
            std::vector<int32_t> numbers{};

            for (auto const& note : chord)
            {
                numbers.push_back(note.Number);
            }

            auto const size = numbers.size();

            switch (kind)
            {
            case VoicingKind::InversionUp:
                if (size >= 2)
                {
                    numbers.front() += 12;
                    Renumber(chord, numbers, before, after);
                }
                break;

            case VoicingKind::InversionDown:
                if (size >= 2)
                {
                    numbers.back() -= 12;
                    Renumber(chord, numbers, before, after);
                }
                break;

            case VoicingKind::Close:
            case VoicingKind::Open:
                if (size >= 2)
                {
                    auto const lowest = numbers.front();
                    std::vector<int32_t> close{ lowest };

                    for (size_t i = 1; i < size; ++i)
                    {
                        auto above = (numbers[i] - lowest) % 12;
                        close.push_back(lowest + (above == 0 ? 12 : above));
                    }

                    std::sort(close.begin(), close.end());

                    if (kind == VoicingKind::Open && size >= 3)
                    {
                        close[1] += 12;
                    }

                    Renumber(chord, close, before, after);
                }
                break;

            case VoicingKind::Drop2:
                if (size >= 3)
                {
                    numbers[size - 2] -= 12;
                    Renumber(chord, numbers, before, after);
                }
                break;

            case VoicingKind::Drop3:
                if (size >= 4)
                {
                    numbers[size - 3] -= 12;
                    Renumber(chord, numbers, before, after);
                }
                break;

            case VoicingKind::OctaveBelow:
                if (numbers.front() >= 12)
                {
                    auto added = chord.front();
                    added.Number = static_cast<uint8_t>(numbers.front() - 12);
                    result.Added.push_back(added);
                }
                break;

            case VoicingKind::SmoothLeading:
                if (size >= 2 && !previous.empty())
                {
                    std::vector<int32_t> classes{};

                    for (auto const number : numbers)
                    {
                        classes.push_back(number % 12);
                    }

                    std::sort(classes.begin(), classes.end());

                    auto const previousMean = std::accumulate(previous.begin(), previous.end(), 0.0) / static_cast<double>(previous.size());
                    std::vector<int32_t> best{};
                    double bestCost{ std::numeric_limits<double>::max() };

                    for (size_t rotation = 0; rotation < classes.size(); ++rotation)
                    {
                        for (auto lowest = previous.front() - 12; lowest <= previous.front() + 12; ++lowest)
                        {
                            auto candidate = Stack(classes, rotation, lowest);

                            if (candidate.front() != lowest || candidate.back() > 127 || candidate.front() < 0)
                            {
                                continue;
                            }

                            double cost{ 0 };

                            if (candidate.size() == previous.size())
                            {
                                for (size_t i = 0; i < candidate.size(); ++i)
                                {
                                    cost += std::abs(candidate[i] - previous[i]);
                                }
                            }
                            else
                            {
                                auto const mean = std::accumulate(candidate.begin(), candidate.end(), 0.0) / static_cast<double>(candidate.size());
                                cost = std::abs(mean - previousMean) * static_cast<double>(candidate.size());
                            }

                            // Ties go to the voicing nearest where the chord already was.
                            cost += std::abs(candidate.front() - numbers.front()) * 0.001;

                            if (cost < bestCost)
                            {
                                bestCost = cost;
                                best = std::move(candidate);
                            }
                        }
                    }

                    if (!best.empty())
                    {
                        Renumber(chord, best, before, after);
                        previous = best;
                        continue;
                    }
                }

                previous = numbers;
                break;
            }
        }

        auto changes = Changes(before, after);
        result.Removed = std::move(changes.Removed);
        result.Added.insert(result.Added.begin(), changes.Added.begin(), changes.Added.end());
        return result;
    }

    _Use_decl_annotations_
    NoteToolResult Mirror(std::vector<Note> const& notes, NoteToolContext const& context, uint8_t around, bool inKey)
    {
        std::vector<Note> after = notes;
        std::vector<int32_t> keyNotes{};

        if (inKey && context.InKey)
        {
            for (int32_t number = 0; number <= 127; ++number)
            {
                if ((*context.InKey)[static_cast<size_t>(number % 12)])
                {
                    keyNotes.push_back(number);
                }
            }
        }

        // The axis on the key note at or below it.
        auto const axis = std::upper_bound(keyNotes.begin(), keyNotes.end(), static_cast<int32_t>(around));
        auto const axisIndex = axis == keyNotes.begin() ? 0 : static_cast<int64_t>(axis - keyNotes.begin()) - 1;

        for (auto& note : after)
        {
            int32_t number = 2 * static_cast<int32_t>(around) - note.Number;
            auto const found = std::lower_bound(keyNotes.begin(), keyNotes.end(), static_cast<int32_t>(note.Number));

            if (found != keyNotes.end() && *found == note.Number)
            {
                auto const index = 2 * axisIndex - static_cast<int64_t>(found - keyNotes.begin());
                number = index >= 0 && index < static_cast<int64_t>(keyNotes.size()) ? keyNotes[static_cast<size_t>(index)] : -1;
            }

            if (number >= 0 && number <= 127)
            {
                note.Number = static_cast<uint8_t>(number);
            }
        }

        return Changes(notes, after);
    }

    _Use_decl_annotations_
    NoteToolResult Reverse(std::vector<Note> const& notes)
    {
        if (notes.size() < 2)
        {
            return {};
        }

        int64_t start{ std::numeric_limits<int64_t>::max() };
        int64_t end{ 0 };

        for (auto const& note : notes)
        {
            start = std::min(start, note.Tick);
            end = std::max(end, note.Tick + note.Length);
        }

        std::vector<Note> after = notes;

        for (auto& note : after)
        {
            note.Tick = start + end - (note.Tick + note.Length);
        }

        return Changes(notes, after);
    }

    // ---------------------------------------------------------------- values

    _Use_decl_annotations_
    NoteToolResult SetValues(std::vector<Note> const& notes, NoteToolContext const& context, ValueSettings const& settings)
    {
        auto const before = Sorted(notes);

        if (before.empty())
        {
            return {};
        }

        auto const get = [&](Note const& note) -> double
        {
            switch (settings.Target)
            {
            case ValueTarget::ReleaseVelocity: return static_cast<double>(note.ReleaseVelocity) / 655.35;
            case ValueTarget::Chance: return static_cast<double>(note.Chance);
            case ValueTarget::Velocity:
            default: return static_cast<double>(note.Velocity) / 655.35;
            }
        };

        auto const set = [&](Note& note, double percent)
        {
            switch (settings.Target)
            {
            case ValueTarget::ReleaseVelocity:
                note.ReleaseVelocity = static_cast<uint16_t>(std::clamp(std::round(percent * 655.35), 0.0, 65535.0));
                break;
            case ValueTarget::Chance:
                note.Chance = static_cast<uint8_t>(std::clamp(std::round(percent), 0.0, 100.0));
                break;
            case ValueTarget::Velocity:
            default:
                note.Velocity = ClampVelocity(percent * 655.35);
                break;
            }
        };

        double sum{ 0 };

        for (auto const& note : before)
        {
            sum += get(note);
        }

        auto const average = sum / static_cast<double>(before.size());
        auto const first = before.front().Tick;
        auto const span = static_cast<double>(before.back().Tick - first);
        std::mt19937 random{ settings.Seed };
        std::vector<Note> after = before;

        for (auto& note : after)
        {
            auto const current = get(note);
            auto const wobble = Wobble(random);
            double value{ current };

            switch (settings.Mode)
            {
            case ValueMode::Set:
                value = settings.First;
                break;
            case ValueMode::Change:
                value = current + settings.First;
                break;
            case ValueMode::Scale:
                value = average + (current - average) * std::max(0.0, settings.First) / 100.0;
                break;
            case ValueMode::Ramp:
            {
                auto const x = span > 0 ? static_cast<double>(note.Tick - first) / span : 0.0;
                value = settings.First + (settings.Second - settings.First) * ShapeCurve(settings.Curve, x, settings.CurveAmount);
                break;
            }
            case ValueMode::Accent:
                if (IsOnDownbeat(context, note.Tick))
                {
                    value = current + settings.First * 2.0;
                }
                else if (IsOnBeat(context, note.Tick))
                {
                    value = current + settings.First;
                }
                break;
            case ValueMode::Random:
                value = current + wobble * settings.First;
                break;
            }

            set(note, value);
        }

        return Changes(before, after);
    }

    // ---------------------------------------------------------------- selecting

    _Use_decl_annotations_
    std::vector<Note> SelectNotes(std::vector<Note> const& all, std::vector<Note> const& selection, NoteToolContext const& context, SelectSettings const& settings)
    {
        auto const& source = selection.empty() ? all : selection;
        std::vector<Note> picked{};

        switch (settings.Kind)
        {
        case SelectKind::TopOfChords:
        case SelectKind::BottomOfChords:
            for (auto const& chord : FindChords(source, context, ChordWindowMs))
            {
                picked.push_back(settings.Kind == SelectKind::TopOfChords ? chord.back() : chord.front());
            }
            break;

        case SelectKind::SameNotes:
        {
            std::array<bool, 128> numbers{};

            for (auto const& note : selection)
            {
                numbers[note.Number] = true;
            }

            std::copy_if(all.begin(), all.end(), std::back_inserter(picked), [&](Note const& note) { return numbers[note.Number]; });
            break;
        }

        case SelectKind::EveryNth:
        {
            auto const every = std::max<uint32_t>(1, settings.Every);
            auto const sorted = Sorted(source);

            for (size_t i = 0; i < sorted.size(); ++i)
            {
                if (i % every == settings.Offset % every)
                {
                    picked.push_back(sorted[i]);
                }
            }
            break;
        }

        case SelectKind::OnBeats:
        case SelectKind::OffBeats:
            std::copy_if(source.begin(), source.end(), std::back_inserter(picked), [&](Note const& note)
            {
                return IsOnBeat(context, note.Tick) == (settings.Kind == SelectKind::OnBeats);
            });
            break;

        case SelectKind::VelocityRange:
        {
            auto const low = std::min(settings.Low, settings.High);
            auto const high = std::max(settings.Low, settings.High);
            std::copy_if(source.begin(), source.end(), std::back_inserter(picked), [&](Note const& note)
            {
                auto const percent = static_cast<double>(note.Velocity) / 655.35;
                return percent >= low - 0.005 && percent <= high + 0.005;
            });
            break;
        }

        case SelectKind::OutsideKey:
            if (context.InKey)
            {
                std::copy_if(source.begin(), source.end(), std::back_inserter(picked), [&](Note const& note)
                {
                    return !(*context.InKey)[note.Number % 12];
                });
            }
            break;

        case SelectKind::Invert:
        {
            auto remaining = Sorted(selection);

            for (auto const& note : Sorted(all))
            {
                auto const found = std::find(remaining.begin(), remaining.end(), note);

                if (found != remaining.end())
                {
                    remaining.erase(found);
                }
                else
                {
                    picked.push_back(note);
                }
            }
            break;
        }
        }

        return Sorted(std::move(picked));
    }
}
