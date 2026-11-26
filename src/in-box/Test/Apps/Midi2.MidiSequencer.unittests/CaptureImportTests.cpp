// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#include "CaptureImportTests.h"

#include "CaptureImport.h"

#include <string>
#include <string_view>

using namespace WEX::Common;
using namespace WEX::Logging;
using namespace WEX::TestExecution;
using namespace midisequencer;

namespace
{
    constexpr int64_t Beat = TicksPerQuarterNote;

    // What the console writes: words with 0x, a clock it saw, a line that isn't a message, and a
    // word count that doesn't match its words.
    constexpr std::string_view ConsoleCapture =
        "Index,TimestampTicks,OffsetMicroseconds,DeltaMicroseconds,Group,Channel,WordCount,Word0,Word1,Word2,Word3,MessageType,MessageName\r\n"
        "1,1000,0.000,0.000,1,1,1,0x20903C64,,,,Midi1ChannelVoice32,MIDI 1.0 Note On\r\n"
        "2,1100,250000.000,250000.000,1,,1,0x10F80000,,,,SystemCommon32,Timing Clock\r\n"
        "3,2000,500000.000,250000.000,1,1,1,0x20803C00,,,,Midi1ChannelVoice32,MIDI 1.0 Note Off\r\n"
        "4,3000,1000000.000,500000.000,2,3,2,0x41924000,0xC0000000,,,Midi2ChannelVoice64,MIDI 2.0 Note On\r\n"
        "5,3500,1250000.000,250000.000,1,1,1,0x20B00140,,,,Midi1ChannelVoice32,MIDI 1.0 Control Change\r\n"
        "6,4000,1500000.000,250000.000,2,3,2,0x41824000,0x00000000,,,Midi2ChannelVoice64,MIDI 2.0 Note Off\r\n"
        "this,is,not,a,message\r\n"
        "7,5000,1600000.000,100000.000,1,1,1,0x41924000,,,,Midi2ChannelVoice64,MIDI 2.0 Note On\r\n";

    CaptureText Text()
    {
        CaptureText text{};
        text.TrackName = [](uint8_t group) { return L"Group " + std::to_wstring(group + 1); };
        text.OriginDetail = L"jam.csv";
        return text;
    }
}

void CaptureImportTests::AConsoleCaptureBecomesATrackForEachGroup()
{
    auto const read = ReadCaptureCsv(ConsoleCapture);
    VERIFY_ARE_EQUAL(static_cast<int>(CaptureStatus::Read), static_cast<int>(read.Status));
    VERIFY_ARE_EQUAL(size_t{ 6 }, read.Messages.size());
    VERIFY_ARE_EQUAL(size_t{ 2 }, read.SkippedLines);

    auto const imported = ImportCapture(read.Messages, { TempoPoint{ 0, 120.0, false } }, { MeterChange{} }, Text());

    // The clock isn't something a track records.
    VERIFY_ARE_EQUAL(size_t{ 1 }, imported.SkippedMessages);
    VERIFY_ARE_EQUAL(size_t{ 2 }, imported.Tracks.size());
    VERIFY_ARE_EQUAL(size_t{ 2 }, imported.Clips.size());

    // Group 1: a MIDI 1.0 note for half a second (a beat at 120), and a controller at 1.25 s.
    auto const& first = imported.Tracks[0];
    VERIFY_ARE_EQUAL(std::wstring{ L"Group 1" }, first.Name);
    VERIFY_ARE_EQUAL(-1, static_cast<int>(first.Destination.Channel));
    VERIFY_ARE_EQUAL(size_t{ 1 }, first.Timeline.size());
    VERIFY_ARE_EQUAL(int64_t{ 0 }, first.Timeline[0].Tick);

    auto const& firstClip = imported.Clips[0];
    VERIFY_ARE_EQUAL(firstClip.Id, first.Timeline[0].ClipId);
    VERIFY_ARE_EQUAL(static_cast<int>(ClipOrigin::Recorded), static_cast<int>(firstClip.Origin));
    VERIFY_ARE_EQUAL(std::wstring{ L"jam.csv" }, firstClip.OriginDetail);
    VERIFY_ARE_EQUAL(size_t{ 1 }, firstClip.Notes.size());
    VERIFY_ARE_EQUAL(int64_t{ 0 }, firstClip.Notes[0].Tick);
    VERIFY_ARE_EQUAL(Beat, firstClip.Notes[0].Length);
    VERIFY_ARE_EQUAL(60, static_cast<int>(firstClip.Notes[0].Number));
    VERIFY_ARE_EQUAL(static_cast<uint32_t>(ScaleUp(100, 7, 16)), static_cast<uint32_t>(firstClip.Notes[0].Velocity));
    VERIFY_ARE_EQUAL(size_t{ 1 }, firstClip.Events.size());
    VERIFY_ARE_EQUAL(Beat * 5 / 2, firstClip.Events[0].Tick);

    // Group 2: a MIDI 2.0 note on channel 3, from 1 s to 1.5 s, at its full velocity.
    auto const& second = imported.Tracks[1];
    VERIFY_ARE_EQUAL(std::wstring{ L"Group 2" }, second.Name);

    auto const& secondClip = imported.Clips[1];
    VERIFY_ARE_EQUAL(size_t{ 1 }, secondClip.Notes.size());
    VERIFY_ARE_EQUAL(Beat * 2, secondClip.Notes[0].Tick);
    VERIFY_ARE_EQUAL(Beat, secondClip.Notes[0].Length);
    VERIFY_ARE_EQUAL(2, static_cast<int>(secondClip.Notes[0].Channel));
    VERIFY_ARE_EQUAL(64, static_cast<int>(secondClip.Notes[0].Number));
    VERIFY_ARE_EQUAL(uint16_t{ 0xC000 }, secondClip.Notes[0].Velocity);
}

void CaptureImportTests::AMidiMonitorCaptureReadsToo()
{
    // MIDI Monitor writes words without 0x, adds columns people read, and notes such as a pause
    // as lines with no words. Its times can start after zero.
    constexpr std::string_view capture =
        "\xEF\xBB\xBFIndex,TimestampTicks,OffsetMicroseconds,DeltaMicroseconds,Group,Channel,WordCount,Word0,Word1,Word2,Word3,MessageName,Decoded,Comment\r\n"
        "1,5000,10.000,0.000,1,1,1,20903C64,,,,MIDI 1.0 Note On,\"Note 60, velocity 100\",\r\n"
        ",,,,,,0,,,,,,\"Paused, then resumed\",\r\n"
        "2,6000,500010.000,500000.000,1,1,1,20803C00,,,,MIDI 1.0 Note Off,\"Note 60, velocity 0\",\"a \"\"quoted\"\" comment\"\r\n";

    auto const read = ReadCaptureCsv(capture);
    VERIFY_ARE_EQUAL(static_cast<int>(CaptureStatus::Read), static_cast<int>(read.Status));
    VERIFY_ARE_EQUAL(size_t{ 2 }, read.Messages.size());
    VERIFY_ARE_EQUAL(size_t{ 0 }, read.SkippedLines);

    auto const imported = ImportCapture(read.Messages, { TempoPoint{ 0, 120.0, false } }, { MeterChange{} }, Text());
    VERIFY_ARE_EQUAL(size_t{ 1 }, imported.Tracks.size());
    VERIFY_ARE_EQUAL(int64_t{ 0 }, imported.Clips[0].Notes[0].Tick);
    VERIFY_ARE_EQUAL(Beat, imported.Clips[0].Notes[0].Length);
}

void CaptureImportTests::TimesFollowTheTempo()
{
    // At 60 beats a minute, half a second is half a beat.
    auto const read = ReadCaptureCsv(ConsoleCapture);
    auto const imported = ImportCapture(read.Messages, { TempoPoint{ 0, 60.0, false } }, { MeterChange{} }, Text());

    VERIFY_ARE_EQUAL(size_t{ 2 }, imported.Clips.size());
    VERIFY_ARE_EQUAL(Beat / 2, imported.Clips[0].Notes[0].Length);
    VERIFY_ARE_EQUAL(Beat, imported.Clips[1].Notes[0].Tick);
}

void CaptureImportTests::OtherTextIsNotACapture()
{
    VERIFY_ARE_EQUAL(static_cast<int>(CaptureStatus::NotACapture), static_cast<int>(ReadCaptureCsv("").Status));
    VERIFY_ARE_EQUAL(static_cast<int>(CaptureStatus::NotACapture), static_cast<int>(ReadCaptureCsv("Name,Phone\r\nPat,555\r\n").Status));
    VERIFY_ARE_EQUAL(static_cast<int>(CaptureStatus::NotACapture), static_cast<int>(ReadCaptureCsv("\x00\x01\x02 not text").Status));

    // A header and nothing else is a capture with nothing in it.
    auto const empty = ReadCaptureCsv("Index,TimestampTicks,OffsetMicroseconds,DeltaMicroseconds,Group,Channel,WordCount,Word0,Word1,Word2,Word3\n");
    VERIFY_ARE_EQUAL(static_cast<int>(CaptureStatus::Read), static_cast<int>(empty.Status));
    VERIFY_IS_TRUE(empty.Messages.empty());
    VERIFY_IS_TRUE(ImportCapture(empty.Messages, { TempoPoint{} }, { MeterChange{} }, Text()).Tracks.empty());
}

void CaptureImportTests::CopiedWordsPasteOneAStep()
{
    // What MIDI Monitor's Copy as UMP words gives, plus a line that isn't words and a MIDI 2.0
    // message missing its second word.
    size_t skipped{ 0 };
    auto const messages = ReadUmpWordText(L"20903C64\r\n20803C00\r\n0x41924000 0xC0000000\r\n41824000 00000000\r\nnot words\r\n40B00100\r\n\r\n", skipped);

    VERIFY_ARE_EQUAL(size_t{ 4 }, messages.size());
    VERIFY_ARE_EQUAL(size_t{ 2 }, skipped);
    VERIFY_ARE_EQUAL(uint8_t{ 2 }, messages[2].WordCount);
    VERIFY_ARE_EQUAL(uint32_t{ 0xC0000000 }, messages[2].Words[1]);

    // A sixteenth apart from bar 2: a note a sixteenth long, then another after it.
    constexpr int64_t Bar = Beat * 4;
    auto const pasted = ClipFromMessages(messages, Bar, Beat / 4, { MeterChange{} }, L"Pasted", L"MIDI Monitor");

    VERIFY_ARE_EQUAL(Bar, pasted.PlacementTick);
    VERIFY_ARE_EQUAL(size_t{ 2 }, pasted.Clip.Notes.size());
    VERIFY_ARE_EQUAL(int64_t{ 0 }, pasted.Clip.Notes[0].Tick);
    VERIFY_ARE_EQUAL(Beat / 4, pasted.Clip.Notes[0].Length);
    VERIFY_ARE_EQUAL(60, static_cast<int>(pasted.Clip.Notes[0].Number));
    VERIFY_ARE_EQUAL(Beat / 2, pasted.Clip.Notes[1].Tick);
    VERIFY_ARE_EQUAL(64, static_cast<int>(pasted.Clip.Notes[1].Number));
    VERIFY_ARE_EQUAL(std::wstring{ L"MIDI Monitor" }, pasted.Clip.OriginDetail);

    // All at once, for a set-up dump: everything lands on the one tick.
    auto const together = ClipFromMessages(messages, Bar, 0, { MeterChange{} }, L"Pasted", L"MIDI Monitor");
    VERIFY_ARE_EQUAL(size_t{ 2 }, together.Clip.Notes.size());
    VERIFY_ARE_EQUAL(together.Clip.Notes[0].Tick, together.Clip.Notes[1].Tick);
}
