// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#include "AutomationTests.h"
#include "SequenceTestData.h"

#include "ArrangeLayout.h"
#include "MessageTranslation.h"
#include "SequenceEdits.h"
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
    constexpr uint32_t Top = 0xFFFFFFFFu;

    bool Contains(std::wstring const& text, std::wstring_view part)
    {
        return text.find(part) != std::wstring::npos;
    }

    AutomationLane MakeRamp(LaneTarget target, uint16_t number, std::vector<LanePoint> points)
    {
        auto lane = MakeLane(target, number, 0);
        lane.Points = std::move(points);
        return lane;
    }

    // One track with one looping one-bar clip in the sequence, and nothing placed yet.
    Sequence LaneSequence()
    {
        Sequence sequence{};
        sequence.Tempo = { TempoPoint{ 0, 120.0, false } };

        Clip clip{};
        clip.Id = L"c-1";
        clip.Length = Bar;
        clip.Loop = true;
        sequence.Clips = { clip };

        Track track{};
        track.Id = L"t-1";
        track.Name = L"Bass";
        track.Destination.Endpoint = EndpointRef{ L"Synth", L"" };
        sequence.Tracks = { track };

        return sequence;
    }

    std::vector<RenderedMessage> Render(Sequence const& sequence, int64_t from, int64_t to, RenderOptions const& options)
    {
        std::vector<RenderedMessage> messages{};
        RenderTrack(sequence, sequence.Tracks[0], from, to, messages, options);
        return messages;
    }

    uint32_t Level7(RenderedMessage const& message) noexcept
    {
        return ScaleDown(message.Words[1], 32, 7);
    }
}

void AutomationTests::LaneValuesFollowThePoints()
{
    auto lane = MakeLane(LaneTarget::Controller, 74, 0);
    VERIFY_IS_FALSE(LaneValueAt(lane, 0).has_value());

    lane.Points = {
        LanePoint{ 960, 1000, false },
        LanePoint{ 1960, 3000, true },
        LanePoint{ 2960, 500, false },
        LanePoint{ 2960, 9000, false },
    };

    // Held before the first point, a straight line between points, held after a hold point, and
    // a jump where two points share a tick.
    VERIFY_ARE_EQUAL(uint32_t{ 1000 }, LaneValueAt(lane, 0).value());
    VERIFY_ARE_EQUAL(uint32_t{ 1000 }, LaneValueAt(lane, 960).value());
    VERIFY_ARE_EQUAL(uint32_t{ 2000 }, LaneValueAt(lane, 1460).value());
    VERIFY_ARE_EQUAL(uint32_t{ 3000 }, LaneValueAt(lane, 2959).value());
    VERIFY_ARE_EQUAL(uint32_t{ 9000 }, LaneValueAt(lane, 2960).value());
    VERIFY_ARE_EQUAL(uint32_t{ 9000 }, LaneValueAt(lane, 100000).value());

    // A program lane never ramps.
    lane.Target = LaneTarget::Program;
    lane.Points = { LanePoint{ 0, 0, false }, LanePoint{ 1000, Top, false } };
    VERIFY_ARE_EQUAL(uint32_t{ 0 }, LaneValueAt(lane, 999).value());
    VERIFY_ARE_EQUAL(Top, LaneValueAt(lane, 1000).value());
}

void AutomationTests::ARampSendsOnlyWhatTheDestinationCanTell()
{
    auto sequence = LaneSequence();
    sequence.Tracks[0].Lanes = { MakeRamp(LaneTarget::Controller, 74, { LanePoint{ 0, 0, false }, LanePoint{ Bar, Top, false } }) };

    RenderOptions options{};
    options.LaneStartValues = true;
    options.Midi2 = false;

    // MIDI 1.0: one message for each of a 7-bit controller's 128 values, never two alike in a row.
    auto const midi1 = Render(sequence, 0, Bar * 2, options);
    VERIFY_ARE_EQUAL(size_t{ 128 }, midi1.size());
    VERIFY_ARE_EQUAL(uint32_t{ 0 }, Level7(midi1.front()));
    VERIFY_ARE_EQUAL(uint32_t{ 127 }, Level7(midi1.back()));

    for (size_t i = 1; i < midi1.size(); ++i)
    {
        VERIFY_ARE_NOT_EQUAL(Level7(midi1[i - 1]), Level7(midi1[i]));
        VERIFY_IS_TRUE(midi1[i].Tick - midi1[i - 1].Tick >= DefaultLaneStepTicks);
        VERIFY_ARE_EQUAL(0x40B04A00u, midi1[i].Words[0]);
    }

    // MIDI 2.0: every step of the ramp, and the exact value at its end.
    options.Midi2 = true;
    auto const midi2 = Render(sequence, 0, Bar * 2, options);
    VERIFY_ARE_EQUAL(size_t{ 1 + (Bar / DefaultLaneStepTicks - 1) + 1 }, midi2.size());
    VERIFY_ARE_EQUAL(Bar, midi2.back().Tick);
    VERIFY_ARE_EQUAL(Top, midi2.back().Words[1]);
}

void AutomationTests::APieceAtATimeMatchesTheWhole()
{
    auto sequence = LaneSequence();

    // A ramp, a hold, a jump, and a clip with a pitch bend lane of its own, looping twice.
    sequence.Tracks[0].Lanes = { MakeRamp(LaneTarget::Controller, 74, {
        LanePoint{ 100, 0, false },
        LanePoint{ 2000, 0xC0000000, false },
        LanePoint{ 2500, 0x40000000, true },
        LanePoint{ 3000, 0x10000000, false },
        LanePoint{ 3000, 0xF0000000, false },
        LanePoint{ 7000, 0, false } }) };

    sequence.Tracks[0].Timeline = { Placement{ L"c-1", Bar * 2, Bar * 2 } };
    sequence.Clips[0].Lanes = { MakeRamp(LaneTarget::PitchBend, 0, {
        LanePoint{ 0, 0x80000000, false },
        LanePoint{ Bar / 2, Top, false },
        LanePoint{ Bar, 0x80000000, false } }) };

    for (auto const midi2 : { true, false })
    {
        RenderOptions options{};
        options.Midi2 = midi2;

        auto const whole = Render(sequence, 0, Bar * 5, options);

        std::vector<RenderedMessage> pieces{};

        for (int64_t from = 0; from < Bar * 5; from += 37)
        {
            auto const part = Render(sequence, from, std::min(from + 37, Bar * 5), options);
            pieces.insert(pieces.end(), part.begin(), part.end());
        }

        Log::Comment(String().Format(L"MIDI %s: %zu messages", midi2 ? L"2.0" : L"1.0", whole.size()));

        VERIFY_IS_TRUE(whole.size() > 20);
        VERIFY_ARE_EQUAL(whole.size(), pieces.size());

        for (size_t i = 0; i < whole.size(); ++i)
        {
            VERIFY_ARE_EQUAL(whole[i].Tick, pieces[i].Tick);
            VERIFY_ARE_EQUAL(whole[i].Words[0], pieces[i].Words[0]);
            VERIFY_ARE_EQUAL(whole[i].Words[1], pieces[i].Words[1]);
        }
    }
}

void AutomationTests::AClipLaneWinsInsideItsPlacement()
{
    auto sequence = LaneSequence();
    sequence.Tracks[0].Lanes = { MakeRamp(LaneTarget::Controller, 74, { LanePoint{ 0, 0x20000000, false } }) };
    sequence.Tracks[0].Timeline = { Placement{ L"c-1", Bar, 0 } };
    sequence.Clips[0].Loop = false;
    sequence.Clips[0].Lanes = {
        MakeRamp(LaneTarget::Controller, 74, { LanePoint{ 0, 0xE0000000, false } }),
        MakeRamp(LaneTarget::PitchBend, 0, { LanePoint{ 0, 0x90000000, false } }),
    };

    RenderOptions options{};
    options.LaneStartValues = true;

    auto const sent = Render(sequence, 0, Bar * 3, options);

    std::vector<RenderedMessage> controller{};
    std::copy_if(sent.begin(), sent.end(), std::back_inserter(controller), [](RenderedMessage const& m) { return m.Words[0] == 0x40B04A00u; });

    // The track's value at the start, the clip's when the clip starts, the track's again when it ends.
    VERIFY_ARE_EQUAL(size_t{ 3 }, controller.size());
    VERIFY_ARE_EQUAL(int64_t{ 0 }, controller[0].Tick);
    VERIFY_ARE_EQUAL(0x20000000u, controller[0].Words[1]);
    VERIFY_ARE_EQUAL(Bar, controller[1].Tick);
    VERIFY_ARE_EQUAL(0xE0000000u, controller[1].Words[1]);
    VERIFY_ARE_EQUAL(Bar * 2, controller[2].Tick);
    VERIFY_ARE_EQUAL(0x20000000u, controller[2].Words[1]);

    // The clip's pitch bend plays with it, and the track has nothing to hand it back to.
    auto const bends = std::count_if(sent.begin(), sent.end(), [](RenderedMessage const& m) { return m.Words[0] == 0x40E00000u; });
    VERIFY_ARE_EQUAL(ptrdiff_t{ 1 }, bends);
}

void AutomationTests::AClipLaneStartsOverEachPass()
{
    auto sequence = LaneSequence();
    sequence.Clips[0].Lanes = { MakeRamp(LaneTarget::Controller, 1, { LanePoint{ 0, 0, false }, LanePoint{ Bar, Top, false } }) };
    sequence.Tracks[0].Timeline = { Placement{ L"c-1", 0, Bar * 3 } };

    RenderOptions options{};
    options.Midi2 = false;

    auto const sent = Render(sequence, 0, Bar * 3, options);

    // Each pass starts from the bottom again, and climbs to the top.
    for (int64_t pass = 0; pass < 3; ++pass)
    {
        auto const start = std::find_if(sent.begin(), sent.end(), [pass](RenderedMessage const& m) { return m.Tick == pass * Bar; });
        VERIFY_IS_TRUE(start != sent.end());
        VERIFY_ARE_EQUAL(uint32_t{ 0 }, Level7(*start));
    }

    auto const tops = std::count_if(sent.begin(), sent.end(), [](RenderedMessage const& m) { return Level7(m) == 127; });
    VERIFY_ARE_EQUAL(ptrdiff_t{ 3 }, tops);
}

void AutomationTests::StartValuesSendWhereTheLaneIs()
{
    auto sequence = LaneSequence();
    sequence.Tracks[0].Lanes = { MakeRamp(LaneTarget::Controller, 7, { LanePoint{ 0, 0, false }, LanePoint{ 1000, 1000000, false } }) };

    RenderOptions options{};
    options.LaneStartValues = true;

    auto const sent = Render(sequence, 500, 501, options);
    VERIFY_ARE_EQUAL(size_t{ 1 }, sent.size());
    VERIFY_ARE_EQUAL(int64_t{ 500 }, sent[0].Tick);
    VERIFY_ARE_EQUAL(uint32_t{ 500000 }, sent[0].Words[1]);

    // Without them, a window between two steps of the ramp sends nothing.
    options.LaneStartValues = false;
    VERIFY_ARE_EQUAL(size_t{ 0 }, Render(sequence, 501, 519, options).size());

    // And nothing at all when lanes are left out, which is what chase does.
    options.LaneStartValues = true;
    options.Lanes = false;
    VERIFY_ARE_EQUAL(size_t{ 0 }, Render(sequence, 0, Bar, options).size());
}

void AutomationTests::TheStepFollowsTheIntervalAndTheFastestTempo()
{
    // 10 ms at 120 BPM is 19.2 ticks, so a step is 20: never closer than the interval.
    VERIFY_ARE_EQUAL(int64_t{ 20 }, LaneStepTicks(120.0, 10));
    VERIFY_ARE_EQUAL(int64_t{ 20 }, LaneStepTicks(120.0, 0));
    VERIFY_ARE_EQUAL(int64_t{ 39 }, LaneStepTicks(240.0, 10));
    VERIFY_ARE_EQUAL(int64_t{ 96 }, LaneStepTicks(120.0, 50));
    VERIFY_ARE_EQUAL(int64_t{ 1 }, LaneStepTicks(MinimumBeatsPerMinute, 1));

    std::vector<TempoPoint> const tempo{ TempoPoint{ 0, 90.0, true }, TempoPoint{ Bar, 174.0, false }, TempoPoint{ Bar * 2, 120.0, false } };
    VERIFY_ARE_EQUAL(174.0, FastestTempo(tempo));
    VERIFY_ARE_EQUAL(120.0, FastestTempo({}));
}

void AutomationTests::LaneMessagesSpeakEachTarget()
{
    uint32_t words[2]{};

    BuildLaneMessage(MakeLane(LaneTarget::Controller, 74, 3), 0x12345678u, words);
    VERIFY_ARE_EQUAL(0x40B34A00u, words[0]);
    VERIFY_ARE_EQUAL(0x12345678u, words[1]);

    BuildLaneMessage(MakeLane(LaneTarget::PitchBend, 0, 1), 0x80000000u, words);
    VERIFY_ARE_EQUAL(0x40E10000u, words[0]);

    BuildLaneMessage(MakeLane(LaneTarget::ChannelPressure, 0, 0), 0x40000000u, words);
    VERIFY_ARE_EQUAL(0x40D00000u, words[0]);

    BuildLaneMessage(MakeLane(LaneTarget::Program, 0, 9), ScaleUp(12, 7, 32), words);
    VERIFY_ARE_EQUAL(0x40C90000u, words[0]);
    VERIFY_ARE_EQUAL(12u << 24, words[1]);

    BuildLaneMessage(MakeLane(LaneTarget::Registered, 2, 0), 0x80000000u, words);
    VERIFY_ARE_EQUAL(0x40200002u, words[0]);

    BuildLaneMessage(MakeLane(LaneTarget::Assignable, 7 * 128 + 54, 1), 0x80000000u, words);
    VERIFY_ARE_EQUAL(0x40310736u, words[0]);

    // The Moog One in the comps speaks MIDI 1.0, so NRPN 7:54 goes as four controllers, 14 bits.
    auto const midi1 = TranslateToMidi1(words, 2);
    VERIFY_ARE_EQUAL(uint8_t{ 4 }, midi1.Count);
    VERIFY_ARE_EQUAL(0x20B16307u, midi1.Messages[0][0]);
    VERIFY_ARE_EQUAL(0x20B16236u, midi1.Messages[1][0]);
    VERIFY_ARE_EQUAL(0x20B10640u, midi1.Messages[2][0]);
    VERIFY_ARE_EQUAL(0x20B12600u, midi1.Messages[3][0]);
}

void AutomationTests::LanesRoundTripThroughTheFile()
{
    auto sequence = LaneSequence();

    auto cutoff = MakeRamp(LaneTarget::Assignable, 7 * 128 + 54, {
        LanePoint{ 0, 0x59999999, false },
        LanePoint{ Bar * 2, 0xCCCCCCCC, true },
        LanePoint{ Bar * 4, 0x4CCCCCCC, false } });
    cutoff.Name = L"SVF cutoff";
    cutoff.Channel = 1;

    sequence.Tracks[0].Lanes = { cutoff, MakeRamp(LaneTarget::Program, 0, { LanePoint{ 0, ScaleUp(5, 7, 32), false } }) };
    sequence.Tracks[0].LanesOpen = false;
    sequence.Clips[0].Lanes = { MakeRamp(LaneTarget::PitchBend, 0, { LanePoint{ 0, 0x80000000, false } }) };
    NormalizeSequence(sequence);

    auto const text = WriteSequenceJson(sequence);

    VERIFY_IS_TRUE(Contains(text, L"\"target\": \"assignable\""));
    VERIFY_IS_TRUE(Contains(text, L"\"bank\": 7"));
    VERIFY_IS_TRUE(Contains(text, L"\"index\": 54"));
    VERIFY_IS_TRUE(Contains(text, L"\"name\": \"SVF cutoff\""));
    VERIFY_IS_TRUE(Contains(text, L"[7680, 3435973836, \"hold\"]"));
    VERIFY_IS_TRUE(Contains(text, L"\"target\": \"program\""));
    VERIFY_IS_TRUE(Contains(text, L"\"target\": \"pitchBend\""));
    VERIFY_IS_TRUE(Contains(text, L"\"lanesOpen\": false"));

    Sequence read{};
    VERIFY_IS_TRUE(ReadSequenceJson(text, read).Succeeded());

    VERIFY_ARE_EQUAL(size_t{ 2 }, read.Tracks[0].Lanes.size());
    VERIFY_IS_FALSE(read.Tracks[0].LanesOpen);

    auto const& lane = read.Tracks[0].Lanes[0];
    VERIFY_ARE_EQUAL(cutoff.Id, lane.Id);
    VERIFY_ARE_EQUAL(std::wstring{ L"SVF cutoff" }, lane.Name);
    VERIFY_ARE_EQUAL(static_cast<int>(LaneTarget::Assignable), static_cast<int>(lane.Target));
    VERIFY_ARE_EQUAL(uint16_t{ 7 * 128 + 54 }, lane.Number);
    VERIFY_ARE_EQUAL(uint8_t{ 1 }, lane.Channel);
    VERIFY_IS_TRUE(cutoff.Points == lane.Points);

    VERIFY_ARE_EQUAL(size_t{ 1 }, read.Clips[0].Lanes.size());
    VERIFY_IS_TRUE(sequence.Clips[0].Lanes[0].Points == read.Clips[0].Lanes[0].Points);

    // Written again, it's the same text.
    VERIFY_ARE_EQUAL(text, WriteSequenceJson(read));
}

void AutomationTests::DamagedLanesAreClampedOrSkipped()
{
    std::wstring const text = LR"({
  "fileVersion": 1,
  "tracks": [ { "id": "t-1", "kind": "track", "name": "A", "lanes": [
    { "id": "a-1", "target": "controller", "number": 300, "channel": 40, "points": [ [960, 5000000000], [-50, -3], [480, 7, "hold"], "junk", [1] ] },
    { "id": "a-2", "target": "teleport", "points": [ [0, 1] ] },
    { "target": "assignable", "bank": 200, "index": 54, "futureKey": true, "points": [] }
  ] } ],
  "clips": []
})";

    Sequence read{};
    auto const result = ReadSequenceJson(text, read);
    VERIFY_IS_TRUE(result.Succeeded());

    // The junk point, the short point and the lane for a target this version doesn't know.
    VERIFY_ARE_EQUAL(size_t{ 3 }, result.SkippedItems);

    auto const& lanes = read.Tracks[0].Lanes;
    VERIFY_ARE_EQUAL(size_t{ 2 }, lanes.size());

    VERIFY_ARE_EQUAL(uint16_t{ 127 }, lanes[0].Number);
    VERIFY_ARE_EQUAL(uint8_t{ 15 }, lanes[0].Channel);
    VERIFY_ARE_EQUAL(size_t{ 3 }, lanes[0].Points.size());
    VERIFY_IS_TRUE((LanePoint{ 0, 0, false }) == lanes[0].Points[0]);
    VERIFY_IS_TRUE((LanePoint{ 480, 7, true }) == lanes[0].Points[1]);
    VERIFY_IS_TRUE((LanePoint{ 960, Top, false }) == lanes[0].Points[2]);

    VERIFY_ARE_EQUAL(uint16_t{ 127 * 128 + 54 }, lanes[1].Number);
    VERIFY_IS_FALSE(lanes[1].Id.empty());
    VERIFY_IS_TRUE(Contains(WriteSequenceJson(read), L"\"futureKey\": true"));

    // Points stop at the ceiling.
    SequenceLimits limits{};
    limits.MaximumPointsPerLane = 2;

    Sequence capped{};
    auto const cappedResult = ReadSequenceJson(text, capped, limits);
    VERIFY_IS_TRUE(cappedResult.Succeeded());
    VERIFY_ARE_EQUAL(size_t{ 2 }, capped.Tracks[0].Lanes[0].Points.size());
}

void AutomationTests::LanePointEdits()
{
    auto lane = MakeLane(LaneTarget::Controller, 200, 3);
    VERIFY_ARE_EQUAL(uint16_t{ 127 }, lane.Number);
    VERIFY_IS_TRUE(lane.Points.empty());

    AddLanePoint(lane, LanePoint{ 960, 1, false });
    AddLanePoint(lane, LanePoint{ 0, 2, false });

    // A second point at a tick goes after the first, which makes a jump there.
    VERIFY_ARE_EQUAL(size_t{ 2 }, AddLanePoint(lane, LanePoint{ 960, 3, false }));
    VERIFY_ARE_EQUAL(uint32_t{ 2 }, lane.Points[0].Value);
    VERIFY_ARE_EQUAL(uint32_t{ 1 }, lane.Points[1].Value);
    VERIFY_ARE_EQUAL(uint32_t{ 3 }, lane.Points[2].Value);

    // A dragged point can't pass its neighbors.
    MoveLanePoint(lane, 1, 5000, 9);
    VERIFY_ARE_EQUAL(int64_t{ 960 }, lane.Points[1].Tick);
    VERIFY_ARE_EQUAL(uint32_t{ 9 }, lane.Points[1].Value);

    MoveLanePoint(lane, 1, 100, 9);
    VERIFY_ARE_EQUAL(int64_t{ 100 }, lane.Points[1].Tick);

    // Drawing over a stretch replaces what was there.
    ReplaceLanePoints(lane, 50, 2000, { LanePoint{ 1800, 8, false }, LanePoint{ 600, 7, false } });
    VERIFY_ARE_EQUAL(size_t{ 3 }, lane.Points.size());
    VERIFY_ARE_EQUAL(int64_t{ 0 }, lane.Points[0].Tick);
    VERIFY_ARE_EQUAL(int64_t{ 600 }, lane.Points[1].Tick);
    VERIFY_ARE_EQUAL(int64_t{ 1800 }, lane.Points[2].Tick);
}

void AutomationTests::LaneEditsUndo()
{
    auto sequence = LaneSequence();
    auto lane = MakeLane(LaneTarget::Controller, 74, 0);

    auto const before = sequence.Tracks[0].Lanes;
    sequence.Tracks[0].Lanes.push_back(lane);

    UndoStack undo{};
    ChangeList changes{};
    changes.push_back(MakeTrackLanesChange(L"t-1", before, sequence.Tracks[0].Lanes));
    undo.Commit(L"Add lane", std::move(changes));

    VERIFY_IS_TRUE(undo.Undo(sequence));
    VERIFY_IS_TRUE(sequence.Tracks[0].Lanes.empty());

    VERIFY_IS_TRUE(undo.Redo(sequence));
    VERIFY_ARE_EQUAL(size_t{ 1 }, sequence.Tracks[0].Lanes.size());
    VERIFY_ARE_EQUAL(lane.Id, sequence.Tracks[0].Lanes[0].Id);

    // The same for a clip's lanes.
    ChangeList clipChanges{};
    auto clipBefore = sequence.Clips[0].Lanes;
    sequence.Clips[0].Lanes.push_back(lane);
    clipChanges.push_back(MakeClipLanesChange(L"c-1", clipBefore, sequence.Clips[0].Lanes));
    undo.Commit(L"Add clip lane", std::move(clipChanges));

    VERIFY_IS_TRUE(undo.Undo(sequence));
    VERIFY_IS_TRUE(sequence.Clips[0].Lanes.empty());
}

void AutomationTests::TempoEdits()
{
    std::vector<TempoPoint> tempo{ TempoPoint{ 0, 120.0, true }, TempoPoint{ Bar * 4, 140.0, false } };

    // Added inside a ramp: the ramp now ends at the new change.
    SetTempoAt(tempo, Bar * 2, 130.0);
    VERIFY_ARE_EQUAL(size_t{ 3 }, tempo.size());
    VERIFY_IS_TRUE(tempo[0].RampToNext);
    VERIFY_ARE_EQUAL(130.0, tempo[1].BeatsPerMinute);

    // Changed in place.
    SetTempoAt(tempo, Bar * 2, 128.0);
    VERIFY_ARE_EQUAL(size_t{ 3 }, tempo.size());
    VERIFY_ARE_EQUAL(128.0, tempo[1].BeatsPerMinute);

    // A dragged change can't pass the next one, and the tempo stays in range.
    MoveTempoPoint(tempo, 1, Bar * 10, 5000.0);
    VERIFY_ARE_EQUAL(Bar * 4 - 1, tempo[1].Tick);
    VERIFY_ARE_EQUAL(MaximumBeatsPerMinute, tempo[1].BeatsPerMinute);

    // The first change stays at the start.
    MoveTempoPoint(tempo, 0, Bar, 100.0);
    VERIFY_ARE_EQUAL(int64_t{ 0 }, tempo[0].Tick);
    VERIFY_ARE_EQUAL(100.0, tempo[0].BeatsPerMinute);

    VERIFY_IS_FALSE(RemoveTempoPoint(tempo, 0));
    VERIFY_IS_TRUE(RemoveTempoPoint(tempo, 2));
    VERIFY_ARE_EQUAL(size_t{ 2 }, tempo.size());
    VERIFY_IS_FALSE(tempo.back().RampToNext);
}

void AutomationTests::MeterEditsKeepLaterBars()
{
    // 4/4, then 7/8 from bar 9.
    std::vector<MeterChange> meter{ MeterChange{ 0, 4, 4 }, MeterChange{ Bar * 8, 7, 8 } };

    // 3/4 from bar 5: the 7/8 stays at bar 9, which is now four bars of 3/4 later.
    SetMeterAt(meter, 5, 3, 4);
    VERIFY_ARE_EQUAL(size_t{ 3 }, meter.size());
    VERIFY_ARE_EQUAL(Bar * 4, meter[1].Tick);
    VERIFY_ARE_EQUAL(Bar * 4 + 4 * 2880, meter[2].Tick);
    VERIFY_ARE_EQUAL(int64_t{ 9 }, BarPositionAtTick(meter, meter[2].Tick).Bar);

    // The first meter changes in place, and the others keep their bars.
    SetMeterAt(meter, 1, 6, 8);
    VERIFY_ARE_EQUAL(size_t{ 3 }, meter.size());
    VERIFY_ARE_EQUAL(uint8_t{ 6 }, meter[0].Numerator);
    VERIFY_ARE_EQUAL(int64_t{ 5 }, BarPositionAtTick(meter, meter[1].Tick).Bar);
    VERIFY_ARE_EQUAL(int64_t{ 9 }, BarPositionAtTick(meter, meter[2].Tick).Bar);

    VERIFY_IS_FALSE(RemoveMeterChange(meter, 0));
    VERIFY_IS_TRUE(RemoveMeterChange(meter, 1));
    VERIFY_ARE_EQUAL(size_t{ 2 }, meter.size());
    VERIFY_ARE_EQUAL(int64_t{ 9 }, BarPositionAtTick(meter, meter[1].Tick).Bar);
}

void AutomationTests::LaneRowsFollowTheirTrack()
{
    auto sequence = LaneSequence();
    sequence.Tracks[0].Lanes = { MakeLane(LaneTarget::Controller, 74, 0), MakeLane(LaneTarget::PitchBend, 0, 0) };

    Track second{};
    second.Id = L"t-2";
    second.Name = L"Drums";
    second.Lanes = { MakeLane(LaneTarget::Controller, 7, 0) };
    sequence.Tracks.push_back(second);

    // Open: each lane gets a row right under its track, in order.
    auto layout = BuildArrangeLayout(sequence);
    VERIFY_ARE_EQUAL(size_t{ 6 }, layout.Scrolling.size());
    VERIFY_ARE_EQUAL(static_cast<int>(ArrangeRowKind::Track), static_cast<int>(layout.Scrolling[0].Kind));
    VERIFY_ARE_EQUAL(static_cast<int>(ArrangeRowKind::Lane), static_cast<int>(layout.Scrolling[1].Kind));
    VERIFY_ARE_EQUAL(sequence.Tracks[0].Lanes[0].Id, layout.Scrolling[1].LaneId);
    VERIFY_ARE_EQUAL(sequence.Tracks[0].Lanes[1].Id, layout.Scrolling[2].LaneId);
    VERIFY_ARE_EQUAL(LaneRowHeight, layout.Scrolling[1].Height);
    VERIFY_ARE_EQUAL(std::wstring{ L"t-2" }, layout.Scrolling[3].TrackId);
    VERIFY_ARE_EQUAL(static_cast<int>(ArrangeRowKind::Lane), static_cast<int>(layout.Scrolling[4].Kind));

    // The track's own row is found, not one of its lanes.
    VERIFY_ARE_EQUAL(static_cast<int>(ArrangeRowKind::Track), static_cast<int>(layout.FindRow(L"t-1")->Kind));

    // Closed: no lane rows. Pinned: the lanes go up with the track.
    sequence.Tracks[0].LanesOpen = false;
    sequence.Tracks[1].Pinned = true;
    layout = BuildArrangeLayout(sequence);

    VERIFY_ARE_EQUAL(size_t{ 2 }, layout.Scrolling.size());
    VERIFY_ARE_EQUAL(size_t{ 3 }, layout.Pinned.size());
    VERIFY_ARE_EQUAL(static_cast<int>(ArrangeRowKind::Lane), static_cast<int>(layout.Pinned[2].Kind));
    VERIFY_IS_TRUE(layout.Pinned[2].Pinned);
    VERIFY_ARE_EQUAL(TempoRowHeight + TrackRowHeight + LaneRowHeight, layout.PinnedHeight);
}
