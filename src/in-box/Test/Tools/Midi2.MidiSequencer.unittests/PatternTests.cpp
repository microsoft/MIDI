// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#include "PatternTests.h"
#include "SequenceTestData.h"

#include "SequenceEdits.h"
#include "SequencePattern.h"
#include "SequenceRender.h"
#include "SequenceSerializer.h"
#include "SequenceUndo.h"

#include <algorithm>

using namespace WEX::Common;
using namespace WEX::Logging;
using namespace WEX::TestExecution;
using namespace midisequencer;

namespace
{
    constexpr int64_t Bar = TicksPerQuarterNote * 4;
    constexpr int64_t Step = TicksPerQuarterNote / 4;

    bool Contains(std::wstring const& text, std::wstring_view part)
    {
        return text.find(part) != std::wstring::npos;
    }

    PatternStep On(uint16_t index)
    {
        PatternStep step{};
        step.Index = index;
        return step;
    }

    // One track playing one pattern clip from the start, for a number of bars.
    Sequence PatternSequence(PatternSettings pattern, int64_t bars, uint32_t seed = 1234)
    {
        Sequence sequence{};
        sequence.Tempo = { TempoPoint{ 0, 120.0, false } };

        Clip clip{};
        clip.Id = L"c-p";
        clip.Kind = ClipKind::Pattern;
        clip.Name = L"Beat";
        clip.Seed = seed;
        clip.Pattern = std::move(pattern);
        sequence.Clips = { clip };

        Track track{};
        track.Id = L"t-1";
        track.Name = L"Drums";
        track.Destination.Endpoint = EndpointRef{ L"Drum machine", L"" };
        track.Timeline = { Placement{ L"c-p", 0, bars * Bar } };
        sequence.Tracks = { track };

        NormalizeSequence(sequence);
        return sequence;
    }

    std::vector<RenderedMessage> RenderAll(Sequence const& sequence, int64_t from, int64_t to)
    {
        std::vector<RenderedMessage> messages{};
        RenderTrack(sequence, sequence.Tracks[0], from, to, messages, RenderOptions{});
        return messages;
    }

    std::vector<RenderedMessage> NoteOns(Sequence const& sequence, int64_t from, int64_t to)
    {
        auto messages = RenderAll(sequence, from, to);
        std::erase_if(messages, [](RenderedMessage const& message) { return message.Kind != RenderedKind::NoteOn; });
        return messages;
    }

    std::vector<int64_t> Ticks(std::vector<RenderedMessage> const& messages)
    {
        std::vector<int64_t> ticks{};

        for (auto const& message : messages)
        {
            ticks.push_back(message.Tick);
        }

        return ticks;
    }

    uint8_t NoteNumber(RenderedMessage const& message) noexcept { return static_cast<uint8_t>((message.Words[0] >> 8) & 0x7F); }
    uint8_t Channel(RenderedMessage const& message) noexcept { return static_cast<uint8_t>((message.Words[0] >> 16) & 0x0F); }
    uint16_t Velocity(RenderedMessage const& message) noexcept { return static_cast<uint16_t>(message.Words[1] >> 16); }

    bool SameMessages(std::vector<RenderedMessage> const& a, std::vector<RenderedMessage> const& b)
    {
        if (a.size() != b.size())
        {
            return false;
        }

        for (size_t i = 0; i < a.size(); ++i)
        {
            if (a[i].Tick != b[i].Tick || a[i].Kind != b[i].Kind || a[i].Words != b[i].Words || a[i].WordCount != b[i].WordCount)
            {
                return false;
            }
        }

        return true;
    }

    // A pattern that uses most of what a step can do.
    PatternSettings BusyPattern()
    {
        auto pattern = MakeDrumPattern();
        pattern.Swing = 60;

        auto& kick = pattern.Lanes[0];
        kick.Steps = { On(0), On(4), On(8), On(12), On(14) };
        kick.Steps[4].Chance = 35;

        auto& snare = pattern.Lanes[1];
        snare.Steps = { On(4), On(12), On(15) };
        snare.Steps[0].Nudge = -20;
        snare.Steps[2].Velocity = 9000;
        snare.Steps[2].Condition = StepCondition::Every;
        snare.Steps[2].Every = 2;
        snare.Steps[2].Of = 2;

        auto& hat = pattern.Lanes[2];
        hat.StepCount = 12;
        hat.Steps = { On(0), On(2), On(3), On(6), On(9) };
        hat.Steps[2].Ratchet = 3;
        hat.Steps[2].Shape = RatchetShape::Softer;
        hat.Steps[4].Chance = 60;

        auto& open = pattern.Lanes[3];
        open.StepCount = 7;
        open.Steps = { On(0), On(5) };
        open.Steps[1].Condition = StepCondition::NotFirstPass;
        open.Steps[1].Length = 300;

        return pattern;
    }
}

void PatternTests::APatternPlaysItsSteps()
{
    auto pattern = MakeDrumPattern();
    pattern.Lanes[0].Steps = { On(0), On(4), On(8), On(12) };
    pattern.Lanes[1].Steps = { On(4) };

    auto const sequence = PatternSequence(pattern, 1);
    VERIFY_ARE_EQUAL(Bar, sequence.Clips[0].Length);

    auto const ons = NoteOns(sequence, 0, Bar);
    VERIFY_ARE_EQUAL(size_t{ 5 }, ons.size());
    VERIFY_IS_TRUE((std::vector<int64_t>{ 0, 960, 960, 1920, 2880 }) == Ticks(ons));

    VERIFY_ARE_EQUAL(uint8_t{ 36 }, NoteNumber(ons[0]));
    VERIFY_ARE_EQUAL(uint8_t{ 36 }, NoteNumber(ons[1]));
    VERIFY_ARE_EQUAL(uint8_t{ 38 }, NoteNumber(ons[2]));

    // On channel 10, the General MIDI drum channel, at the default velocity.
    VERIFY_ARE_EQUAL(uint8_t{ 9 }, Channel(ons[0]));
    VERIFY_ARE_EQUAL(PatternStep{}.Velocity, Velocity(ons[0]));

    // Each hit lasts half a step.
    auto const all = RenderAll(sequence, 0, Bar);
    auto const firstOff = std::find_if(all.begin(), all.end(), [](RenderedMessage const& message) { return message.Kind == RenderedKind::NoteOff; });
    VERIFY_IS_TRUE(firstOff != all.end());
    VERIFY_ARE_EQUAL(Step / 2, firstOff->Tick);
}

void PatternTests::LanesKeepTheirOwnLength()
{
    auto pattern = MakeDrumPattern();
    pattern.Lanes[0].Steps = { On(0) };
    pattern.Lanes[2].StepCount = 12;
    pattern.Lanes[2].Steps = { On(0) };

    // The 12-step hat comes round every 12 steps while the kick goes round every 16.
    auto sequence = PatternSequence(pattern, 3);
    auto hats = NoteOns(sequence, 0, 3 * Bar);
    std::erase_if(hats, [](RenderedMessage const& message) { return NoteNumber(message) != 42; });
    VERIFY_IS_TRUE((std::vector<int64_t>{ 0, 12 * Step, 24 * Step, 36 * Step }) == Ticks(hats));

    // They line up again after 48 steps, and a 7-step lane with a step on makes it 336.
    VERIFY_ARE_EQUAL(uint64_t{ 48 }, PatternCycleSteps(pattern, false, 100000));
    pattern.Lanes[3].StepCount = 7;
    pattern.Lanes[3].Steps = { On(1) };
    VERIFY_ARE_EQUAL(uint64_t{ 336 }, PatternCycleSteps(pattern, false, 100000));
    VERIFY_ARE_EQUAL(uint64_t{ 100 }, PatternCycleSteps(pattern, false, 100));

    // Turned off, every lane runs the pattern's 16 steps.
    pattern.LanesKeepLength = false;
    sequence = PatternSequence(pattern, 3);
    hats = NoteOns(sequence, 0, 3 * Bar);
    std::erase_if(hats, [](RenderedMessage const& message) { return NoteNumber(message) != 42; });
    VERIFY_IS_TRUE((std::vector<int64_t>{ 0, Bar, 2 * Bar }) == Ticks(hats));

    // A new step count moves the lanes that ran the old one. The 12 and 7 step lanes keep theirs.
    SetPatternStepCount(pattern, 8);
    VERIFY_ARE_EQUAL(uint16_t{ 8 }, pattern.StepCount);
    VERIFY_ARE_EQUAL(uint16_t{ 8 }, pattern.Lanes[0].StepCount);
    VERIFY_ARE_EQUAL(uint16_t{ 8 }, pattern.Lanes[1].StepCount);
    VERIFY_ARE_EQUAL(uint16_t{ 12 }, pattern.Lanes[2].StepCount);
    VERIFY_ARE_EQUAL(uint16_t{ 7 }, pattern.Lanes[3].StepCount);
}

void PatternTests::SwingAndNudgeMoveSteps()
{
    auto pattern = MakeDrumPattern();
    pattern.Swing = 75;
    pattern.Lanes[0].Steps = { On(0), On(1), On(2), On(3) };

    auto const swung = NoteOns(PatternSequence(pattern, 1), 0, Bar);
    VERIFY_IS_TRUE((std::vector<int64_t>{ 0, Step + Step / 2, 2 * Step, 3 * Step + Step / 2 }) == Ticks(swung));

    // A nudge moves one step, never more than half a step, and never before the clip starts.
    pattern.Swing = 50;
    pattern.Lanes[0].Steps = { On(0), On(4), On(8) };
    pattern.Lanes[0].Steps[0].Nudge = -60;
    pattern.Lanes[0].Steps[1].Nudge = -30;
    pattern.Lanes[0].Steps[2].Nudge = 5000;

    auto const nudged = NoteOns(PatternSequence(pattern, 1), 0, Bar);
    VERIFY_IS_TRUE((std::vector<int64_t>{ 0, 4 * Step - 30, 8 * Step + Step / 2 }) == Ticks(nudged));
}

void PatternTests::RatchetsRepeatInsideTheStep()
{
    auto pattern = MakeDrumPattern();
    pattern.Lanes[0].Steps = { On(0) };
    pattern.Lanes[0].Steps[0].Ratchet = 3;
    pattern.Lanes[0].Steps[0].Shape = RatchetShape::Softer;
    pattern.Lanes[0].Steps[0].Velocity = 50000;

    auto const sequence = PatternSequence(pattern, 1);
    auto const ons = NoteOns(sequence, 0, Bar);
    VERIFY_IS_TRUE((std::vector<int64_t>{ 0, 80, 160 }) == Ticks(ons));
    VERIFY_ARE_EQUAL(uint16_t{ 50000 }, Velocity(ons[0]));
    VERIFY_ARE_EQUAL(uint16_t{ 40000 }, Velocity(ons[1]));
    VERIFY_ARE_EQUAL(uint16_t{ 30000 }, Velocity(ons[2]));

    // Each repeat lasts half of its share of the step.
    auto const all = RenderAll(sequence, 0, Bar);
    auto const firstOff = std::find_if(all.begin(), all.end(), [](RenderedMessage const& message) { return message.Kind == RenderedKind::NoteOff; });
    VERIFY_ARE_EQUAL(int64_t{ 40 }, firstOff->Tick);

    // Louder rises to the step's velocity on the last repeat.
    pattern.Lanes[0].Steps[0].Ratchet = 4;
    pattern.Lanes[0].Steps[0].Shape = RatchetShape::Louder;
    auto const louder = NoteOns(PatternSequence(pattern, 1), 0, Bar);
    VERIFY_ARE_EQUAL(size_t{ 4 }, louder.size());
    VERIFY_IS_TRUE(Velocity(louder[0]) < Velocity(louder[1]));
    VERIFY_IS_TRUE(Velocity(louder[1]) < Velocity(louder[2]));
    VERIFY_ARE_EQUAL(uint16_t{ 50000 }, Velocity(louder[3]));
}

void PatternTests::ConditionsCountPassesThroughTheLane()
{
    auto pattern = MakeDrumPattern();

    pattern.Lanes[0].Steps = { On(0) };
    pattern.Lanes[0].Steps[0].Condition = StepCondition::Every;
    pattern.Lanes[0].Steps[0].Every = 2;
    pattern.Lanes[0].Steps[0].Of = 1;

    pattern.Lanes[1].Steps = { On(0) };
    pattern.Lanes[1].Steps[0].Condition = StepCondition::FirstPass;

    pattern.Lanes[2].Steps = { On(0) };
    pattern.Lanes[2].Steps[0].Condition = StepCondition::NotFirstPass;

    // A 12-step lane counts its own passes.
    pattern.Lanes[3].StepCount = 12;
    pattern.Lanes[3].Steps = { On(0) };
    pattern.Lanes[3].Steps[0].Condition = StepCondition::Every;
    pattern.Lanes[3].Steps[0].Every = 2;
    pattern.Lanes[3].Steps[0].Of = 2;

    auto const ons = NoteOns(PatternSequence(pattern, 4), 0, 4 * Bar);

    auto const of = [&ons](uint8_t note)
    {
        std::vector<int64_t> ticks{};

        for (auto const& message : ons)
        {
            if (NoteNumber(message) == note)
            {
                ticks.push_back(message.Tick);
            }
        }

        return ticks;
    };

    VERIFY_IS_TRUE((std::vector<int64_t>{ 0, 2 * Bar }) == of(36));
    VERIFY_IS_TRUE((std::vector<int64_t>{ 0 }) == of(38));
    VERIFY_IS_TRUE((std::vector<int64_t>{ Bar, 2 * Bar, 3 * Bar }) == of(42));
    VERIFY_IS_TRUE((std::vector<int64_t>{ 12 * Step, 36 * Step, 60 * Step }) == of(46));

    // With conditions, the kick's lane takes 2 passes of 16 to come round and the 12-step lane 2
    // passes of 12, so everything lines up after 96 steps.
    VERIFY_ARE_EQUAL(uint64_t{ 96 }, PatternCycleSteps(pattern, true, 100000));
}

void PatternTests::ChanceIsTheSameEveryTime()
{
    auto pattern = MakeDrumPattern();

    for (uint16_t i = 0; i < 16; ++i)
    {
        pattern.Lanes[0].Steps.push_back(On(i));
        pattern.Lanes[0].Steps.back().Chance = 50;
    }

    auto const sequence = PatternSequence(pattern, 8, 777);
    auto const first = NoteOns(sequence, 0, 8 * Bar);
    auto const second = NoteOns(sequence, 0, 8 * Bar);

    VERIFY_IS_TRUE(SameMessages(first, second));
    VERIFY_IS_TRUE(first.size() > 128 * 3 / 10);
    VERIFY_IS_TRUE(first.size() < 128 * 7 / 10);

    // A new seed, which is what Vary does, gives other hits.
    auto const varied = NoteOns(PatternSequence(pattern, 8, 778), 0, 8 * Bar);
    VERIFY_IS_FALSE(SameMessages(first, varied));

    // Moving the lane doesn't change which of its steps play.
    auto moved = sequence;
    std::rotate(moved.Clips[0].Pattern->Lanes.begin(), moved.Clips[0].Pattern->Lanes.begin() + 1, moved.Clips[0].Pattern->Lanes.end());
    VERIFY_IS_TRUE(SameMessages(first, NoteOns(moved, 0, 8 * Bar)));

    // Never and always.
    for (auto& step : pattern.Lanes[0].Steps) { step.Chance = 0; }
    VERIFY_IS_TRUE(NoteOns(PatternSequence(pattern, 8), 0, 8 * Bar).empty());

    for (auto& step : pattern.Lanes[0].Steps) { step.Chance = 100; }
    VERIFY_ARE_EQUAL(size_t{ 128 }, NoteOns(PatternSequence(pattern, 8), 0, 8 * Bar).size());
}

void PatternTests::MelodicStepsPlayTheirOwnNotes()
{
    auto pattern = MakeMelodicPattern();
    VERIFY_ARE_EQUAL(size_t{ 1 }, pattern.Lanes.size());

    pattern.Lanes[0].Steps = { On(0), On(1), On(2) };
    pattern.Lanes[0].Steps[0].Note = 40;
    pattern.Lanes[0].Steps[1].Note = 43;
    pattern.Lanes[0].Steps[1].Accent = true;
    pattern.Lanes[0].Steps[2].Note = 45;
    pattern.Lanes[0].Steps[2].Slide = true;

    auto const sequence = PatternSequence(pattern, 1);
    auto const ons = NoteOns(sequence, 0, Bar);
    VERIFY_ARE_EQUAL(size_t{ 3 }, ons.size());
    VERIFY_ARE_EQUAL(uint8_t{ 40 }, NoteNumber(ons[0]));
    VERIFY_ARE_EQUAL(uint8_t{ 43 }, NoteNumber(ons[1]));
    VERIFY_ARE_EQUAL(uint8_t{ 45 }, NoteNumber(ons[2]));
    VERIFY_ARE_EQUAL(uint8_t{ 0 }, Channel(ons[0]));

    // An accent plays at full velocity.
    VERIFY_ARE_EQUAL(uint16_t{ 0xFFFF }, Velocity(ons[1]));

    // A slide runs on into the next step.
    auto const all = RenderAll(sequence, 0, Bar);
    auto const slideOff = std::find_if(all.begin(), all.end(), [](RenderedMessage const& message)
    {
        return message.Kind == RenderedKind::NoteOff && NoteNumber(message) == 45;
    });

    VERIFY_ARE_EQUAL(2 * Step + Step + Step / 4, slideOff->Tick);

    // A melodic pattern has exactly one lane.
    pattern.Lanes.push_back(MakePatternLane(60, 16));
    NormalizePattern(pattern);
    VERIFY_ARE_EQUAL(size_t{ 1 }, pattern.Lanes.size());
}

void PatternTests::MutedAndSoloedLanes()
{
    auto pattern = MakeDrumPattern();
    pattern.Lanes[0].Steps = { On(0) };
    pattern.Lanes[1].Steps = { On(0) };

    pattern.Lanes[0].Muted = true;
    auto ons = NoteOns(PatternSequence(pattern, 1), 0, Bar);
    VERIFY_ARE_EQUAL(size_t{ 1 }, ons.size());
    VERIFY_ARE_EQUAL(uint8_t{ 38 }, NoteNumber(ons[0]));

    // A soloed lane with nothing on silences the rest.
    pattern.Lanes[0].Muted = false;
    pattern.Lanes[2].Soloed = true;
    VERIFY_IS_TRUE(NoteOns(PatternSequence(pattern, 1), 0, Bar).empty());

    pattern.Lanes[0].Soloed = true;
    ons = NoteOns(PatternSequence(pattern, 1), 0, Bar);
    VERIFY_ARE_EQUAL(size_t{ 1 }, ons.size());
    VERIFY_ARE_EQUAL(uint8_t{ 36 }, NoteNumber(ons[0]));
}

void PatternTests::APieceAtATimeMatchesTheWhole()
{
    auto const sequence = PatternSequence(BusyPattern(), 8);
    auto const whole = RenderAll(sequence, 0, 8 * Bar);
    VERIFY_IS_TRUE(whole.size() > 100);

    // The engine renders in short windows. Every hit comes out once, whatever the window.
    for (int64_t const window : { int64_t{ 37 }, int64_t{ 240 }, int64_t{ 1001 } })
    {
        std::vector<RenderedMessage> pieces{};

        for (int64_t from = 0; from < 8 * Bar; from += window)
        {
            RenderTrack(sequence, sequence.Tracks[0], from, std::min(from + window, 8 * Bar), pieces, RenderOptions{});
        }

        std::vector<RenderedMessage> wholeOns{ whole };
        std::erase_if(wholeOns, [](RenderedMessage const& message) { return message.Kind != RenderedKind::NoteOn; });
        std::erase_if(pieces, [](RenderedMessage const& message) { return message.Kind != RenderedKind::NoteOn; });

        Log::Comment(String().Format(L"Window %lld: %zu note ons, %zu whole", window, pieces.size(), wholeOns.size()));
        VERIFY_IS_TRUE(SameMessages(wholeOns, pieces));
    }
}

void PatternTests::PatternsRoundTripThroughTheFile()
{
    auto sequence = PatternSequence(BusyPattern(), 4);

    auto melodic = MakePatternClip(L"Acid line", PatternMode::Melodic);
    melodic.Pattern->Lanes[0].Steps = { On(0), On(3) };
    melodic.Pattern->Lanes[0].Steps[0].Note = 40;
    melodic.Pattern->Lanes[0].Steps[1].Accent = true;
    melodic.Pattern->Lanes[0].Steps[1].Slide = true;
    sequence.Clips.push_back(melodic);
    sequence.Clips[0].Pattern->Lanes[1].Name = L"Ghost snare";
    sequence.Clips[0].Pattern->Lanes[3].Muted = true;
    NormalizeSequence(sequence);

    auto const text = WriteSequenceJson(sequence);

    VERIFY_IS_TRUE(Contains(text, L"\"kind\": \"pattern\""));
    VERIFY_IS_TRUE(Contains(text, L"\"mode\": \"melodic\""));
    VERIFY_IS_TRUE(Contains(text, L"\"stepCount\": 12"));
    VERIFY_IS_TRUE(Contains(text, L"\"swing\": 60"));
    VERIFY_IS_TRUE(Contains(text, L"\"condition\": \"2:2\""));
    VERIFY_IS_TRUE(Contains(text, L"\"condition\": \"notFirstPass\""));
    VERIFY_IS_TRUE(Contains(text, L"\"ratchet\": 3, \"shape\": \"softer\""));
    VERIFY_IS_TRUE(Contains(text, L"\"note\": 40"));
    VERIFY_IS_TRUE(Contains(text, L"\"accent\": true, \"slide\": true"));

    // A pattern's notes come from its steps, so they aren't written.
    VERIFY_IS_FALSE(Contains(text, L"\"notes\""));

    Sequence read{};
    VERIFY_IS_TRUE(ReadSequenceJson(text, read).Succeeded());
    VERIFY_ARE_EQUAL(size_t{ 2 }, read.Clips.size());

    for (size_t c = 0; c < 2; ++c)
    {
        auto const& before = *sequence.Clips[c].Pattern;
        auto const& after = *read.Clips[c].Pattern;

        VERIFY_ARE_EQUAL(static_cast<int>(before.Mode), static_cast<int>(after.Mode));
        VERIFY_ARE_EQUAL(before.StepCount, after.StepCount);
        VERIFY_ARE_EQUAL(before.StepTicks, after.StepTicks);
        VERIFY_ARE_EQUAL(before.Swing, after.Swing);
        VERIFY_ARE_EQUAL(before.LanesKeepLength, after.LanesKeepLength);
        VERIFY_ARE_EQUAL(before.Channel, after.Channel);
        VERIFY_ARE_EQUAL(before.Lanes.size(), after.Lanes.size());

        for (size_t i = 0; i < before.Lanes.size(); ++i)
        {
            VERIFY_ARE_EQUAL(before.Lanes[i].Id, after.Lanes[i].Id);
            VERIFY_ARE_EQUAL(before.Lanes[i].Name, after.Lanes[i].Name);
            VERIFY_ARE_EQUAL(before.Lanes[i].Note, after.Lanes[i].Note);
            VERIFY_ARE_EQUAL(before.Lanes[i].StepCount, after.Lanes[i].StepCount);
            VERIFY_ARE_EQUAL(before.Lanes[i].Muted, after.Lanes[i].Muted);
            VERIFY_ARE_EQUAL(before.Lanes[i].Soloed, after.Lanes[i].Soloed);
            VERIFY_IS_TRUE(before.Lanes[i].Steps == after.Lanes[i].Steps);
        }

        // The drawn notes are worked out again when it's read.
        VERIFY_ARE_EQUAL(sequence.Clips[c].Notes.size(), read.Clips[c].Notes.size());
        VERIFY_ARE_EQUAL(sequence.Clips[c].Length, read.Clips[c].Length);
    }

    VERIFY_ARE_EQUAL(text, WriteSequenceJson(read));
}

void PatternTests::DamagedPatternsAreClampedOrSkipped()
{
    std::wstring const text = LR"({
  "fileVersion": 1,
  "tracks": [],
  "clips": [ { "id": "c-1", "kind": "pattern", "name": "P", "length": 1,
    "notes": [ [0, 100, 0, 60, 1000] ],
    "settings": { "mode": "drums", "stepCount": 500, "stepTicks": 480, "swing": 99, "channel": 40, "futureKey": 1,
      "lanes": [
        { "id": "p-1", "note": 300, "stepCount": 0, "steps": [ [0, 70000], [3, 100, { "chance": 250, "ratchet": 30, "condition": "9:2", "length": 0, "nudge": 99999, "shape": "wobbly" }], [64, 1], "junk", [2] ] },
        "junk"
      ] } } ]
})";

    Sequence read{};
    auto const result = ReadSequenceJson(text, read);
    VERIFY_IS_TRUE(result.Succeeded());

    // Three steps and one lane that couldn't be used.
    VERIFY_ARE_EQUAL(size_t{ 4 }, result.SkippedItems);

    auto const& pattern = *read.Clips[0].Pattern;
    VERIFY_ARE_EQUAL(uint16_t{ MaximumPatternSteps }, pattern.StepCount);
    VERIFY_ARE_EQUAL(int64_t{ 480 }, pattern.StepTicks);
    VERIFY_ARE_EQUAL(MaximumSwing, pattern.Swing);
    VERIFY_ARE_EQUAL(uint8_t{ 15 }, pattern.Channel);
    VERIFY_ARE_EQUAL(size_t{ 1 }, pattern.Lanes.size());

    auto const& lane = pattern.Lanes[0];
    VERIFY_ARE_EQUAL(uint8_t{ 127 }, lane.Note);
    VERIFY_ARE_EQUAL(uint16_t{ 1 }, lane.StepCount);
    VERIFY_ARE_EQUAL(size_t{ 2 }, lane.Steps.size());
    VERIFY_ARE_EQUAL(uint16_t{ 0xFFFF }, lane.Steps[0].Velocity);

    auto const& step = lane.Steps[1];
    VERIFY_ARE_EQUAL(uint8_t{ 100 }, step.Chance);
    VERIFY_ARE_EQUAL(MaximumRatchet, step.Ratchet);
    VERIFY_ARE_EQUAL(static_cast<int>(StepCondition::Always), static_cast<int>(step.Condition));
    VERIFY_ARE_EQUAL(static_cast<int>(RatchetShape::Even), static_cast<int>(step.Shape));
    VERIFY_ARE_EQUAL(uint16_t{ 1 }, step.Length);
    VERIFY_ARE_EQUAL(int16_t{ 240 }, step.Nudge);

    // The notes written with it are left out, and its length comes from its steps.
    VERIFY_ARE_EQUAL(int64_t{ 64 * 480 }, read.Clips[0].Length);
    VERIFY_IS_TRUE(std::all_of(read.Clips[0].Notes.begin(), read.Clips[0].Notes.end(), [](Note const& note) { return note.Number == 127; }));

    VERIFY_IS_TRUE(Contains(WriteSequenceJson(read), L"\"futureKey\": 1"));
}

void PatternTests::ThePreviewFollowsThePattern()
{
    auto clip = MakePatternClip(L"Beat", PatternMode::Drums);
    VERIFY_ARE_EQUAL(static_cast<int>(ClipKind::Pattern), static_cast<int>(clip.Kind));
    VERIFY_ARE_EQUAL(size_t{ 4 }, clip.Pattern->Lanes.size());
    VERIFY_ARE_EQUAL(Bar, clip.Length);
    VERIFY_IS_TRUE(clip.Notes.empty());

    clip.Pattern->Lanes[0].Steps = { On(0), On(8) };
    clip.Pattern->Lanes[0].Steps[1].Chance = 40;
    RefreshPatternPreview(clip);

    // Every step that could play is drawn, with its chance on the note.
    VERIFY_ARE_EQUAL(size_t{ 2 }, clip.Notes.size());
    VERIFY_ARE_EQUAL(8 * Step, clip.Notes[1].Tick);
    VERIFY_ARE_EQUAL(uint8_t{ 40 }, clip.Notes[1].Chance);

    // Fewer, longer steps change the clip's length.
    clip.Pattern->StepCount = 8;
    clip.Pattern->StepTicks = 2 * Step;
    RefreshPatternPreview(clip);
    VERIFY_ARE_EQUAL(Bar, clip.Length);
    VERIFY_ARE_EQUAL(size_t{ 1 }, clip.Notes.size());

    // Turning steps on and off.
    auto& lane = clip.Pattern->Lanes[1];
    VERIFY_IS_NOT_NULL(SetStepOn(lane, 5, true, lane.Note));
    VERIFY_IS_NOT_NULL(SetStepOn(lane, 2, true, lane.Note));
    VERIFY_ARE_EQUAL(uint16_t{ 2 }, lane.Steps[0].Index);
    VERIFY_IS_NOT_NULL(FindStep(lane, 5));
    VERIFY_IS_NULL(SetStepOn(lane, 5, false, lane.Note));
    VERIFY_IS_NULL(FindStep(lane, 5));
    VERIFY_ARE_EQUAL(size_t{ 1 }, lane.Steps.size());
}

void PatternTests::MakeNotesPlaysWhatThePatternPlays()
{
    auto pattern = BusyPattern();

    // Chance is kept on the notes, and a notes clip rolls it differently, so leave it out here.
    for (auto& lane : pattern.Lanes)
    {
        for (auto& step : lane.Steps)
        {
            step.Chance = 100;
        }
    }

    auto sequence = PatternSequence(pattern, 16);
    auto const notes = MakeNotesFromPattern(sequence.Clips[0], 64 * Bar);

    // Lanes of 16, 12 and 7 steps, and a snare every 2nd pass of 16: they line up after 672 steps.
    VERIFY_ARE_EQUAL(static_cast<int>(ClipKind::Notes), static_cast<int>(notes.Kind));
    VERIFY_IS_FALSE(notes.Pattern.has_value());
    VERIFY_ARE_EQUAL(int64_t{ 672 } * Step, notes.Length);
    VERIFY_ARE_NOT_EQUAL(sequence.Clips[0].Id, notes.Id);

    // Placed for the same 16 bars, it plays exactly what the pattern played.
    auto copy = sequence;
    copy.Clips.push_back(notes);
    copy.Tracks[0].Timeline = { Placement{ notes.Id, 0, 16 * Bar } };

    VERIFY_IS_TRUE(SameMessages(NoteOns(sequence, 0, 16 * Bar), NoteOns(copy, 0, 16 * Bar)));

    // A step's chance stays on its note.
    sequence.Clips[0].Pattern->Lanes[0].Steps[4].Chance = 35;
    auto const withChance = MakeNotesFromPattern(sequence.Clips[0], 64 * Bar);
    VERIFY_IS_TRUE(std::any_of(withChance.Notes.begin(), withChance.Notes.end(), [](Note const& note) { return note.Chance == 35; }));

    // A short ceiling stops at whole passes.
    VERIFY_ARE_EQUAL(4 * Bar, MakeNotesFromPattern(sequence.Clips[0], 4 * Bar + 100).Length);
}

void PatternTests::PatternEditsUndo()
{
    auto pattern = MakeDrumPattern();
    pattern.Lanes[0].Steps = { On(0) };

    auto sequence = PatternSequence(pattern, 1);
    auto& clip = sequence.Clips[0];
    VERIFY_ARE_EQUAL(size_t{ 1 }, clip.Notes.size());

    auto const before = *clip.Pattern;
    auto after = before;
    SetStepOn(after.Lanes[1], 4, true, after.Lanes[1].Note);
    after.StepCount = 8;

    clip.Pattern = after;
    RefreshPatternPreview(clip);

    UndoStack undo{};
    ChangeList changes{};
    changes.push_back(MakeClipPatternChange(clip.Id, before, after));
    undo.Commit(L"Edit pattern", std::move(changes));

    VERIFY_ARE_EQUAL(size_t{ 2 }, sequence.Clips[0].Notes.size());
    VERIFY_ARE_EQUAL(8 * Step, sequence.Clips[0].Length);

    VERIFY_IS_TRUE(undo.Undo(sequence));
    VERIFY_ARE_EQUAL(size_t{ 1 }, sequence.Clips[0].Notes.size());
    VERIFY_ARE_EQUAL(Bar, sequence.Clips[0].Length);

    VERIFY_IS_TRUE(undo.Redo(sequence));
    VERIFY_ARE_EQUAL(size_t{ 2 }, sequence.Clips[0].Notes.size());
    VERIFY_ARE_EQUAL(8 * Step, sequence.Clips[0].Length);
}

void PatternTests::DraggedLanesMoveInPlace()
{
    auto const order = [](PatternSettings const& pattern)
    {
        std::wstring names{};

        for (auto const& lane : pattern.Lanes)
        {
            names += lane.Name;
        }

        return names;
    };

    PatternSettings pattern{};

    for (auto const name : { L"A", L"B", L"C", L"D" })
    {
        auto lane = MakePatternLane(36, 16);
        lane.Name = name;
        pattern.Lanes.push_back(lane);
    }

    // Down, to the end, and up to the top. A drop lands before the lane it's on.
    VERIFY_IS_TRUE(MovePatternLane(pattern, 0, 2));
    VERIFY_ARE_EQUAL(std::wstring{ L"BACD" }, order(pattern));

    VERIFY_IS_TRUE(MovePatternLane(pattern, 1, 4));
    VERIFY_ARE_EQUAL(std::wstring{ L"BCDA" }, order(pattern));

    VERIFY_IS_TRUE(MovePatternLane(pattern, 3, 0));
    VERIFY_ARE_EQUAL(std::wstring{ L"ABCD" }, order(pattern));

    // Dropped where it already is, or out of range, nothing moves.
    VERIFY_IS_FALSE(MovePatternLane(pattern, 1, 1));
    VERIFY_IS_FALSE(MovePatternLane(pattern, 1, 2));
    VERIFY_IS_FALSE(MovePatternLane(pattern, 4, 0));
    VERIFY_IS_FALSE(MovePatternLane(pattern, 0, 5));
    VERIFY_ARE_EQUAL(std::wstring{ L"ABCD" }, order(pattern));

    // A lane takes its steps with it.
    pattern.Lanes[2].Steps = { On(3) };
    VERIFY_IS_TRUE(MovePatternLane(pattern, 2, 0));
    VERIFY_ARE_EQUAL(std::wstring{ L"CABD" }, order(pattern));
    VERIFY_ARE_EQUAL(size_t{ 1 }, pattern.Lanes[0].Steps.size());
    VERIFY_IS_TRUE(pattern.Lanes[2].Steps.empty());
}
