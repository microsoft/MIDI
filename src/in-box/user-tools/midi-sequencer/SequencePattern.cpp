// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#include "SequencePattern.h"

#include <algorithm>
#include <cmath>
#include <numeric>
#include <unordered_set>

namespace midisequencer
{
    namespace
    {
        uint64_t Mix(_In_ uint64_t value) noexcept
        {
            value += 0x9E3779B97F4A7C15ull;
            value = (value ^ (value >> 30)) * 0xBF58476D1CE4E5B9ull;
            value = (value ^ (value >> 27)) * 0x94D049BB133111EBull;
            return value ^ (value >> 31);
        }

        // From the lane's id rather than its place in the list, so moving a lane doesn't change
        // which of its steps play.
        uint64_t HashText(_In_ std::wstring_view text) noexcept
        {
            uint64_t hash{ 0xCBF29CE484222325ull };

            for (auto const c : text)
            {
                hash ^= static_cast<uint16_t>(c);
                hash *= 0x100000001B3ull;
            }

            return hash;
        }

        bool ConditionHolds(_In_ PatternStep const& step, _In_ uint64_t lanePass) noexcept
        {
            switch (step.Condition)
            {
            case StepCondition::Every:
            {
                auto const every = static_cast<uint64_t>(std::max<uint8_t>(step.Every, 1));
                auto const of = static_cast<uint64_t>(std::clamp<uint8_t>(step.Of, 1, std::max<uint8_t>(step.Every, 1)));
                return lanePass % every == of - 1;
            }

            case StepCondition::FirstPass:
                return lanePass == 0;

            case StepCondition::NotFirstPass:
                return lanePass > 0;

            case StepCondition::Always:
            default:
                return true;
            }
        }

        uint16_t ShapedVelocity(_In_ uint16_t velocity, _In_ RatchetShape shape, _In_ uint8_t repeat, _In_ uint8_t count) noexcept
        {
            auto factor = 1.0;

            if (shape == RatchetShape::Softer)
            {
                factor = std::max(0.2, 1.0 - 0.2 * repeat);
            }
            else if (shape == RatchetShape::Louder && count > 1)
            {
                factor = 0.45 + 0.55 * static_cast<double>(repeat) / static_cast<double>(count - 1);
            }

            return static_cast<uint16_t>(std::clamp<long>(std::lround(static_cast<double>(velocity) * factor), 1, 65535));
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

        int64_t ClampedNudge(_In_ int16_t nudge, _In_ int64_t stepTicks) noexcept
        {
            auto const reach = stepTicks / 2;
            return std::clamp<int64_t>(nudge, -reach, reach);
        }
    }

    PatternSettings MakeDrumPattern()
    {
        PatternSettings pattern{};
        pattern.Mode = PatternMode::Drums;
        pattern.Channel = 9;

        // Kick, snare, closed hi-hat and open hi-hat in the General MIDI drum map.
        for (uint8_t const note : { uint8_t{ 36 }, uint8_t{ 38 }, uint8_t{ 42 }, uint8_t{ 46 } })
        {
            pattern.Lanes.push_back(MakePatternLane(note, pattern.StepCount));
        }

        return pattern;
    }

    PatternSettings MakeMelodicPattern()
    {
        PatternSettings pattern{};
        pattern.Mode = PatternMode::Melodic;
        pattern.Channel = 0;
        pattern.Lanes.push_back(MakePatternLane(48, pattern.StepCount));
        return pattern;
    }

    _Use_decl_annotations_
    PatternLane MakePatternLane(uint8_t note, uint16_t stepCount)
    {
        PatternLane lane{};
        lane.Id = NewId(L"p");
        lane.Note = static_cast<uint8_t>(note & 0x7F);
        lane.StepCount = std::clamp<uint16_t>(stepCount, 1, MaximumPatternSteps);
        return lane;
    }

    _Use_decl_annotations_
    void NormalizePattern(PatternSettings& pattern)
    {
        if (pattern.Mode != PatternMode::Drums && pattern.Mode != PatternMode::Melodic)
        {
            pattern.Mode = PatternMode::Drums;
        }

        pattern.StepCount = std::clamp<uint16_t>(pattern.StepCount, 1, MaximumPatternSteps);
        pattern.StepTicks = std::clamp<int64_t>(pattern.StepTicks, 1, TicksPerQuarterNote * 4);
        pattern.Swing = std::clamp(pattern.Swing, MinimumSwing, MaximumSwing);
        pattern.Channel &= 0x0F;

        if (pattern.Lanes.size() > MaximumPatternLanes)
        {
            pattern.Lanes.resize(MaximumPatternLanes);
        }

        if (pattern.Mode == PatternMode::Melodic)
        {
            if (pattern.Lanes.empty())
            {
                pattern.Lanes.push_back(MakePatternLane(48, pattern.StepCount));
            }
            else if (pattern.Lanes.size() > 1)
            {
                pattern.Lanes.resize(1);
            }
        }

        std::unordered_set<std::wstring> ids{};
        auto const reach = pattern.StepTicks / 2;

        for (auto& lane : pattern.Lanes)
        {
            if (lane.Id.empty() || ids.contains(lane.Id))
            {
                lane.Id = NewId(L"p");
            }

            ids.insert(lane.Id);
            lane.Note &= 0x7F;
            lane.StepCount = std::clamp<uint16_t>(lane.StepCount, 1, MaximumPatternSteps);

            std::erase_if(lane.Steps, [](PatternStep const& step) { return step.Index >= MaximumPatternSteps; });
            std::stable_sort(lane.Steps.begin(), lane.Steps.end(), [](PatternStep const& a, PatternStep const& b) { return a.Index < b.Index; });

            // One step per index: the last one written wins.
            std::vector<PatternStep> unique{};
            unique.reserve(lane.Steps.size());

            for (size_t i = 0; i < lane.Steps.size(); ++i)
            {
                if (i + 1 < lane.Steps.size() && lane.Steps[i + 1].Index == lane.Steps[i].Index)
                {
                    continue;
                }

                unique.push_back(lane.Steps[i]);
            }

            lane.Steps = std::move(unique);

            for (auto& step : lane.Steps)
            {
                step.Chance = std::min<uint8_t>(step.Chance, 100);
                step.Ratchet = std::clamp<uint8_t>(step.Ratchet, 1, MaximumRatchet);

                if (step.Shape > RatchetShape::Louder)
                {
                    step.Shape = RatchetShape::Even;
                }

                if (step.Condition > StepCondition::NotFirstPass)
                {
                    step.Condition = StepCondition::Always;
                }

                step.Every = std::clamp<uint8_t>(step.Every, 2, 16);
                step.Of = std::clamp<uint8_t>(step.Of, 1, step.Every);
                step.Nudge = static_cast<int16_t>(std::clamp<int64_t>(step.Nudge, std::max<int64_t>(-reach, INT16_MIN), std::min<int64_t>(reach, INT16_MAX)));
                step.Length = std::clamp<uint16_t>(step.Length, 1, MaximumStepLength);
                step.Note = pattern.Mode == PatternMode::Drums ? lane.Note : static_cast<uint8_t>(step.Note & 0x7F);
            }
        }
    }

    _Use_decl_annotations_
    uint16_t LaneStepCount(PatternSettings const& pattern, PatternLane const& lane) noexcept
    {
        auto const count = pattern.LanesKeepLength ? lane.StepCount : pattern.StepCount;
        return std::clamp<uint16_t>(count, 1, MaximumPatternSteps);
    }

    _Use_decl_annotations_
    void SetPatternStepCount(PatternSettings& pattern, uint16_t count) noexcept
    {
        count = std::clamp<uint16_t>(count, 1, MaximumPatternSteps);

        for (auto& lane : pattern.Lanes)
        {
            if (lane.StepCount == pattern.StepCount || pattern.Mode == PatternMode::Melodic)
            {
                lane.StepCount = count;
            }
        }

        pattern.StepCount = count;
    }

    _Use_decl_annotations_
    bool MovePatternLane(PatternSettings& pattern, size_t lane, size_t before) noexcept
    {
        auto const count = pattern.Lanes.size();

        if (lane >= count || before > count || before == lane || before == lane + 1)
        {
            return false;
        }

        auto const lanes = pattern.Lanes.begin();
        auto const from = static_cast<ptrdiff_t>(lane);
        auto const to = static_cast<ptrdiff_t>(before);

        // Turned in place, so nothing is copied or allocated.
        if (to < from)
        {
            std::rotate(lanes + to, lanes + from, lanes + from + 1);
        }
        else
        {
            std::rotate(lanes + from, lanes + from + 1, lanes + to);
        }

        return true;
    }

    _Use_decl_annotations_
    PatternStep const* FindStep(PatternLane const& lane, uint16_t index) noexcept
    {
        auto const found = std::lower_bound(lane.Steps.begin(), lane.Steps.end(), index,
            [](PatternStep const& step, uint16_t value) { return step.Index < value; });

        return found != lane.Steps.end() && found->Index == index ? &*found : nullptr;
    }

    _Use_decl_annotations_
    PatternStep* FindStep(PatternLane& lane, uint16_t index) noexcept
    {
        auto const found = std::lower_bound(lane.Steps.begin(), lane.Steps.end(), index,
            [](PatternStep const& step, uint16_t value) { return step.Index < value; });

        return found != lane.Steps.end() && found->Index == index ? &*found : nullptr;
    }

    _Use_decl_annotations_
    PatternStep* SetStepOn(PatternLane& lane, uint16_t index, bool on, uint8_t note)
    {
        if (index >= MaximumPatternSteps)
        {
            return nullptr;
        }

        auto const at = std::lower_bound(lane.Steps.begin(), lane.Steps.end(), index,
            [](PatternStep const& step, uint16_t value) { return step.Index < value; });

        auto const exists = at != lane.Steps.end() && at->Index == index;

        if (!on)
        {
            if (exists)
            {
                lane.Steps.erase(at);
            }

            return nullptr;
        }

        if (exists)
        {
            return &*at;
        }

        PatternStep step{};
        step.Index = index;
        step.Note = static_cast<uint8_t>(note & 0x7F);

        return &*lane.Steps.insert(at, step);
    }

    _Use_decl_annotations_
    bool StepPlaysOnPass(PatternStep const& step, uint32_t clipSeed, std::wstring_view laneId, uint64_t lanePass) noexcept
    {
        if (!ConditionHolds(step, lanePass))
        {
            return false;
        }

        if (step.Chance >= 100)
        {
            return true;
        }

        if (step.Chance == 0)
        {
            return false;
        }

        auto const mixed = Mix((static_cast<uint64_t>(clipSeed) << 32) ^ Mix(lanePass) ^ HashText(laneId) ^
            (static_cast<uint64_t>(step.Index) * 0x9E3779B97F4A7C15ull));

        return mixed % 100 < step.Chance;
    }

    _Use_decl_annotations_
    void ForEachPatternNote(
        Clip const& clip,
        int64_t passStart,
        int64_t passEnd,
        uint64_t pass,
        int64_t fromTick,
        int64_t toTick,
        bool applyChance,
        std::function<void(Note const&)> const& add)
    {
        if (!clip.Pattern.has_value())
        {
            return;
        }

        auto const& pattern = *clip.Pattern;
        auto const stepTicks = std::max<int64_t>(1, pattern.StepTicks);
        auto const stepCount = static_cast<int64_t>(std::clamp<uint16_t>(pattern.StepCount, 1, MaximumPatternSteps));

        auto const windowStart = std::max(fromTick, passStart);
        auto const windowEnd = std::min(toTick, passEnd);

        if (windowStart >= windowEnd || pattern.Lanes.empty())
        {
            return;
        }

        auto const swing = static_cast<int64_t>(std::llround(static_cast<double>(stepTicks) *
            static_cast<double>(std::clamp(pattern.Swing, MinimumSwing, MaximumSwing) - 50) / 50.0));

        // A step's notes start at most half a step early (a nudge) and less than two steps late
        // (swing, a nudge and its last ratchet), so only the steps near the window can matter.
        auto const firstStep = std::max<int64_t>(0, (windowStart - passStart) / stepTicks - 3);
        auto const lastStep = std::min<int64_t>(stepCount - 1, (windowEnd - passStart) / stepTicks + 3);

        auto const anySolo = std::any_of(pattern.Lanes.begin(), pattern.Lanes.end(), [](PatternLane const& lane) { return lane.Soloed; });
        auto const melodic = pattern.Mode == PatternMode::Melodic;

        for (auto local = firstStep; local <= lastStep; ++local)
        {
            auto const global = pass * static_cast<uint64_t>(stepCount) + static_cast<uint64_t>(local);

            for (auto const& lane : pattern.Lanes)
            {
                if (lane.Muted || (anySolo && !lane.Soloed))
                {
                    continue;
                }

                auto const laneSteps = static_cast<uint64_t>(LaneStepCount(pattern, lane));
                auto const step = FindStep(lane, static_cast<uint16_t>(global % laneSteps));

                if (step == nullptr)
                {
                    continue;
                }

                auto const lanePass = global / laneSteps;

                if (applyChance ? !StepPlaysOnPass(*step, clip.Seed, lane.Id, lanePass) : !ConditionHolds(*step, lanePass))
                {
                    continue;
                }

                auto const count = std::clamp<uint8_t>(step->Ratchet, 1, MaximumRatchet);
                auto const sub = std::max<int64_t>(1, stepTicks / count);
                auto const base = passStart + local * stepTicks + (global % 2 == 1 ? swing : 0) + ClampedNudge(step->Nudge, stepTicks);

                int64_t length{ 0 };

                if (melodic && step->Slide && count == 1)
                {
                    // Runs on into the next step, so a synth set to glide on overlapping notes slides.
                    length = stepTicks + stepTicks / 4;
                }
                else
                {
                    auto const span = count > 1 ? sub : stepTicks;
                    length = std::max<int64_t>(1, span * std::clamp<uint16_t>(step->Length, 1, MaximumStepLength) / 100);

                    if (count > 1)
                    {
                        length = std::min(length, sub);
                    }
                }

                auto const velocity = melodic && step->Accent ? uint16_t{ 0xFFFF } : step->Velocity;

                for (uint8_t repeat = 0; repeat < count; ++repeat)
                {
                    auto const on = std::max(passStart, base + repeat * sub);

                    if (on >= passEnd)
                    {
                        break;
                    }

                    if (on < windowStart || on >= windowEnd)
                    {
                        continue;
                    }

                    Note note{};
                    note.Tick = on;
                    note.Length = length;
                    note.Channel = static_cast<uint8_t>(pattern.Channel & 0x0F);
                    note.Number = static_cast<uint8_t>((melodic ? step->Note : lane.Note) & 0x7F);
                    note.Velocity = ShapedVelocity(velocity, step->Shape, repeat, count);
                    note.ReleaseVelocity = 0x8000;
                    note.Chance = applyChance ? uint8_t{ 100 } : step->Chance;

                    add(note);
                }
            }
        }
    }

    _Use_decl_annotations_
    void RefreshPatternPreview(Clip& clip)
    {
        if (clip.Kind != ClipKind::Pattern || !clip.Pattern.has_value())
        {
            return;
        }

        clip.Length = std::max<int64_t>(1, clip.Pattern->StepCount) * std::max<int64_t>(1, clip.Pattern->StepTicks);

        std::vector<Note> notes{};
        ForEachPatternNote(clip, 0, clip.Length, 0, 0, clip.Length, false, [&notes](Note const& note) { notes.push_back(note); });

        clip.Notes = std::move(notes);

        std::stable_sort(clip.Notes.begin(), clip.Notes.end(), [](Note const& a, Note const& b)
        {
            return a.Tick != b.Tick ? a.Tick < b.Tick : a.Number < b.Number;
        });
    }

    _Use_decl_annotations_
    uint64_t PatternCycleSteps(PatternSettings const& pattern, bool withConditions, uint64_t limit) noexcept
    {
        limit = std::max<uint64_t>(1, limit);

        auto cycle = static_cast<uint64_t>(std::clamp<uint16_t>(pattern.StepCount, 1, MaximumPatternSteps));

        for (auto const& lane : pattern.Lanes)
        {
            if (lane.Steps.empty())
            {
                continue;
            }

            auto laneCycle = static_cast<uint64_t>(LaneStepCount(pattern, lane));

            if (withConditions)
            {
                uint64_t passes{ 1 };

                for (auto const& step : lane.Steps)
                {
                    if (step.Condition == StepCondition::Every)
                    {
                        passes = CappedLcm(passes, std::max<uint8_t>(step.Every, 1), limit);
                    }
                }

                laneCycle = laneCycle > limit / std::max<uint64_t>(1, passes) ? limit + 1 : laneCycle * passes;
            }

            cycle = CappedLcm(cycle, laneCycle, limit);

            if (cycle > limit)
            {
                return limit;
            }
        }

        return std::min(cycle, limit);
    }

    _Use_decl_annotations_
    Clip MakeNotesFromPattern(Clip const& pattern, int64_t maximumTicks)
    {
        Clip notes{};
        notes.Id = NewId(L"c");
        notes.Kind = ClipKind::Notes;
        notes.Name = pattern.Name;
        notes.Color = pattern.Color;
        notes.Length = pattern.Length;
        notes.Loop = pattern.Loop;
        notes.Seed = pattern.Seed;
        notes.Origin = pattern.Origin;
        notes.OriginDetail = pattern.OriginDetail;

        if (!pattern.Pattern.has_value())
        {
            notes.Notes = pattern.Notes;
            notes.Events = pattern.Events;
            notes.Lanes = pattern.Lanes;
            return notes;
        }

        auto const& settings = *pattern.Pattern;
        auto const stepTicks = std::max<int64_t>(1, settings.StepTicks);
        auto const stepCount = static_cast<uint64_t>(std::clamp<uint16_t>(settings.StepCount, 1, MaximumPatternSteps));
        auto const passLength = static_cast<int64_t>(stepCount) * stepTicks;

        auto const limitSteps = static_cast<uint64_t>(std::max<int64_t>(passLength, maximumTicks) / stepTicks);
        auto const cycle = PatternCycleSteps(settings, true, limitSteps);

        auto passes = std::max<uint64_t>(1, (cycle + stepCount - 1) / stepCount);

        while (passes > 1 && static_cast<int64_t>(passes) * passLength > maximumTicks)
        {
            --passes;
        }

        notes.Length = static_cast<int64_t>(passes) * passLength;

        for (uint64_t pass = 0; pass < passes; ++pass)
        {
            auto const start = static_cast<int64_t>(pass) * passLength;

            ForEachPatternNote(pattern, start, start + passLength, pass, start, start + passLength, false,
                [&notes](Note const& note) { notes.Notes.push_back(note); });

            for (auto event : pattern.Events)
            {
                if (event.Tick < passLength)
                {
                    event.Tick += start;
                    notes.Events.push_back(event);
                }
            }
        }

        // The clip's own lanes went round with each pass, so they're written out once per pass.
        for (auto const& lane : pattern.Lanes)
        {
            auto copy = lane;
            copy.Points.clear();

            for (uint64_t pass = 0; pass < passes; ++pass)
            {
                auto const start = static_cast<int64_t>(pass) * passLength;

                for (auto point : lane.Points)
                {
                    if (point.Tick < passLength)
                    {
                        point.Tick += start;
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
