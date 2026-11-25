// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#include "SequenceModelTests.h"
#include "SequenceTestData.h"

#include "SequenceSerializer.h"

#include <format>

using namespace WEX::Common;
using namespace WEX::Logging;
using namespace WEX::TestExecution;
using namespace midisequencer;

namespace
{
    bool Contains(std::wstring const& text, std::wstring_view part)
    {
        return text.find(part) != std::wstring::npos;
    }

    Sequence ReadOrFail(std::wstring const& text, SequenceReadResult* resultOut = nullptr, SequenceLimits const& limits = {})
    {
        Sequence sequence{};
        auto const result = ReadSequenceJson(text, sequence, limits);

        VERIFY_IS_TRUE(result.Succeeded());

        if (resultOut != nullptr)
        {
            *resultOut = result;
        }

        return sequence;
    }

    // The smallest file the reader accepts, with whatever goes in the middle.
    std::wstring MinimalFile(std::wstring_view inner)
    {
        return std::format(L"{{ \"fileVersion\": 1, \"tracks\": [], {} }}", inner);
    }
}

void SequenceSerializerTests::ASequenceRoundTrips()
{
    auto const original = testdata::SampleSequence();
    auto const text = WriteSequenceJson(original);

    Log::Comment(String().Format(L"%zu characters", text.size()));

    auto const read = ReadOrFail(text);

    // Written again, it is the same text, character for character.
    VERIFY_ARE_EQUAL(text, WriteSequenceJson(read));

    VERIFY_ARE_EQUAL(original.Name, read.Name);
    VERIFY_IS_TRUE(read.Provenance.has_value());
    VERIFY_ARE_EQUAL(std::wstring{ L"composite" }, read.Provenance->DigitalSourceType);
    VERIFY_IS_TRUE(original.Tempo == read.Tempo);
    VERIFY_IS_TRUE(original.Meter == read.Meter);
    VERIFY_IS_TRUE(original.Tags == read.Tags);
    VERIFY_ARE_EQUAL(original.Scenes.size(), read.Scenes.size());
    VERIFY_ARE_EQUAL(original.Clips.size(), read.Clips.size());

    for (size_t i = 0; i < original.Clips.size(); ++i)
    {
        VERIFY_IS_TRUE(original.Clips[i].Notes == read.Clips[i].Notes);
        VERIFY_IS_TRUE(original.Clips[i].Events == read.Clips[i].Events);
        VERIFY_ARE_EQUAL(original.Clips[i].Seed, read.Clips[i].Seed);
        VERIFY_ARE_EQUAL(static_cast<int>(original.Clips[i].Origin), static_cast<int>(read.Clips[i].Origin));
    }

    auto const bass = FindTrack(read, L"t-bass");
    VERIFY_IS_NOT_NULL(bass);
    VERIFY_IS_TRUE(FindTrack(original, L"t-bass")->Destination == bass->Destination);
    VERIFY_IS_TRUE(FindTrack(original, L"t-bass")->Source == bass->Source);
    VERIFY_IS_TRUE(FindTrack(original, L"t-bass")->Startup == bass->Startup);
    VERIFY_IS_TRUE(FindTrack(original, L"t-bass")->Timeline == bass->Timeline);
    VERIFY_IS_TRUE(FindTrack(original, L"t-bass")->Tags == bass->Tags);

    auto const pad = FindTrack(read, L"t-pad");
    VERIFY_IS_NOT_NULL(pad);
    VERIFY_IS_TRUE(pad->Muted);
    VERIFY_ARE_EQUAL(int8_t{ -1 }, pad->Destination.Channel);

    // What a person reading the file sees.
    VERIFY_IS_TRUE(Contains(text, L"\"channel\": \"asRecorded\""));
    VERIFY_IS_TRUE(Contains(text, L"[960, 2000, 1, 52, 24576, 16384]"));
    VERIFY_IS_TRUE(Contains(text, L"[480, 403, 1, 40, 36044, 0, 0, 0, 60]"));
    VERIFY_IS_TRUE(Contains(text, L"[0, \"40B14A00 80000000\"]"));
    VERIFY_IS_TRUE(Contains(text, L"{ \"tick\": 61440, \"text\": \"Drop\", \"color\": \"#F2C14E\" }"));
}

void SequenceSerializerTests::KeysFromANewerBuildSurvive()
{
    std::wstring const text = LR"({
  "fileVersion": 1,
  "futureSetting": { "b": 2, "a": [1, 2] },
  "scenes": [ { "id": "s-1", "name": "One", "followAction": "next" } ],
  "tracks": [ { "id": "t-1", "kind": "track", "name": "Keys", "automation": [ { "lane": "cc74" } ], "slots": [ "c-1" ] } ],
  "clips": [ { "id": "c-1", "kind": "notes", "name": "Hook", "length": 3840, "loop": true, "grooveTemplate": "mpc60" } ]
})";

    auto const read = ReadOrFail(text);
    auto const written = WriteSequenceJson(read);

    // Windows.Data.Json may keep an object's keys in its own order, so look for each part.
    VERIFY_IS_TRUE(Contains(written, L"\"futureSetting\": {"));
    VERIFY_IS_TRUE(Contains(written, L"\"b\":2"));
    VERIFY_IS_TRUE(Contains(written, L"\"a\":[1,2]"));
    VERIFY_IS_TRUE(Contains(written, L"\"followAction\": \"next\""));
    VERIFY_IS_TRUE(Contains(written, L"\"automation\": [{\"lane\":\"cc74\"}]"));
    VERIFY_IS_TRUE(Contains(written, L"\"grooveTemplate\": \"mpc60\""));

    // And they stay the same through a second trip.
    VERIFY_ARE_EQUAL(written, WriteSequenceJson(ReadOrFail(written)));
}

void SequenceSerializerTests::ANewerFileIsFlagged()
{
    SequenceReadResult result{};
    ReadOrFail(LR"({ "fileVersion": 7, "tracks": [] })", &result);
    VERIFY_IS_TRUE(result.FromNewerVersion);

    ReadOrFail(LR"({ "fileVersion": 1, "tracks": [] })", &result);
    VERIFY_IS_FALSE(result.FromNewerVersion);
}

void SequenceSerializerTests::SomethingElseIsRefused()
{
    Sequence sequence{};

    VERIFY_ARE_EQUAL(static_cast<int>(SequenceReadStatus::NotJson), static_cast<int>(ReadSequenceJson(L"MThd", sequence).Status));
    VERIFY_ARE_EQUAL(static_cast<int>(SequenceReadStatus::NotJson), static_cast<int>(ReadSequenceJson(L"[1, 2, 3]", sequence).Status));
    VERIFY_ARE_EQUAL(static_cast<int>(SequenceReadStatus::NotASequence), static_cast<int>(ReadSequenceJson(L"{}", sequence).Status));

    // A MIDI Glass layout is JSON, but not a sequence.
    VERIFY_ARE_EQUAL(static_cast<int>(SequenceReadStatus::NotASequence),
        static_cast<int>(ReadSequenceJson(LR"({ "formatVersion": 1, "pages": [] })", sequence).Status));
}

void SequenceSerializerTests::DamagedValuesAreClampedOrSkipped()
{
    SequenceReadResult result{};

    auto const read = ReadOrFail(MinimalFile(LR"(
  "tempo": [ { "tick": -50, "bpm": 1e300 }, "fast" ],
  "meter": [ { "tick": 0, "numerator": 900, "denominator": 3 } ],
  "tags": [ { "tick": 10, "text": "" }, "not a tag", { "tick": 20, "text": "Real\u202E tag" } ],
  "clips": [ {
    "id": "c-1", "length": -10,
    "notes": [ [0, 10, 99, 300, 70000], [5], "x", [1, 2, 3, 4, 5, 6, 7, 8, 900], ["a", 1, 1, 1, 1] ],
    "events": [ [0, "40B14A00"], [0, "zz"], [0, "20B00764"], [1] ]
  } ])"), &result);

    VERIFY_IS_TRUE(result.SkippedItems >= 7);

    VERIFY_ARE_EQUAL(int64_t{ 0 }, read.Tempo.front().Tick);
    VERIFY_ARE_EQUAL(MaximumBeatsPerMinute, read.Tempo.front().BeatsPerMinute);

    VERIFY_ARE_EQUAL(uint8_t{ 64 }, read.Meter.front().Numerator);
    VERIFY_ARE_EQUAL(uint8_t{ 4 }, read.Meter.front().Denominator);

    // Direction overrides are taken out of text, so a name can't disguise itself.
    VERIFY_ARE_EQUAL(size_t{ 1 }, read.Tags.size());
    VERIFY_ARE_EQUAL(std::wstring{ L"Real tag" }, read.Tags[0].Text);

    auto const& clip = read.Clips.front();
    VERIFY_ARE_EQUAL(int64_t{ 1 }, clip.Length);
    VERIFY_ARE_EQUAL(size_t{ 2 }, clip.Notes.size());
    VERIFY_ARE_EQUAL(uint8_t{ 15 }, clip.Notes[0].Channel);
    VERIFY_ARE_EQUAL(uint8_t{ 127 }, clip.Notes[0].Number);
    VERIFY_ARE_EQUAL(uint16_t{ 0xFFFF }, clip.Notes[0].Velocity);
    VERIFY_ARE_EQUAL(uint8_t{ 100 }, clip.Notes[1].Chance);

    // Only the message with the right number of words for its type survives.
    VERIFY_ARE_EQUAL(size_t{ 1 }, clip.Events.size());
    VERIFY_ARE_EQUAL(0x20B00764u, clip.Events[0].Words[0]);
}

void SequenceSerializerTests::CountsStopAtTheLimits()
{
    std::wstring notes{};

    for (int i = 0; i < 50; ++i)
    {
        notes += std::format(L"{}[{}, 10, 0, 60, 1000]", i == 0 ? L"" : L", ", i * 10);
    }

    std::wstring tracks{};

    for (int i = 0; i < 30; ++i)
    {
        tracks += std::format(L"{}{{ \"id\": \"t-{}\", \"name\": \"T\" }}", i == 0 ? L"" : L", ", i);
    }

    auto const text = std::format(L"{{ \"fileVersion\": 1, \"tracks\": [ {} ], \"clips\": [ {{ \"id\": \"c\", \"notes\": [ {} ] }} ] }}", tracks, notes);

    SequenceLimits limits{};
    limits.MaximumNotesPerClip = 20;
    limits.MaximumTracks = 10;

    SequenceReadResult result{};
    auto const read = ReadOrFail(text, &result, limits);

    VERIFY_ARE_EQUAL(size_t{ 20 }, read.Clips.front().Notes.size());
    VERIFY_ARE_EQUAL(size_t{ 10 }, read.Tracks.size());
    VERIFY_ARE_EQUAL(size_t{ 50 }, result.SkippedItems);

    // Folders nested past the limit are left out rather than followed.
    std::wstring nested{ L"{ \"id\": \"t-deep\", \"name\": \"Deep\" }" };

    for (int depth = 0; depth < 40; ++depth)
    {
        nested = std::format(L"{{ \"id\": \"f-{}\", \"kind\": \"folder\", \"name\": \"F\", \"tracks\": [ {} ] }}", depth, nested);
    }

    auto const deep = ReadOrFail(std::format(L"{{ \"fileVersion\": 1, \"tracks\": [ {} ] }}", nested));
    VERIFY_IS_NULL(FindTrack(deep, L"t-deep"));
    VERIFY_IS_TRUE(CountTracks(deep) <= SequenceLimits{}.MaximumFolderDepth);
}

void SequenceSerializerTests::AnotherResolutionIsScaled()
{
    auto const read = ReadOrFail(MinimalFile(LR"(
  "ticksPerQuarterNote": 480,
  "tags": [ { "tick": 480, "text": "Beat 2" } ],
  "clips": [ { "id": "c-1", "length": 1920, "notes": [ [480, 240, 0, 60, 1000] ], "events": [ [240, "20B00764"] ] } ])"));

    VERIFY_ARE_EQUAL(TicksPerQuarterNote, read.Tags[0].Tick);

    auto const& clip = read.Clips.front();
    VERIFY_ARE_EQUAL(TicksPerQuarterNote * 4, clip.Length);
    VERIFY_ARE_EQUAL(TicksPerQuarterNote, clip.Notes[0].Tick);
    VERIFY_ARE_EQUAL(TicksPerQuarterNote / 2, clip.Notes[0].Length);
    VERIFY_ARE_EQUAL(TicksPerQuarterNote / 2, clip.Events[0].Tick);

    // Written back at 960.
    VERIFY_IS_TRUE(Contains(WriteSequenceJson(read), L"\"ticksPerQuarterNote\": 960"));
}

void SequenceSerializerTests::MessagesReadAndWriteAsText()
{
    ClipEvent event{};

    VERIFY_IS_TRUE(UmpFromText(L"40b14a00 80000000", event));
    VERIFY_ARE_EQUAL(uint8_t{ 2 }, event.WordCount);
    VERIFY_ARE_EQUAL(0x40B14A00u, event.Words[0]);
    VERIFY_ARE_EQUAL(0x80000000u, event.Words[1]);
    VERIFY_ARE_EQUAL(std::wstring{ L"40B14A00 80000000" }, UmpToText(event));

    VERIFY_IS_TRUE(UmpFromText(L"  20B00764  ", event));
    VERIFY_ARE_EQUAL(uint8_t{ 1 }, event.WordCount);

    // The word count has to match the message type.
    VERIFY_IS_FALSE(UmpFromText(L"40B14A00", event));
    VERIFY_IS_FALSE(UmpFromText(L"20B00764 00000000", event));
    VERIFY_IS_FALSE(UmpFromText(L"", event));
    VERIFY_IS_FALSE(UmpFromText(L"2OB00764", event));
    VERIFY_IS_FALSE(UmpFromText(L"120B00764", event));
    VERIFY_IS_FALSE(UmpFromText(L"F0000000 0 0 0 0", event));
}
