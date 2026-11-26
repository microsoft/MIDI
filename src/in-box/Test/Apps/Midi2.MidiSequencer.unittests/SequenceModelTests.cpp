// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#include "SequenceModelTests.h"
#include "SequenceTestData.h"

#include "SequenceModel.h"

#include <cmath>

using namespace WEX::Common;
using namespace WEX::Logging;
using namespace WEX::TestExecution;
using namespace midisequencer;

namespace
{
    constexpr int64_t Bar = TicksPerQuarterNote * 4;

    bool Near(double a, double b, double tolerance = 1e-9)
    {
        return std::fabs(a - b) <= tolerance;
    }
}

void SequenceModelTests::ASteadyTempoIsLinear()
{
    TempoMap const map{ std::vector<TempoPoint>{ TempoPoint{ 0, 120.0, false } } };

    VERIFY_IS_TRUE(Near(0.5, map.SecondsAtTick(TicksPerQuarterNote)));
    VERIFY_IS_TRUE(Near(2.0, map.SecondsAtTick(Bar)));
    VERIFY_ARE_EQUAL(TicksPerQuarterNote, map.TickAtSeconds(0.5));
    VERIFY_ARE_EQUAL(Bar * 100, map.TickAtSeconds(200.0));

    // A change of tempo at bar 2: the second bar at 60 BPM takes four seconds.
    TempoMap const change{ std::vector<TempoPoint>{ TempoPoint{ 0, 120.0, false }, TempoPoint{ Bar, 60.0, false } } };
    VERIFY_IS_TRUE(Near(6.0, change.SecondsAtTick(Bar * 2)));
    VERIFY_ARE_EQUAL(Bar * 2, change.TickAtSeconds(6.0));
    VERIFY_IS_TRUE(Near(60.0, change.BeatsPerMinuteAtTick(Bar + 1)));
}

void SequenceModelTests::ARampFollowsTheCurve()
{
    // 100 to 200 BPM over one bar.
    TempoMap const map{ std::vector<TempoPoint>{ TempoPoint{ 0, 100.0, true }, TempoPoint{ Bar, 200.0, false } } };

    auto const slope = 100.0 / static_cast<double>(Bar);
    auto const expected = (60.0 / TicksPerQuarterNote) * std::log(2.0) / slope;

    VERIFY_IS_TRUE(Near(expected, map.SecondsAtTick(Bar), 1e-9));

    // Faster than a bar at 100, slower than a bar at 200.
    VERIFY_IS_TRUE(map.SecondsAtTick(Bar) < 2.4);
    VERIFY_IS_TRUE(map.SecondsAtTick(Bar) > 1.2);

    VERIFY_IS_TRUE(Near(150.0, map.BeatsPerMinuteAtTick(Bar / 2), 1e-9));

    // Time back to ticks lands on the same tick, all the way through the ramp and after it.
    for (int64_t tick = 0; tick <= Bar * 2; tick += 37)
    {
        VERIFY_ARE_EQUAL(tick, map.TickAtSeconds(map.SecondsAtTick(tick)));
    }
}

void SequenceModelTests::BarsFollowMeterChanges()
{
    // Two bars of 4/4, then 3/4.
    std::vector<MeterChange> const meter{ MeterChange{ 0, 4, 4 }, MeterChange{ Bar * 2, 3, 4 } };
    auto const threeFour = TicksPerQuarterNote * 3;

    auto position = BarPositionAtTick(meter, Bar * 2);
    VERIFY_ARE_EQUAL(int64_t{ 3 }, position.Bar);
    VERIFY_ARE_EQUAL(int64_t{ 1 }, position.Beat);

    position = BarPositionAtTick(meter, Bar * 2 + threeFour + TicksPerQuarterNote * 2 + 10);
    VERIFY_ARE_EQUAL(int64_t{ 4 }, position.Bar);
    VERIFY_ARE_EQUAL(int64_t{ 3 }, position.Beat);
    VERIFY_ARE_EQUAL(int64_t{ 10 }, position.TicksIntoBeat);

    VERIFY_ARE_EQUAL(int64_t{ 0 }, TickAtBar(meter, 1));
    VERIFY_ARE_EQUAL(Bar * 2, TickAtBar(meter, 3));
    VERIFY_ARE_EQUAL(Bar * 2 + threeFour * 2, TickAtBar(meter, 5));

    // 7/8: a beat is an eighth note.
    MeterChange const sevenEight{ 0, 7, 8 };
    VERIFY_ARE_EQUAL(TicksPerQuarterNote / 2, TicksPerBeat(sevenEight));
    VERIFY_ARE_EQUAL(TicksPerQuarterNote * 7 / 2, TicksPerBar(sevenEight));
}

void SequenceModelTests::ScalingUpThenDownGivesBackTheOriginal()
{
    for (uint32_t value = 0; value < 128; ++value)
    {
        VERIFY_ARE_EQUAL(value, ScaleDown(ScaleUp(value, 7, 16), 16, 7));
        VERIFY_ARE_EQUAL(value, ScaleDown(ScaleUp(value, 7, 32), 32, 7));
    }

    for (uint32_t value = 0; value < 16384; value += 7)
    {
        VERIFY_ARE_EQUAL(value, ScaleDown(ScaleUp(value, 14, 32), 32, 14));
    }

    // Bottom, center and top land on bottom, center and top.
    VERIFY_ARE_EQUAL(0u, ScaleUp(0, 7, 16));
    VERIFY_ARE_EQUAL(0x8000u, ScaleUp(64, 7, 16));
    VERIFY_ARE_EQUAL(0xFFFFu, ScaleUp(127, 7, 16));
    VERIFY_ARE_EQUAL(0xFFFFFFFFu, ScaleUp(127, 7, 32));
    VERIFY_ARE_EQUAL(0x80000000u, ScaleUp(8192, 14, 32));
    VERIFY_ARE_EQUAL(0xFFFFFFFFu, ScaleUp(16383, 14, 32));

    VERIFY_ARE_EQUAL(127u, ScaleDown(0xFFFF, 16, 7));
    VERIFY_ARE_EQUAL(80u, ScaleDown(41287, 16, 7));
}

void SequenceModelTests::ChanceIsTheSameEveryTime()
{
    auto note = testdata::MakeNote(0, 120, 60);
    note.Chance = 50;

    uint32_t played{ 0 };

    for (uint64_t pass = 0; pass < 2000; ++pass)
    {
        auto const first = NotePlaysOnPass(note, 0x4F2A, 3, pass);
        VERIFY_ARE_EQUAL(first, NotePlaysOnPass(note, 0x4F2A, 3, pass));

        if (first)
        {
            ++played;
        }
    }

    Log::Comment(String().Format(L"A 50%% note played on %u of 2000 passes.", played));
    VERIFY_IS_TRUE(played > 800 && played < 1200);

    // Another seed gives another pattern.
    uint32_t differences{ 0 };

    for (uint64_t pass = 0; pass < 200; ++pass)
    {
        differences += NotePlaysOnPass(note, 0x4F2A, 3, pass) != NotePlaysOnPass(note, 0x91C3, 3, pass) ? 1 : 0;
    }

    VERIFY_IS_TRUE(differences > 20);

    note.Chance = 100;
    VERIFY_IS_TRUE(NotePlaysOnPass(note, 1, 1, 1));

    note.Chance = 0;
    VERIFY_IS_FALSE(NotePlaysOnPass(note, 1, 1, 1));
}

void SequenceModelTests::NormalizingFixesWhatItCan()
{
    Sequence sequence{};
    sequence.Tempo = { TempoPoint{ Bar, 90.0, true }, TempoPoint{ Bar, 95.0, false }, TempoPoint{ Bar * 2, 5000.0, false } };

    // A meter change partway through a bar moves back to where that bar starts.
    sequence.Meter = { MeterChange{ Bar + 100, 3, 4 }, MeterChange{ 0, 4, 3 } };
    sequence.Scenes = { Scene{ L"", L"One", nullptr }, Scene{ L"", L"Two", nullptr } };

    Clip clip{};
    clip.Id = L"c-1";
    clip.Length = 0;
    clip.Notes = { testdata::MakeNote(10, 0, 200), testdata::MakeNote(-5, 10, 60), testdata::MakeNote(0, 10, 60) };
    clip.Events = { testdata::MakeEvent(0, 0x40B14A00, 1) };
    clip.Events[0].WordCount = 1;

    Clip duplicate{};
    duplicate.Id = L"c-1";

    Track track{};
    track.Id = L"t-1";
    track.Timeline = { Placement{ L"c-missing", 0, 0 }, Placement{ L"c-1", Bar, 0 } };
    track.Slots = { L"c-missing" };
    track.Destination.Channel = 40;

    sequence.Tracks = { track };
    sequence.Clips = { clip, duplicate };

    NormalizeSequence(sequence);

    VERIFY_ARE_EQUAL(size_t{ 3 }, sequence.Tempo.size());
    VERIFY_ARE_EQUAL(int64_t{ 0 }, sequence.Tempo[0].Tick);
    VERIFY_ARE_EQUAL(95.0, sequence.Tempo[1].BeatsPerMinute);
    VERIFY_ARE_EQUAL(MaximumBeatsPerMinute, sequence.Tempo[2].BeatsPerMinute);
    VERIFY_IS_FALSE(sequence.Tempo.back().RampToNext);

    VERIFY_ARE_EQUAL(size_t{ 2 }, sequence.Meter.size());
    VERIFY_ARE_EQUAL(uint8_t{ 4 }, sequence.Meter[0].Denominator);
    VERIFY_ARE_EQUAL(Bar, sequence.Meter[1].Tick);

    VERIFY_IS_FALSE(sequence.Scenes[0].Id.empty());
    VERIFY_ARE_NOT_EQUAL(sequence.Scenes[0].Id, sequence.Scenes[1].Id);

    auto const& fixed = sequence.Clips[0];
    VERIFY_ARE_EQUAL(int64_t{ 1 }, fixed.Length);
    VERIFY_ARE_EQUAL(size_t{ 2 }, fixed.Notes.size());
    VERIFY_ARE_EQUAL(int64_t{ 0 }, fixed.Notes[0].Tick);
    VERIFY_ARE_EQUAL(uint8_t{ 200 & 0x7F }, fixed.Notes[1].Number);
    VERIFY_ARE_EQUAL(int64_t{ 1 }, fixed.Notes[1].Length);
    VERIFY_IS_TRUE(fixed.Events.empty());

    VERIFY_ARE_NOT_EQUAL(sequence.Clips[0].Id, sequence.Clips[1].Id);

    auto const& fixedTrack = sequence.Tracks[0];
    VERIFY_ARE_EQUAL(size_t{ 1 }, fixedTrack.Timeline.size());
    VERIFY_ARE_EQUAL(std::wstring{ L"c-1" }, fixedTrack.Timeline[0].ClipId);
    VERIFY_ARE_EQUAL(size_t{ 2 }, fixedTrack.Slots.size());
    VERIFY_IS_TRUE(fixedTrack.Slots[0].empty());
    VERIFY_ARE_EQUAL(int8_t{ 15 }, fixedTrack.Destination.Channel);
}

void SequenceModelTests::ATrackIsFoundInsideFolders()
{
    auto sequence = testdata::SampleSequence();

    auto const bass = FindTrack(sequence, L"t-bass");
    VERIFY_IS_NOT_NULL(bass);
    VERIFY_ARE_EQUAL(std::wstring{ L"Bass" }, bass->Name);

    VERIFY_IS_NULL(FindTrack(sequence, L"t-nothing"));
    VERIFY_ARE_EQUAL(size_t{ 4 }, CountTracks(sequence));

    // Two placements and one slot.
    VERIFY_ARE_EQUAL(size_t{ 3 }, CountClipUses(sequence, L"c-bass-a"));

    std::vector<std::pair<std::wstring, size_t>> order{};
    ForEachTrack(sequence, [&order](Track const& track, size_t depth) { order.emplace_back(track.Id, depth); return true; });

    VERIFY_ARE_EQUAL(size_t{ 4 }, order.size());
    VERIFY_ARE_EQUAL(std::wstring{ L"t-drums" }, order[0].first);
    VERIFY_ARE_EQUAL(std::wstring{ L"f-synths" }, order[1].first);
    VERIFY_ARE_EQUAL(std::wstring{ L"t-bass" }, order[2].first);
    VERIFY_ARE_EQUAL(size_t{ 1 }, order[2].second);
}

void SequenceModelTests::TextSurvivesUtf8()
{
    std::wstring const text{ L"Caf\u00E9 \u266A \U0001D11E" };
    VERIFY_ARE_EQUAL(text, FromUtf8(ToUtf8(text)));

    // A lone continuation byte and a cut-off sequence become replacement characters.
    std::string const broken{ "A\x80" "B\xE2\x99" };
    VERIFY_ARE_EQUAL(std::wstring{ L"A\uFFFDB\uFFFD\uFFFD" }, FromUtf8(broken));
}
