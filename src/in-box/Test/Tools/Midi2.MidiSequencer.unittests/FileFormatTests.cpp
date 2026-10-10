// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#include "FileFormatTests.h"
#include "SequenceTestData.h"

#include "MidiClipFile.h"
#include "SequenceRender.h"
#include "StandardMidiFileBridge.h"

#include "midi_file_smf_reader.h"

#include <algorithm>
#include <cmath>

using namespace WEX::Common;
using namespace WEX::Logging;
using namespace WEX::TestExecution;
using namespace midisequencer;

namespace
{
    constexpr int64_t Bar = TicksPerQuarterNote * 4;

    uint32_t WordAt(std::vector<uint8_t> const& bytes, size_t offset)
    {
        return (static_cast<uint32_t>(bytes[offset]) << 24) | (static_cast<uint32_t>(bytes[offset + 1]) << 16) |
            (static_cast<uint32_t>(bytes[offset + 2]) << 8) | bytes[offset + 3];
    }

    ClipEvent Event(int64_t tick, std::initializer_list<uint32_t> words)
    {
        ClipEvent event{};
        event.Tick = tick;
        event.WordCount = static_cast<uint8_t>(words.size());
        std::copy(words.begin(), words.end(), event.Words.begin());
        return event;
    }

    size_t CountNoteOns(Sequence const& sequence, Track const& track)
    {
        std::vector<RenderedMessage> messages{};
        RenderTrack(sequence, track, 0, INT64_MAX, messages);
        return static_cast<size_t>(std::count_if(messages.begin(), messages.end(),
            [](RenderedMessage const& message) { return message.Kind == RenderedKind::NoteOn; }));
    }

    midifile::MidiSequence ParseOrFail(std::vector<uint8_t> const& bytes)
    {
        midifile::MidiSequence file{};
        auto const result = midifile::ParseStandardMidiFile(std::span<uint8_t const>{ bytes }, file);
        VERIFY_IS_TRUE(result.Succeeded());
        return file;
    }
}

void ClipFileTests::AFileHasTheShapeTheSpecDescribes()
{
    ClipFile file{};
    file.Configuration.push_back(Event(0, { 0xD0100000, 50000000, 0, 0 }));
    file.Events.push_back(Event(10, { 0x40903C00, 0xC0000000 }));
    file.EndTick = 100;

    auto const bytes = WriteClipFile(file);

    VERIFY_ARE_EQUAL(size_t{ 88 }, bytes.size());
    VERIFY_IS_TRUE(std::equal(bytes.begin(), bytes.begin() + 8, "SMF2CLIP"));

    // DCS(0) then DCTPQ(960).
    VERIFY_ARE_EQUAL(0x00400000u, WordAt(bytes, 8));
    VERIFY_ARE_EQUAL(0x003003C0u, WordAt(bytes, 12));

    // DCS(0), Set Tempo.
    VERIFY_ARE_EQUAL(0x00400000u, WordAt(bytes, 16));
    VERIFY_ARE_EQUAL(0xD0100000u, WordAt(bytes, 20));

    // DCS(0), Start of Clip.
    VERIFY_ARE_EQUAL(0x00400000u, WordAt(bytes, 36));
    VERIFY_ARE_EQUAL(0xF0200000u, WordAt(bytes, 40));

    // DCS(10), the note.
    VERIFY_ARE_EQUAL(0x0040000Au, WordAt(bytes, 56));
    VERIFY_ARE_EQUAL(0x40903C00u, WordAt(bytes, 60));

    // DCS(90), End of Clip, last.
    VERIFY_ARE_EQUAL(0x0040005Au, WordAt(bytes, 68));
    VERIFY_ARE_EQUAL(0xF0210000u, WordAt(bytes, 72));
}

void ClipFileTests::ALongGapUsesANoop()
{
    ClipFile file{};
    file.Events.push_back(Event(0, { 0x20903C64 }));
    file.Events.push_back(Event(2500000, { 0x20803C00 }));
    file.EndTick = 2500000;

    auto const bytes = WriteClipFile(file);

    size_t noops{ 0 };

    for (size_t offset = 8; offset + 8 <= bytes.size(); offset += 4)
    {
        if (WordAt(bytes, offset) == 0x004FFFFFu && WordAt(bytes, offset + 4) == 0x00000000u)
        {
            ++noops;
        }
    }

    VERIFY_ARE_EQUAL(size_t{ 2 }, noops);

    ClipFile read{};
    VERIFY_IS_TRUE(ReadClipFile(bytes, read).Succeeded());
    VERIFY_ARE_EQUAL(size_t{ 2 }, read.Events.size());
    VERIFY_ARE_EQUAL(int64_t{ 2500000 }, read.Events[1].Tick);
    VERIFY_ARE_EQUAL(int64_t{ 2500000 }, read.EndTick);
}

void ClipFileTests::WhatIsWrittenIsReadBack()
{
    ClipFile file{};
    file.TicksPerQuarterNote = 480;
    file.Profiles.push_back(Event(0, { 0x3006F07E, 0x7F0D2201 }));
    file.Configuration.push_back(Event(0, { 0xD0100000, 48387097, 0, 0 }));
    file.Configuration.push_back(Event(0, { 0x20C05100 }));
    file.Events.push_back(Event(0, { 0x40903C00, 0xC0000000 }));
    file.Events.push_back(Event(0, { 0x40903F00, 0xC0000000 }));
    file.Events.push_back(Event(240, { 0x40B04A00, 0x80000000 }));
    file.Events.push_back(Event(480, { 0x40803C00, 0x00000000 }));
    file.Events.push_back(Event(480, { 0x40803F00, 0x00000000 }));
    file.EndTick = 1920;

    ClipFile read{};
    auto const result = ReadClipFile(WriteClipFile(file), read);

    VERIFY_IS_TRUE(result.Succeeded());
    VERIFY_IS_FALSE(result.Truncated);
    VERIFY_ARE_EQUAL(uint16_t{ 480 }, read.TicksPerQuarterNote);
    VERIFY_IS_TRUE(file.Profiles == read.Profiles);
    VERIFY_IS_TRUE(file.Configuration == read.Configuration);
    VERIFY_IS_TRUE(file.Events == read.Events);
    VERIFY_ARE_EQUAL(file.EndTick, read.EndTick);
}

void ClipFileTests::AFileThatEndsEarlyKeepsWhatWasRead()
{
    ClipFile file{};

    for (int i = 0; i < 10; ++i)
    {
        file.Events.push_back(Event(i * 100, { 0x20903C64 }));
    }

    auto bytes = WriteClipFile(file);
    bytes.resize(bytes.size() - 10);

    ClipFile read{};
    auto const result = ReadClipFile(bytes, read);

    VERIFY_IS_TRUE(result.Succeeded());
    VERIFY_IS_TRUE(result.Truncated);
    VERIFY_ARE_EQUAL(size_t{ 10 }, read.Events.size());
}

void ClipFileTests::SomethingElseIsRefused()
{
    ClipFile read{};

    std::vector<uint8_t> const smf{ 'M', 'T', 'h', 'd', 0, 0, 0, 6, 0, 1, 0, 1, 1, 0xE0 };
    VERIFY_ARE_EQUAL(static_cast<int>(ClipFileReadStatus::NotAClipFile), static_cast<int>(ReadClipFile(smf, read).Status));

    std::vector<uint8_t> const tiny{ 'S', 'M', 'F' };
    VERIFY_ARE_EQUAL(static_cast<int>(ClipFileReadStatus::NotAClipFile), static_cast<int>(ReadClipFile(tiny, read).Status));

    ClipFile file{};

    for (int i = 0; i < 100; ++i)
    {
        file.Events.push_back(Event(i, { 0x20903C64 }));
    }

    VERIFY_ARE_EQUAL(static_cast<int>(ClipFileReadStatus::TooMuchData), static_cast<int>(ReadClipFile(WriteClipFile(file), read, 50).Status));
}

void ClipFileTests::TempoTimeSignatureAndTextMessages()
{
    uint32_t words[4]{};

    BuildSetTempo(120.0, 3, words);
    VERIFY_ARE_EQUAL(0xD3100000u, words[0]);
    VERIFY_ARE_EQUAL(50000000u, words[1]);

    double bpm{};
    VERIFY_IS_TRUE(ReadSetTempo(Event(0, { words[0], words[1], words[2], words[3] }), bpm));
    VERIFY_ARE_EQUAL(120.0, bpm);

    BuildSetTimeSignature(MeterChange{ 0, 6, 8 }, 0, words);
    VERIFY_ARE_EQUAL(0xD0100001u, words[0]);
    VERIFY_ARE_EQUAL(0x06030800u, words[1]);

    MeterChange meter{};
    VERIFY_IS_TRUE(ReadSetTimeSignature(Event(0, { words[0], words[1], words[2], words[3] }), meter));
    VERIFY_ARE_EQUAL(uint8_t{ 6 }, meter.Numerator);
    VERIFY_ARE_EQUAL(uint8_t{ 8 }, meter.Denominator);

    // A denominator power of 0 is one the format can't name.
    VERIFY_IS_FALSE(ReadSetTimeSignature(Event(0, { 0xD0100001, 0x04000800, 0, 0 }), meter));

    // Text longer than 12 bytes takes several packets: start, continue, end.
    std::wstring const name{ L"Night Drive Sketch \u2014 rough mix" };

    std::vector<ClipEvent> events{};
    AppendMetadataText(events, 0, MetadataText::ClipName, name, 0);

    VERIFY_ARE_EQUAL(size_t{ 3 }, events.size());
    VERIFY_ARE_EQUAL(1u, (events[0].Words[0] >> 22) & 0x03);
    VERIFY_ARE_EQUAL(2u, (events[1].Words[0] >> 22) & 0x03);
    VERIFY_ARE_EQUAL(3u, (events[2].Words[0] >> 22) & 0x03);
    VERIFY_ARE_EQUAL(0xD0500103u, events[0].Words[0]);

    ClipFile file{};
    file.Events = events;
    file.EndTick = 960;

    auto const imported = ImportClipFile(file);
    VERIFY_ARE_EQUAL(name, imported.Name);
}

void ClipFileTests::ATrackExportsAndImportsBack()
{
    auto const sequence = testdata::SampleSequence();
    auto const bass = FindTrack(sequence, L"t-bass");

    ClipFileText text{};
    text.SequenceName = sequence.Name;
    text.Copyright = L"\u00A9 2026 Pat Example";

    auto const exported = ExportTrackToClipFile(sequence, *bass, text);

    ClipFile read{};
    VERIFY_IS_TRUE(ReadClipFile(WriteClipFile(exported), read).Succeeded());

    auto const imported = ImportClipFile(read);
    auto const& clip = imported.Content;

    VERIFY_ARE_EQUAL(std::wstring{ L"Bass" }, imported.Name);
    VERIFY_ARE_EQUAL(CountNoteOns(sequence, *bass), clip.Notes.size());

    // The file carries the whole tempo and meter map, and the meter changes at bar 21.
    VERIFY_ARE_EQUAL(Bar * 20, clip.Length);

    // The first note: 16-bit velocity, unchanged, on the destination's channel.
    VERIFY_ARE_EQUAL(Bar * 4, clip.Notes.front().Tick);
    VERIFY_ARE_EQUAL(int64_t{ 403 }, clip.Notes.front().Length);
    VERIFY_ARE_EQUAL(uint16_t{ 0xFFFF }, clip.Notes.front().Velocity);
    VERIFY_ARE_EQUAL(uint8_t{ 1 }, clip.Notes.front().Channel);

    auto const held = std::find_if(clip.Notes.begin(), clip.Notes.end(), [](Note const& note) { return note.Number == 52; });
    VERIFY_IS_TRUE(held != clip.Notes.end());
    VERIFY_ARE_EQUAL(uint16_t{ 0x4000 }, held->ReleaseVelocity);

    // The controller and the per-note pitch bend, twice, on the destination's group.
    VERIFY_ARE_EQUAL(size_t{ 4 }, clip.Events.size());
    VERIFY_ARE_EQUAL(2u, (clip.Events.front().Words[0] >> 24) & 0x0F);

    VERIFY_ARE_EQUAL(size_t{ 2 }, imported.Startup.size());
    VERIFY_ARE_EQUAL(0x22C15100u, imported.Startup[0].Words[0]);

    VERIFY_ARE_EQUAL(size_t{ 3 }, imported.Tags.size());
    VERIFY_IS_FALSE(imported.Tempo.empty());
    VERIFY_IS_TRUE(std::fabs(imported.Tempo.front().BeatsPerMinute - 124.0) < 0.0001);

    // The ramp from bar 17 to 19 goes out as a step at every MIDI clock.
    auto const ramp = std::count_if(imported.Tempo.begin(), imported.Tempo.end(),
        [](TempoPoint const& point) { return point.Tick >= Bar * 16 && point.Tick < Bar * 18; });
    VERIFY_ARE_EQUAL(static_cast<ptrdiff_t>(Bar * 2 / (TicksPerQuarterNote / 24)), ramp);

    VERIFY_IS_TRUE(std::any_of(imported.Meter.begin(), imported.Meter.end(),
        [](MeterChange const& change) { return change.Tick == Bar * 20 && change.Numerator == 7 && change.Denominator == 8; }));
}

void StandardMidiFileTests::AnExportReadsBackWithTheSameNotes()
{
    auto const sequence = testdata::SampleSequence();

    auto const exported = ExportStandardMidiFile(sequence, {}, ClipFileText{ sequence.Name, L"", L"" });
    VERIFY_IS_TRUE(exported.Succeeded);

    auto const file = ParseOrFail(exported.Bytes);

    auto const expected = CountNoteOns(sequence, *FindTrack(sequence, L"t-drums")) + CountNoteOns(sequence, *FindTrack(sequence, L"t-bass"));
    VERIFY_ARE_EQUAL(expected, file.Notes.size());

    // A conductor track, then Drums, Bass and Pad.
    VERIFY_ARE_EQUAL(size_t{ 4 }, file.Tracks.size());
    VERIFY_ARE_EQUAL(std::string{ "Drums" }, file.Tracks[1].Name);
    VERIFY_ARE_EQUAL(uint16_t{ 960 }, file.Division.TicksPerQuarterNote);

    auto const& first = file.Notes.front();
    VERIFY_ARE_EQUAL(uint8_t{ 9 }, first.Channel);
    VERIFY_ARE_EQUAL(uint8_t{ 36 }, first.NoteNumber);
    VERIFY_ARE_EQUAL(uint8_t{ 127 }, first.Velocity);

    VERIFY_IS_TRUE(std::fabs(file.BeatsPerMinuteAtTick(0) - 124.0) < 0.01);
}

void StandardMidiFileTests::VelocityScalesUpOnImportAndBackOnExport()
{
    auto const sequence = testdata::SampleSequence();
    auto const first = ParseOrFail(ExportStandardMidiFile(sequence, {}, ClipFileText{}).Bytes);

    auto const imported = ImportStandardMidiFile(first, L"Night Drive.mid");

    for (auto const& clip : imported.Clips)
    {
        VERIFY_ARE_EQUAL(static_cast<int>(ClipOrigin::Imported), static_cast<int>(clip.Origin));
        VERIFY_ARE_EQUAL(std::wstring{ L"Night Drive.mid" }, clip.OriginDetail);

        for (auto const& note : clip.Notes)
        {
            // Every velocity is one a MIDI 1.0 value scales up to.
            VERIFY_ARE_EQUAL(note.Velocity, static_cast<uint16_t>(ScaleUp(ScaleDown(note.Velocity, 16, 7), 7, 16)));
            VERIFY_ARE_EQUAL(uint16_t{ 0x8000 }, note.ReleaseVelocity);
        }
    }

    Sequence again{};
    again.Tempo = imported.Tempo;
    again.Meter = imported.Meter;
    again.Tags = imported.Tags;
    again.Tracks = imported.Tracks;
    again.Clips = imported.Clips;
    NormalizeSequence(again);

    auto const second = ParseOrFail(ExportStandardMidiFile(again, {}, ClipFileText{}).Bytes);

    VERIFY_ARE_EQUAL(first.Notes.size(), second.Notes.size());

    for (size_t i = 0; i < first.Notes.size(); ++i)
    {
        VERIFY_ARE_EQUAL(first.Notes[i].StartTick, second.Notes[i].StartTick);
        VERIFY_ARE_EQUAL(first.Notes[i].EndTick, second.Notes[i].EndTick);
        VERIFY_ARE_EQUAL(first.Notes[i].NoteNumber, second.Notes[i].NoteNumber);
        VERIFY_ARE_EQUAL(first.Notes[i].Velocity, second.Notes[i].Velocity);
        VERIFY_ARE_EQUAL(first.Notes[i].Channel, second.Notes[i].Channel);
    }

    // Program and volume at the start came back as start-up messages.
    auto const withStartup = std::find_if(imported.Tracks.begin(), imported.Tracks.end(),
        [](Track const& track) { return !track.Startup.empty(); });
    VERIFY_IS_TRUE(withStartup != imported.Tracks.end());
    VERIFY_ARE_EQUAL(size_t{ 2 }, withStartup->Startup.size());
}

void StandardMidiFileTests::WhatMidi1CantSayIsCounted()
{
    auto const sequence = testdata::SampleSequence();
    auto const exported = ExportStandardMidiFile(sequence, { L"t-bass" }, ClipFileText{});

    VERIFY_IS_TRUE(exported.Succeeded);

    // A per-note pitch bend in each of the two placements has nowhere to go in MIDI 1.0.
    Log::Comment(String().Format(L"Skipped %u messages", exported.SkippedMessages));
    VERIFY_ARE_EQUAL(2u, exported.SkippedMessages);
}

void StandardMidiFileTests::TagsTravelAsMarkers()
{
    auto const sequence = testdata::SampleSequence();
    auto const file = ParseOrFail(ExportStandardMidiFile(sequence, {}, ClipFileText{}).Bytes);

    auto hasMarker = [&file](std::string const& text, uint32_t tick)
    {
        return std::any_of(file.TextEvents.begin(), file.TextEvents.end(), [&](midifile::TextEvent const& event)
        {
            return event.Kind == midifile::TextKind::Marker && event.Text == text && event.Tick == tick;
        });
    };

    VERIFY_IS_TRUE(hasMarker("Intro", 0));
    VERIFY_IS_TRUE(hasMarker("Drop", static_cast<uint32_t>(Bar * 16)));
    VERIFY_IS_TRUE(hasMarker("Filter opens here", static_cast<uint32_t>(Bar * 16)));

    auto const imported = ImportStandardMidiFile(file, L"x.mid");
    VERIFY_ARE_EQUAL(size_t{ 3 }, imported.Tags.size());
}
