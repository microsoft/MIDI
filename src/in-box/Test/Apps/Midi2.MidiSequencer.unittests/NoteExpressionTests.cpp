// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#include "NoteExpressionTests.h"

#include "NoteExpression.h"
#include "SequenceTestData.h"

#include <algorithm>
#include <cstdlib>
#include <vector>

using namespace WEX::Common;
using namespace WEX::Logging;
using namespace WEX::TestExecution;
using namespace midisequencer;
using testdata::MakeNote;

namespace
{
    std::vector<ClipEvent> BendsOn(std::vector<ClipEvent> const& events, uint8_t number)
    {
        std::vector<ClipEvent> bends{};

        for (auto const& event : events)
        {
            if (auto const bent = PerNotePitchBendOf(event); bent.has_value() && bent->Number == number)
            {
                bends.push_back(event);
            }
        }

        return bends;
    }
}

void NoteExpressionTests::MessagesAreShapedAsTheSpecSays()
{
    // Type 4, status 6, channel, note number, reserved; then the 32-bit value.
    auto const bend = PerNotePitchBend(10, 3, 60, PitchBendCenter);
    VERIFY_ARE_EQUAL(uint32_t{ 0x40633C00 }, bend.Words[0]);
    VERIFY_ARE_EQUAL(PitchBendCenter, bend.Words[1]);
    VERIFY_ARE_EQUAL(uint8_t{ 2 }, bend.WordCount);

    auto const bent = PerNotePitchBendOf(bend);
    VERIFY_IS_TRUE(bent.has_value());
    VERIFY_ARE_EQUAL(uint8_t{ 3 }, bent->Channel);
    VERIFY_ARE_EQUAL(uint8_t{ 60 }, bent->Number);

    // Registered Controller bank 0, index 7, as 7.25 fixed point: 12 semitones.
    auto const range = PerNoteBendRange(0, 2, 12.0);
    VERIFY_ARE_EQUAL(uint32_t{ 0x40220007 }, range.Words[0]);
    VERIFY_ARE_EQUAL(uint32_t{ 0x18000000 }, range.Words[1]);
    VERIFY_IS_FALSE(PerNotePitchBendOf(range).has_value());
}

void NoteExpressionTests::BendValuesStayInRange()
{
    VERIFY_ARE_EQUAL(PitchBendCenter, BendValue(0.0, 2.0));
    VERIFY_ARE_EQUAL(uint32_t{ 0xFFFFFFFF }, BendValue(2.0, 2.0));
    VERIFY_ARE_EQUAL(uint32_t{ 0 }, BendValue(-2.0, 2.0));
    VERIFY_ARE_EQUAL(uint32_t{ 0xC0000000 }, BendValue(1.0, 2.0));
    VERIFY_ARE_EQUAL(uint32_t{ 0xFFFFFFFF }, BendValue(5.0, 2.0), L"held inside the range");
    VERIFY_ARE_EQUAL(PitchBendCenter, BendValue(1.0, 0.0));
}

void NoteExpressionTests::GlideSlidesIntoTheNextNote()
{
    NoteToolContext context{};
    std::vector<Note> notes{ MakeNote(0, 480, 60), MakeNote(480, 480, 64) };

    GlideSettings settings{};
    auto const result = Glide({}, notes, context, settings);
    auto const bends = BendsOn(result.Events, 64);

    VERIFY_ARE_EQUAL(size_t{ 12 }, bends.size(), L"a bend before the note and 11 steps");
    VERIFY_ARE_EQUAL(result.Added, bends.size());
    VERIFY_IS_TRUE(BendsOn(result.Events, 60).empty());

    // Starts four semitones down, at the note before's pitch, and ends in tune.
    VERIFY_ARE_EQUAL(int64_t{ 479 }, bends.front().Tick);
    VERIFY_ARE_EQUAL(BendValue(-4.0, 12.0), bends.front().Words[1]);
    VERIFY_ARE_EQUAL(PitchBendCenter, bends.back().Words[1]);
    VERIFY_IS_TRUE(std::is_sorted(bends.begin(), bends.end(), [](ClipEvent const& a, ClipEvent const& b) { return a.Tick < b.Tick; }));

    // 90 ms at 120 BPM is about 173 ticks.
    VERIFY_IS_TRUE(std::llabs(bends.back().Tick - (480 + 173)) <= 1);

    // Again replaces it, and the range comes once when asked for.
    settings.SendRange = true;
    auto const again = Glide(result.Events, notes, context, settings);
    VERIFY_ARE_EQUAL(size_t{ 12 }, again.Removed);
    VERIFY_ARE_EQUAL(size_t{ 13 }, again.Events.size());
    VERIFY_ARE_EQUAL(PerNoteBendRange(0, 0, 12.0), again.Events.front());
}

void NoteExpressionTests::GlideSkipsChordsGapsAndLeapsTooBig()
{
    NoteToolContext context{};
    GlideSettings settings{};

    std::vector<Note> chord{ MakeNote(0, 480, 60), MakeNote(0, 480, 64) };
    VERIFY_ARE_EQUAL(size_t{ 0 }, Glide({}, chord, context, settings).Added);

    std::vector<Note> gap{ MakeNote(0, 240, 60), MakeNote(480, 480, 64) };
    VERIFY_ARE_EQUAL(size_t{ 0 }, Glide({}, gap, context, settings).Added);

    settings.OnlyJoined = false;
    VERIFY_ARE_EQUAL(size_t{ 12 }, Glide({}, gap, context, settings).Added);

    std::vector<Note> leap{ MakeNote(0, 480, 60), MakeNote(480, 480, 80) };
    VERIFY_ARE_EQUAL(size_t{ 0 }, Glide({}, leap, context, settings).Added, L"20 semitones is past a 12 semitone range");
}

void NoteExpressionTests::VibratoSwellsAndComesBackToTheMiddle()
{
    NoteToolContext context{};
    std::vector<Note> notes{ MakeNote(0, 1920, 60), MakeNote(2000, 100, 62) };

    VibratoSettings settings{};
    settings.RateHz = 5.0;
    auto const result = Vibrato({}, notes, context, settings);
    auto const bends = BendsOn(result.Events, 60);

    // A second of note, starting 200 ms in, a step every 12.5 ms, and one back to the middle.
    VERIFY_IS_GREATER_THAN_OR_EQUAL(bends.size(), size_t{ 60 });
    VERIFY_IS_LESS_THAN_OR_EQUAL(bends.size(), size_t{ 66 });
    VERIFY_IS_TRUE(std::llabs(bends.front().Tick - 384) <= 1, L"200 ms in");
    VERIFY_ARE_EQUAL(int64_t{ 1919 }, bends.back().Tick);
    VERIFY_ARE_EQUAL(PitchBendCenter, bends.back().Words[1]);

    auto const most = BendValue(0.30, 2.0) - PitchBendCenter;

    for (auto const& bend : bends)
    {
        auto const away = bend.Words[1] >= PitchBendCenter ? bend.Words[1] - PitchBendCenter : PitchBendCenter - bend.Words[1];
        VERIFY_IS_LESS_THAN_OR_EQUAL(away, most + 1);
    }

    VERIFY_IS_TRUE(BendsOn(result.Events, 62).empty(), L"too short for vibrato");
}

void NoteExpressionTests::RemoveTakesOnlyThoseNotesBends()
{
    NoteToolContext context{};
    std::vector<Note> notes{ MakeNote(0, 480, 60), MakeNote(480, 480, 64) };

    auto events = Glide({}, notes, context, GlideSettings{}).Events;
    events.push_back(testdata::MakeEvent(500, 0x40B00700, 0x80000000));

    auto const removed = RemovePerNoteBend(events, { notes[1] });
    VERIFY_ARE_EQUAL(size_t{ 12 }, removed.Removed);
    VERIFY_ARE_EQUAL(size_t{ 1 }, removed.Events.size(), L"the control change stays");
}