// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#include "SmfReaderTests.h"
#include "SmfTestFileBuilder.h"

#include "SmfReader.h"

#include <windows.h>

#include <map>
#include <string>
#include <vector>

using namespace WEX::Common;
using namespace WEX::Logging;
using namespace WEX::TestExecution;

using namespace midifile;
using namespace smftest;

namespace
{
    constexpr uint16_t TicksPerQuarterNote = 480;

    ReadResult Parse(std::vector<uint8_t> const& bytes, MidiSequence& sequence, ReadLimits const& limits = {})
    {
        return ParseStandardMidiFile(std::span<uint8_t const>{ bytes.data(), bytes.size() }, sequence, limits);
    }

    // A minimal but complete file: one note, one bar apart, with tempo and time signature.
    std::vector<uint8_t> BuildTypicalFile()
    {
        TrackBuilder conductor{};

        conductor
            .MetaText(0, 0x03, "Conductor")
            .Tempo(0, 500000)
            .TimeSignature(0, 4, 2)
            .EndOfTrack(1920);

        TrackBuilder music{};

        music
            .MetaText(0, 0x03, "Piano")
            .ProgramChange(0, 0, 4)
            .NoteOn(0, 0, 60, 100)
            .NoteOff(480, 0, 60, 0)
            .NoteOn(0, 0, 64, 100)
            .NoteOff(480, 0, 64, 0)
            .EndOfTrack(0);

        return BuildFile(1, TicksPerQuarterNote, { conductor, music });
    }
}

void SmfReaderTests::ReadsHeaderAndASingleNote()
{
    TrackBuilder track{};

    track.NoteOn(0, 0, 60, 100).NoteOff(480, 0, 60, 64).EndOfTrack();

    auto const file = BuildFile(0, TicksPerQuarterNote, { track });

    MidiSequence sequence{};
    auto const result = Parse(file, sequence);

    VERIFY_IS_TRUE(result.Succeeded());
    VERIFY_IS_FALSE(result.Truncated);
    VERIFY_ARE_EQUAL(1u, result.TracksRead);

    VERIFY_IS_TRUE(sequence.Format == SequenceFormat::SingleTrack);
    VERIFY_IS_FALSE(sequence.Division.IsSmpte);
    VERIFY_ARE_EQUAL(TicksPerQuarterNote, sequence.Division.TicksPerQuarterNote);

    VERIFY_ARE_EQUAL(size_t{ 2 }, sequence.Events.size());
    VERIFY_ARE_EQUAL(size_t{ 1 }, sequence.Notes.size());

    VERIFY_ARE_EQUAL(uint32_t{ 0 }, sequence.Notes[0].StartTick);
    VERIFY_ARE_EQUAL(uint32_t{ 480 }, sequence.Notes[0].EndTick);
    VERIFY_ARE_EQUAL(uint8_t{ 60 }, sequence.Notes[0].NoteNumber);
    VERIFY_ARE_EQUAL(uint8_t{ 100 }, sequence.Notes[0].Velocity);

    VERIFY_ARE_EQUAL(uint32_t{ 480 }, sequence.LastTick);
    VERIFY_ARE_EQUAL(uint16_t{ 0x0001 }, sequence.UsedChannelMask);
}

void SmfReaderTests::MergesTracksIntoOneTimeline()
{
    TrackBuilder first{};
    first.NoteOn(0, 0, 60, 100).NoteOff(960, 0, 60, 0).EndOfTrack();

    TrackBuilder second{};
    second.NoteOn(480, 1, 67, 90).NoteOff(480, 1, 67, 0).EndOfTrack();

    auto const file = BuildFile(1, TicksPerQuarterNote, { first, second });

    MidiSequence sequence{};
    VERIFY_IS_TRUE(Parse(file, sequence).Succeeded());

    VERIFY_ARE_EQUAL(size_t{ 4 }, sequence.Events.size());

    // ticks must come out in order regardless of which track they were written on
    for (size_t index = 1; index < sequence.Events.size(); ++index)
    {
        VERIFY_IS_TRUE(sequence.Events[index - 1].Tick <= sequence.Events[index].Tick);
    }

    VERIFY_ARE_EQUAL(uint32_t{ 0 }, sequence.Events[0].Tick);
    VERIFY_ARE_EQUAL(uint32_t{ 480 }, sequence.Events[1].Tick);
    VERIFY_ARE_EQUAL(uint32_t{ 960 }, sequence.Events[2].Tick);

    VERIFY_ARE_EQUAL(uint16_t{ 0x0003 }, sequence.UsedChannelMask);
    VERIFY_ARE_EQUAL(size_t{ 2 }, sequence.Tracks.size());
}

void SmfReaderTests::SkipsUnknownChunksBetweenTracks()
{
    TrackBuilder track{};
    track.NoteOn(0, 0, 60, 100).NoteOff(480, 0, 60, 0).EndOfTrack();

    auto file = BuildFile(0, TicksPerQuarterNote, { track });

    // an unrecognized chunk between the header and the track
    std::vector<uint8_t> withExtra{};

    withExtra.insert(withExtra.end(), file.begin(), file.begin() + 14);
    withExtra.insert(withExtra.end(), { 'X', 'Y', 'Z', 'Z', 0, 0, 0, 4, 1, 2, 3, 4 });
    withExtra.insert(withExtra.end(), file.begin() + 14, file.end());

    MidiSequence sequence{};
    auto const result = Parse(withExtra, sequence);

    VERIFY_IS_TRUE(result.Succeeded());
    VERIFY_ARE_EQUAL(size_t{ 1 }, sequence.Notes.size());
}

void SmfReaderTests::UnwrapsRiffWrappedFiles()
{
    auto const wrapped = WrapInRiff(BuildTypicalFile());

    MidiSequence sequence{};
    auto const result = Parse(wrapped, sequence);

    VERIFY_IS_TRUE(result.Succeeded());
    VERIFY_ARE_EQUAL(size_t{ 2 }, sequence.Notes.size());
}

void SmfReaderTests::LaysMultiSequenceFilesEndToEnd()
{
    TrackBuilder first{};
    first.NoteOn(0, 0, 60, 100).NoteOff(480, 0, 60, 0).EndOfTrack();

    TrackBuilder second{};
    second.NoteOn(0, 0, 62, 100).NoteOff(480, 0, 62, 0).EndOfTrack();

    auto const file = BuildFile(2, TicksPerQuarterNote, { first, second });

    MidiSequence sequence{};
    VERIFY_IS_TRUE(Parse(file, sequence).Succeeded());

    VERIFY_ARE_EQUAL(size_t{ 2 }, sequence.Notes.size());

    // the second sequence starts where the first one finished rather than on top of it
    VERIFY_ARE_EQUAL(uint32_t{ 0 }, sequence.Notes[0].StartTick);
    VERIFY_ARE_EQUAL(uint32_t{ 480 }, sequence.Notes[1].StartTick);
}

void SmfReaderTests::ExpandsRunningStatus()
{
    TrackBuilder track{};

    track
        .NoteOn(0, 0, 60, 100)
        .RunningStatusData(480, 62, 100)
        .RunningStatusData(480, 60, 0)
        .RunningStatusData(0, 62, 0)
        .EndOfTrack();

    auto const file = BuildFile(0, TicksPerQuarterNote, { track });

    MidiSequence sequence{};
    VERIFY_IS_TRUE(Parse(file, sequence).Succeeded());

    VERIFY_ARE_EQUAL(size_t{ 4 }, sequence.Events.size());

    // every stored message carries its own status byte, whatever the file did
    for (auto const& event : sequence.Events)
    {
        auto const bytes = sequence.BytesOf(event);

        VERIFY_ARE_EQUAL(size_t{ 3 }, bytes.size());
        VERIFY_ARE_EQUAL(uint8_t{ 0x90 }, bytes[0]);
    }

    VERIFY_ARE_EQUAL(size_t{ 2 }, sequence.Notes.size());
}

void SmfReaderTests::TreatsNoteOnWithZeroVelocityAsANoteOff()
{
    TrackBuilder track{};

    track.NoteOn(0, 0, 60, 100).NoteOn(240, 0, 60, 0).EndOfTrack();

    auto const file = BuildFile(0, TicksPerQuarterNote, { track });

    MidiSequence sequence{};
    VERIFY_IS_TRUE(Parse(file, sequence).Succeeded());

    VERIFY_ARE_EQUAL(size_t{ 1 }, sequence.Notes.size());
    VERIFY_ARE_EQUAL(uint32_t{ 240 }, sequence.Notes[0].EndTick);
}

void SmfReaderTests::PairsOverlappingCopiesOfTheSameNote()
{
    // the same note started twice before either end arrives, which is legal and does happen
    TrackBuilder track{};

    track
        .NoteOn(0, 0, 60, 100)
        .NoteOn(120, 0, 60, 80)
        .NoteOff(120, 0, 60, 0)
        .NoteOff(120, 0, 60, 0)
        .EndOfTrack();

    auto const file = BuildFile(0, TicksPerQuarterNote, { track });

    MidiSequence sequence{};
    VERIFY_IS_TRUE(Parse(file, sequence).Succeeded());

    VERIFY_ARE_EQUAL(size_t{ 2 }, sequence.Notes.size());

    // the most recent start is closed first, so the outer note is the longer one
    VERIFY_ARE_EQUAL(uint32_t{ 0 }, sequence.Notes[0].StartTick);
    VERIFY_ARE_EQUAL(uint32_t{ 360 }, sequence.Notes[0].EndTick);
    VERIFY_ARE_EQUAL(uint32_t{ 120 }, sequence.Notes[1].StartTick);
    VERIFY_ARE_EQUAL(uint32_t{ 240 }, sequence.Notes[1].EndTick);
}

void SmfReaderTests::HoldsANoteWhoseEndIsMissing()
{
    TrackBuilder track{};

    track.NoteOn(0, 0, 60, 100).NoteOn(960, 0, 72, 100).NoteOff(480, 0, 72, 0).EndOfTrack();

    auto const file = BuildFile(0, TicksPerQuarterNote, { track });

    MidiSequence sequence{};
    VERIFY_IS_TRUE(Parse(file, sequence).Succeeded());

    VERIFY_ARE_EQUAL(size_t{ 2 }, sequence.Notes.size());

    // note 60 never ends in the file, so it is held to the end rather than dropped
    VERIFY_ARE_EQUAL(uint8_t{ 60 }, sequence.Notes[0].NoteNumber);
    VERIFY_ARE_EQUAL(uint32_t{ 1440 }, sequence.Notes[0].EndTick);
}

void SmfReaderTests::KeepsSystemExclusiveWhole()
{
    std::vector<uint8_t> const payload{ 0x7E, 0x7F, 0x09, 0x01, 0xF7 };

    TrackBuilder track{};
    track.SystemExclusive(0, payload).EndOfTrack(480);

    auto const file = BuildFile(0, TicksPerQuarterNote, { track });

    MidiSequence sequence{};
    VERIFY_IS_TRUE(Parse(file, sequence).Succeeded());

    VERIFY_ARE_EQUAL(size_t{ 1 }, sequence.Events.size());
    VERIFY_IS_TRUE(sequence.Events[0].Kind == EventKind::SystemExclusive);

    auto const bytes = sequence.BytesOf(sequence.Events[0]);

    VERIFY_ARE_EQUAL(size_t{ 6 }, bytes.size());
    VERIFY_ARE_EQUAL(uint8_t{ 0xF0 }, bytes[0]);
    VERIFY_ARE_EQUAL(uint8_t{ 0xF7 }, bytes[5]);
}

void SmfReaderTests::ReassemblesSplitSystemExclusive()
{
    TrackBuilder track{};

    track
        .SystemExclusive(0, { 0x41, 0x10, 0x42 })
        .Escape(240, { 0x12, 0x40, 0x00 })
        .Escape(240, { 0x7F, 0x00, 0x41, 0xF7 })
        .EndOfTrack();

    auto const file = BuildFile(0, TicksPerQuarterNote, { track });

    MidiSequence sequence{};
    VERIFY_IS_TRUE(Parse(file, sequence).Succeeded());

    VERIFY_ARE_EQUAL(size_t{ 1 }, sequence.Events.size());
    VERIFY_IS_TRUE(sequence.Events[0].Kind == EventKind::SystemExclusive);

    auto const bytes = sequence.BytesOf(sequence.Events[0]);

    VERIFY_ARE_EQUAL(size_t{ 11 }, bytes.size());
    VERIFY_ARE_EQUAL(uint8_t{ 0xF0 }, bytes[0]);
    VERIFY_ARE_EQUAL(uint8_t{ 0x41 }, bytes[1]);
    VERIFY_ARE_EQUAL(uint8_t{ 0xF7 }, bytes[10]);

    // the reassembled dump is timed from the event that completed it
    VERIFY_ARE_EQUAL(uint32_t{ 480 }, sequence.Events[0].Tick);
}

void SmfReaderTests::RecordsBankWithProgramChange()
{
    TrackBuilder track{};

    track
        .ControlChange(0, 0, 0x00, 8)
        .ControlChange(0, 0, 0x20, 3)
        .ProgramChange(0, 0, 48)
        .NoteOn(0, 0, 60, 100)
        .NoteOff(480, 0, 60, 0)
        .EndOfTrack();

    auto const file = BuildFile(0, TicksPerQuarterNote, { track });

    MidiSequence sequence{};
    VERIFY_IS_TRUE(Parse(file, sequence).Succeeded());

    VERIFY_ARE_EQUAL(size_t{ 1 }, sequence.ProgramChanges.size());
    VERIFY_ARE_EQUAL(uint8_t{ 48 }, sequence.ProgramChanges[0].Program);
    VERIFY_ARE_EQUAL(uint8_t{ 8 }, sequence.ProgramChanges[0].BankMsb);
    VERIFY_ARE_EQUAL(uint8_t{ 3 }, sequence.ProgramChanges[0].BankLsb);
}

void SmfReaderTests::ReadsChordSymbolsFromSystemExclusive()
{
    // The shape found throughout real files: F0 00 20 24 00 01 <name> 0A, written both with and
    // without a closing F7.
    auto const chord = [](std::string const& name, bool terminate)
        {
            std::vector<uint8_t> payload{ 0x00, 0x20, 0x24, 0x00, 0x01 };

            for (auto const character : name)
            {
                payload.push_back(static_cast<uint8_t>(character));
            }

            payload.push_back(0x0A);

            if (terminate)
            {
                payload.push_back(0xF7);
            }

            return payload;
        };

    TrackBuilder track{};

    track
        .SystemExclusive(0, chord("Bb", true))
        .NoteOn(0, 0, 60, 100)
        .SystemExclusive(480, chord("F#m7", true))
        .SystemExclusive(480, chord("D/F#", false))
        .NoteOff(0, 0, 60, 0)
        .EndOfTrack();

    auto const file = BuildFile(0, TicksPerQuarterNote, { track });

    MidiSequence sequence{};
    VERIFY_IS_TRUE(Parse(file, sequence).Succeeded());

    VERIFY_ARE_EQUAL(size_t{ 3 }, sequence.TextEvents.size());

    VERIFY_IS_TRUE(sequence.TextEvents[0].Kind == TextKind::ChordSymbol);
    VERIFY_ARE_EQUAL(String(L"Bb"), String(String(sequence.TextEvents[0].Text.c_str())));
    VERIFY_ARE_EQUAL(uint32_t{ 0 }, sequence.TextEvents[0].Tick);

    VERIFY_ARE_EQUAL(String(L"F#m7"), String(String(sequence.TextEvents[1].Text.c_str())));
    VERIFY_ARE_EQUAL(uint32_t{ 480 }, sequence.TextEvents[1].Tick);

    // The unterminated one is still read, because the symbol is worth having either way
    VERIFY_ARE_EQUAL(String(L"D/F#"), String(String(sequence.TextEvents[2].Text.c_str())));

    // The message is passed on to the device as well, not swallowed. The unterminated one is not
    // put on the wire, which is why there are two and not three.
    size_t systemExclusiveCount = 0;

    for (auto const& event : sequence.Events)
    {
        if (event.Kind == EventKind::SystemExclusive)
        {
            ++systemExclusiveCount;
        }
    }

    VERIFY_ARE_EQUAL(size_t{ 2 }, systemExclusiveCount);
}

void SmfReaderTests::IgnoresSystemExclusiveThatIsNotAChordSymbol()
{
    TrackBuilder track{};

    track
        .SystemExclusive(0, { 0x7E, 0x7F, 0x09, 0x01, 0xF7 })                          // identity
        .SystemExclusive(0, { 0x00, 0x20, 0x24, 0x00, 0x02, 0x41, 0x0A, 0xF7 })        // same maker, other message
        .SystemExclusive(0, { 0x00, 0x20, 0x24, 0x00, 0x01, 0x03, 0x7F, 0x0A, 0xF7 })  // binary, not a name
        .SystemExclusive(0, { 0x00, 0x20, 0x24, 0x00, 0x01, 0x0A, 0xF7 })              // empty name
        .NoteOn(0, 0, 60, 100)
        .NoteOff(480, 0, 60, 0)
        .EndOfTrack();

    auto const file = BuildFile(0, TicksPerQuarterNote, { track });

    MidiSequence sequence{};
    VERIFY_IS_TRUE(Parse(file, sequence).Succeeded());

    VERIFY_ARE_EQUAL(size_t{ 0 }, sequence.TextEvents.size());
}

void SmfReaderTests::ConvertsTicksToTimeAtOneTempo()
{
    auto const file = BuildTypicalFile();

    MidiSequence sequence{};
    VERIFY_IS_TRUE(Parse(file, sequence).Succeeded());

    // 500000 microseconds per quarter note, 480 ticks per quarter note
    VERIFY_ARE_EQUAL(uint64_t{ 0 }, sequence.MicrosecondsAtTick(0));
    VERIFY_ARE_EQUAL(uint64_t{ 500000 }, sequence.MicrosecondsAtTick(480));
    VERIFY_ARE_EQUAL(uint64_t{ 2000000 }, sequence.MicrosecondsAtTick(1920));

    VERIFY_ARE_EQUAL(120.0, sequence.BeatsPerMinuteAtTick(0));
}

void SmfReaderTests::AccumulatesMultipleTempoChanges()
{
    TrackBuilder track{};

    track
        .Tempo(0, 500000)           // 120 beats per minute
        .NoteOn(0, 0, 60, 100)
        .Tempo(960, 250000)         // 240 beats per minute, two quarter notes in
        .NoteOff(960, 0, 60, 0)
        .EndOfTrack();

    auto const file = BuildFile(0, TicksPerQuarterNote, { track });

    MidiSequence sequence{};
    VERIFY_IS_TRUE(Parse(file, sequence).Succeeded());

    VERIFY_ARE_EQUAL(size_t{ 2 }, sequence.TempoMap.size());

    // two quarter notes at 120, then two at 240
    VERIFY_ARE_EQUAL(uint64_t{ 1000000 }, sequence.MicrosecondsAtTick(960));
    VERIFY_ARE_EQUAL(uint64_t{ 1500000 }, sequence.MicrosecondsAtTick(1920));

    VERIFY_ARE_EQUAL(240.0, sequence.BeatsPerMinuteAtTick(960));
    VERIFY_ARE_EQUAL(120.0, sequence.BeatsPerMinuteAtTick(959));
}

void SmfReaderTests::RoundTripsTicksAndTime()
{
    TrackBuilder track{};

    track
        .Tempo(0, 500000)
        .NoteOn(0, 0, 60, 100)
        .Tempo(960, 300000)
        .NoteOff(1920, 0, 60, 0)
        .EndOfTrack();

    auto const file = BuildFile(0, TicksPerQuarterNote, { track });

    MidiSequence sequence{};
    VERIFY_IS_TRUE(Parse(file, sequence).Succeeded());

    for (uint32_t tick = 0; tick <= 2880; tick += 37)
    {
        auto const microseconds = sequence.MicrosecondsAtTick(tick);
        auto const back = sequence.TickAtMicroseconds(microseconds);

        // integer division loses at most a tick either way
        VERIFY_IS_LESS_THAN_OR_EQUAL(tick > back ? tick - back : back - tick, uint32_t{ 1 });
    }
}

void SmfReaderTests::CountsBarsAndBeats()
{
    TrackBuilder track{};

    track
        .TimeSignature(0, 4, 2)         // 4/4
        .NoteOn(0, 0, 60, 100)
        .TimeSignature(1920, 3, 2)      // 3/4 from bar 2
        .NoteOff(1920, 0, 60, 0)
        .EndOfTrack();

    auto const file = BuildFile(0, TicksPerQuarterNote, { track });

    MidiSequence sequence{};
    VERIFY_IS_TRUE(Parse(file, sequence).Succeeded());

    VERIFY_ARE_EQUAL(size_t{ 2 }, sequence.TimeSignatureMap.size());
    VERIFY_ARE_EQUAL(uint32_t{ 1920 }, sequence.TimeSignatureMap[0].TicksPerBar);
    VERIFY_ARE_EQUAL(uint32_t{ 1440 }, sequence.TimeSignatureMap[1].TicksPerBar);

    auto position = sequence.BarPositionAtTick(0);
    VERIFY_ARE_EQUAL(uint32_t{ 1 }, position.Bar);
    VERIFY_ARE_EQUAL(uint32_t{ 1 }, position.Beat);

    position = sequence.BarPositionAtTick(960);
    VERIFY_ARE_EQUAL(uint32_t{ 1 }, position.Bar);
    VERIFY_ARE_EQUAL(uint32_t{ 3 }, position.Beat);

    // the new time signature starts bar 2, and 3/4 bars follow
    position = sequence.BarPositionAtTick(1920);
    VERIFY_ARE_EQUAL(uint32_t{ 2 }, position.Bar);
    VERIFY_ARE_EQUAL(uint32_t{ 1 }, position.Beat);

    position = sequence.BarPositionAtTick(1920 + 1440);
    VERIFY_ARE_EQUAL(uint32_t{ 3 }, position.Bar);
    VERIFY_ARE_EQUAL(uint32_t{ 1 }, position.Beat);
}

void SmfReaderTests::UsesRealTimeForSmpteDivision()
{
    // 25 frames per second, 40 ticks per frame, so exactly 1000 ticks per second
    uint16_t const division = static_cast<uint16_t>((static_cast<uint8_t>(-25) << 8) | 40);

    TrackBuilder track{};
    track.NoteOn(0, 0, 60, 100).NoteOff(1000, 0, 60, 0).EndOfTrack();

    auto const file = BuildFile(0, division, { track });

    MidiSequence sequence{};
    VERIFY_IS_TRUE(Parse(file, sequence).Succeeded());

    VERIFY_IS_TRUE(sequence.Division.IsSmpte);
    VERIFY_ARE_EQUAL(uint8_t{ 25 }, sequence.Division.FramesPerSecond);
    VERIFY_ARE_EQUAL(uint8_t{ 40 }, sequence.Division.TicksPerFrame);

    // a tempo event must not move real time
    VERIFY_ARE_EQUAL(uint64_t{ 1000000 }, sequence.MicrosecondsAtTick(1000));
}

void SmfReaderTests::TakesTrackNameFromMetaEvent()
{
    TrackBuilder track{};

    track
        .MetaText(0, 0x03, "Lead Synth")
        .MetaText(0, 0x02, "Copyright Contoso")
        .NoteOn(0, 0, 60, 100)
        .NoteOff(480, 0, 60, 0)
        .EndOfTrack();

    auto const file = BuildFile(0, TicksPerQuarterNote, { track });

    MidiSequence sequence{};
    VERIFY_IS_TRUE(Parse(file, sequence).Succeeded());

    VERIFY_ARE_EQUAL(String(L"Lead Synth"), String(String(sequence.Tracks[0].Name.c_str())));
    VERIFY_ARE_EQUAL(String(L"Copyright Contoso"), String(String(sequence.Copyright.c_str())));
    VERIFY_ARE_EQUAL(String(L"Lead Synth"), String(String(sequence.Title.c_str())));
}

void SmfReaderTests::KeepsLyricsInOrder()
{
    TrackBuilder track{};

    track
        .MetaText(0, 0x05, "Twin-")
        .NoteOn(0, 0, 60, 100)
        .MetaText(480, 0x05, "kle")
        .NoteOff(0, 0, 60, 0)
        .MetaText(480, 0x06, "Chorus")
        .EndOfTrack();

    auto const file = BuildFile(0, TicksPerQuarterNote, { track });

    MidiSequence sequence{};
    VERIFY_IS_TRUE(Parse(file, sequence).Succeeded());

    VERIFY_ARE_EQUAL(size_t{ 3 }, sequence.TextEvents.size());
    VERIFY_ARE_EQUAL(uint32_t{ 0 }, sequence.TextEvents[0].Tick);
    VERIFY_ARE_EQUAL(uint32_t{ 480 }, sequence.TextEvents[1].Tick);
    VERIFY_ARE_EQUAL(uint32_t{ 960 }, sequence.TextEvents[2].Tick);
    VERIFY_IS_TRUE(sequence.TextEvents[2].Kind == TextKind::Marker);
}

void SmfReaderTests::FindsTheChordSymbolInEffectAtATick()
{
    auto const chord = [](std::string const& name)
        {
            std::vector<uint8_t> payload{ 0x00, 0x20, 0x24, 0x00, 0x01 };

            for (auto const character : name)
            {
                payload.push_back(static_cast<uint8_t>(character));
            }

            payload.push_back(0x0A);
            payload.push_back(0xF7);

            return payload;
        };

    TrackBuilder track{};

    track
        .NoteOn(0, 0, 60, 100)
        .SystemExclusive(480, chord("C"))
        .SystemExclusive(480, chord("Am7"))
        .SystemExclusive(960, chord("F"))
        .NoteOff(480, 0, 60, 0)
        .EndOfTrack();

    auto const file = BuildFile(0, TicksPerQuarterNote, { track });

    MidiSequence sequence{};
    VERIFY_IS_TRUE(Parse(file, sequence).Succeeded());

    VERIFY_ARE_EQUAL(size_t{ 3 }, sequence.ChordSymbolIndexes.size());

    // before the first one there is nothing to show
    VERIFY_IS_NULL(sequence.ChordSymbolAtTick(0));
    VERIFY_IS_NULL(sequence.ChordSymbolAtTick(479));

    VERIFY_IS_NOT_NULL(sequence.ChordSymbolAtTick(480));
    VERIFY_ARE_EQUAL(String(L"C"), String(String(sequence.ChordSymbolAtTick(480)->Text.c_str())));

    // it stays in effect until the next one
    VERIFY_ARE_EQUAL(String(L"C"), String(String(sequence.ChordSymbolAtTick(959)->Text.c_str())));
    VERIFY_ARE_EQUAL(String(L"Am7"), String(String(sequence.ChordSymbolAtTick(960)->Text.c_str())));
    VERIFY_ARE_EQUAL(String(L"Am7"), String(String(sequence.ChordSymbolAtTick(1919)->Text.c_str())));
    VERIFY_ARE_EQUAL(String(L"F"), String(String(sequence.ChordSymbolAtTick(1920)->Text.c_str())));

    // and past the end of the file
    VERIFY_ARE_EQUAL(String(L"F"), String(String(sequence.ChordSymbolAtTick(100000)->Text.c_str())));
}

void SmfReaderTests::CountsNotesSoundingPerTrack()
{
    TrackBuilder first{};
    first
        .NoteOn(0, 0, 60, 100)
        .NoteOn(0, 0, 64, 100)      // a two note chord held for a whole note
        .NoteOff(1920, 0, 60, 0)
        .NoteOff(0, 0, 64, 0)
        .EndOfTrack();

    TrackBuilder second{};
    second
        .NoteOn(480, 1, 40, 100)    // one note, from beat 2 to beat 3
        .NoteOff(480, 1, 40, 0)
        .EndOfTrack();

    auto const file = BuildFile(1, TicksPerQuarterNote, { first, second });

    MidiSequence sequence{};
    VERIFY_IS_TRUE(Parse(file, sequence).Succeeded());

    std::vector<uint8_t> counts{};

    sequence.CollectSoundingNoteCounts(0, counts);
    VERIFY_ARE_EQUAL(size_t{ 2 }, counts.size());
    VERIFY_ARE_EQUAL(uint8_t{ 2 }, counts[0]);
    VERIFY_ARE_EQUAL(uint8_t{ 0 }, counts[1]);

    sequence.CollectSoundingNoteCounts(600, counts);
    VERIFY_ARE_EQUAL(uint8_t{ 2 }, counts[0]);
    VERIFY_ARE_EQUAL(uint8_t{ 1 }, counts[1]);

    // the second track's note has ended, the first track is still holding
    sequence.CollectSoundingNoteCounts(1000, counts);
    VERIFY_ARE_EQUAL(uint8_t{ 2 }, counts[0]);
    VERIFY_ARE_EQUAL(uint8_t{ 0 }, counts[1]);

    // a note is over at the instant it ends, not after it
    sequence.CollectSoundingNoteCounts(1920, counts);
    VERIFY_ARE_EQUAL(uint8_t{ 0 }, counts[0]);
    VERIFY_ARE_EQUAL(uint8_t{ 0 }, counts[1]);
}

void SmfReaderTests::RejectsSomethingThatIsNotAMidiFile()
{
    std::vector<uint8_t> const notMidi{ 'h', 'e', 'l', 'l', 'o', ' ', 't', 'h', 'e', 'r', 'e', '!', '!', '!', '!' };

    MidiSequence sequence{};
    auto const result = Parse(notMidi, sequence);

    VERIFY_IS_FALSE(result.Succeeded());
    VERIFY_IS_TRUE(result.Status == ReadStatus::NotAMidiFile);
    VERIFY_IS_TRUE(sequence.IsEmpty());
}

void SmfReaderTests::RejectsATruncatedHeader()
{
    std::vector<uint8_t> const shortFile{ 'M', 'T', 'h', 'd', 0, 0, 0, 6, 0, 0 };

    MidiSequence sequence{};
    auto const result = Parse(shortFile, sequence);

    VERIFY_IS_FALSE(result.Succeeded());
    VERIFY_IS_TRUE(sequence.IsEmpty());
}

void SmfReaderTests::ClampsATrackLengthThatOverrunsTheFile()
{
    TrackBuilder track{};
    track.NoteOn(0, 0, 60, 100).NoteOff(480, 0, 60, 0).EndOfTrack();

    // the file claims a track far longer than the bytes that follow
    auto const file = BuildFileWithTrackLength(0, TicksPerQuarterNote, track, 0x7FFFFFFF);

    MidiSequence sequence{};
    auto const result = Parse(file, sequence);

    VERIFY_IS_TRUE(result.Succeeded());
    VERIFY_IS_TRUE(result.Truncated);
    VERIFY_ARE_EQUAL(size_t{ 1 }, sequence.Notes.size());
}

void SmfReaderTests::KeepsWhatItReadFromATruncatedTrack()
{
    TrackBuilder track{};
    track.NoteOn(0, 0, 60, 100).NoteOn(480, 0, 64, 100).NoteOff(480, 0, 64, 0).EndOfTrack();

    auto file = BuildFile(0, TicksPerQuarterNote, { track });

    // cut the file off part way through the last note off
    file.resize(file.size() - 6);

    MidiSequence sequence{};
    auto const result = Parse(file, sequence);

    VERIFY_IS_TRUE(result.Succeeded());
    VERIFY_IS_TRUE(result.Truncated);
    VERIFY_IS_LESS_THAN_OR_EQUAL(size_t{ 1 }, sequence.Notes.size());
}

void SmfReaderTests::StopsOnADataByteWithNoStatus()
{
    TrackBuilder track{};

    // a delta time followed by a data byte, with no status byte ever having been seen
    track.RunningStatusData(0, 0x40, 0x40).NoteOn(0, 0, 60, 100).EndOfTrack();

    auto const file = BuildFile(0, TicksPerQuarterNote, { track });

    MidiSequence sequence{};
    auto const result = Parse(file, sequence);

    // nothing after the bad byte can be interpreted, so there is nothing to play
    VERIFY_IS_FALSE(result.Succeeded());
    VERIFY_IS_TRUE(result.Status == ReadStatus::NoPlayableData);
}

void SmfReaderTests::RejectsAnOverlongVariableLengthQuantity()
{
    TrackBuilder track{};

    // five continuation bytes, one more than the format allows
    track.Raw({ 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x00 }).NoteOn(0, 0, 60, 100).EndOfTrack();

    auto const file = BuildFile(0, TicksPerQuarterNote, { track });

    MidiSequence sequence{};
    auto const result = Parse(file, sequence);

    VERIFY_IS_FALSE(result.Succeeded());
    VERIFY_IS_TRUE(result.Status == ReadStatus::NoPlayableData);
}

void SmfReaderTests::RejectsAFileWithNoPlayableData()
{
    TrackBuilder track{};
    track.MetaText(0, 0x03, "Nothing here").EndOfTrack();

    auto const file = BuildFile(0, TicksPerQuarterNote, { track });

    MidiSequence sequence{};
    auto const result = Parse(file, sequence);

    VERIFY_IS_FALSE(result.Succeeded());
    VERIFY_IS_TRUE(result.Status == ReadStatus::NoPlayableData);
}

void SmfReaderTests::HonorsTheEventCountLimit()
{
    TrackBuilder track{};

    for (int index = 0; index < 500; ++index)
    {
        track.NoteOn(0, 0, 60, 100).NoteOff(10, 0, 60, 0);
    }

    track.EndOfTrack();

    auto const file = BuildFile(0, TicksPerQuarterNote, { track });

    ReadLimits limits{};
    limits.MaximumEvents = 100;

    MidiSequence sequence{};
    auto const result = Parse(file, sequence, limits);

    VERIFY_IS_TRUE(result.Succeeded());
    VERIFY_IS_TRUE(result.Truncated);
    VERIFY_ARE_EQUAL(size_t{ 100 }, sequence.Events.size());
}

void SmfReaderTests::SurvivesEveryTruncationOfAValidFile()
{
    auto const file = BuildTypicalFile();

    // Nothing here asserts a result. The point is that no prefix of a real file, of which there
    // are plenty in the world, may fault, hang or allocate without bound.
    for (size_t length = 0; length <= file.size(); ++length)
    {
        std::vector<uint8_t> const prefix{ file.begin(), file.begin() + static_cast<ptrdiff_t>(length) };

        MidiSequence sequence{};
        auto const result = Parse(prefix, sequence);

        if (result.Succeeded())
        {
            VERIFY_IS_FALSE(sequence.Events.empty());
            VERIFY_IS_FALSE(sequence.TempoMap.empty());
        }
        else
        {
            VERIFY_IS_TRUE(sequence.IsEmpty());
        }
    }

    Log::Comment(String().Format(L"Parsed %d truncations without a fault.", static_cast<int>(file.size() + 1)));
}

void SmfReaderTests::SurvivesCorruptedBytesInAValidFile()
{
    auto const original = BuildTypicalFile();

    uint8_t const corruptions[]{ 0x00, 0x7F, 0x80, 0xF0, 0xF7, 0xFF };

    for (size_t index = 0; index < original.size(); ++index)
    {
        for (auto const value : corruptions)
        {
            auto damaged = original;
            damaged[index] = value;

            MidiSequence sequence{};
            auto const result = Parse(damaged, sequence);

            // Either it reads something coherent or it refuses. Both are fine; a crash is not.
            if (result.Succeeded())
            {
                VERIFY_IS_FALSE(sequence.TempoMap.empty());
                VERIFY_IS_TRUE(sequence.TempoMap[0].Tick == 0);

                for (auto const& event : sequence.Events)
                {
                    VERIFY_IS_TRUE(
                        static_cast<size_t>(event.ByteOffset) + event.ByteCount <= sequence.EventBytes.size());
                }

                for (auto const& note : sequence.Notes)
                {
                    VERIFY_IS_TRUE(note.EndTick >= note.StartTick);
                }
            }
        }
    }

    Log::Comment(String().Format(
        L"Parsed %d single byte corruptions without a fault.",
        static_cast<int>(original.size() * _countof(corruptions))));
}

namespace
{
    void ReportFile(std::wstring const& path)
    {
        MidiSequence sequence{};
        auto const result = ReadStandardMidiFile(path, sequence);

        if (!result.Succeeded())
        {
            Log::Comment(String().Format(
                L"  %s: status %d at offset %llu",
                path.c_str(),
                static_cast<int>(result.Status),
                result.ByteOffset));

            return;
        }

        for (auto const& event : sequence.Events)
        {
            VERIFY_IS_TRUE(static_cast<size_t>(event.ByteOffset) + event.ByteCount <= sequence.EventBytes.size());
            VERIFY_IS_TRUE(event.TrackIndex < sequence.Tracks.size());
        }

        for (auto const& note : sequence.Notes)
        {
            VERIFY_IS_TRUE(note.EndTick >= note.StartTick);
        }

        VERIFY_IS_FALSE(sequence.TempoMap.empty());
        VERIFY_IS_FALSE(sequence.TimeSignatureMap.empty());

        auto const position = sequence.BarPositionAtTick(sequence.LastTick);

        Log::Comment(String().Format(
            L"  %s: format %d, %d tracks, %d events, %d notes, %d tempo changes, %.1f s, %d bars, ppqn %d%s",
            path.c_str(),
            static_cast<int>(sequence.Format),
            static_cast<int>(sequence.Tracks.size()),
            static_cast<int>(sequence.Events.size()),
            static_cast<int>(sequence.Notes.size()),
            static_cast<int>(sequence.TempoMap.size()),
            static_cast<double>(sequence.DurationMicroseconds) / 1000000.0,
            static_cast<int>(position.Bar),
            static_cast<int>(sequence.Division.TicksPerQuarterNote),
            result.Truncated ? L" (truncated)" : L""));
    }

    void ReportFileInDetail(std::wstring const& path)
    {
        MidiSequence sequence{};
        auto const result = ReadStandardMidiFile(path, sequence);

        Log::Comment(String().Format(L"file: %s", path.c_str()));
        Log::Comment(String().Format(
            L"  status %d, truncated %d, tracks declared %d read %d",
            static_cast<int>(result.Status),
            result.Truncated ? 1 : 0,
            result.TracksDeclared,
            result.TracksRead));

        if (!result.Succeeded())
        {
            return;
        }

        Log::Comment(String().Format(
            L"  smpte %d, ppqn %d, frames %d, ticksPerFrame %d",
            sequence.Division.IsSmpte ? 1 : 0,
            static_cast<int>(sequence.Division.TicksPerQuarterNote),
            static_cast<int>(sequence.Division.FramesPerSecond),
            static_cast<int>(sequence.Division.TicksPerFrame)));

        Log::Comment(String().Format(
            L"  events %d, notes %d, lastTick %u, duration %.3f s",
            static_cast<int>(sequence.Events.size()),
            static_cast<int>(sequence.Notes.size()),
            sequence.LastTick,
            static_cast<double>(sequence.DurationMicroseconds) / 1000000.0));

        Log::Comment(String().Format(L"  tempo map (%d entries):", static_cast<int>(sequence.TempoMap.size())));

        size_t shown = 0;

        for (auto const& entry : sequence.TempoMap)
        {
            if (shown++ >= 8)
            {
                Log::Comment(L"    ...");
                break;
            }

            Log::Comment(String().Format(
                L"    tick %u  usPerQuarter %u  (%.2f BPM)  atMicroseconds %llu",
                entry.Tick,
                entry.MicrosecondsPerQuarterNote,
                entry.MicrosecondsPerQuarterNote == 0 ? 0.0 : 60000000.0 / entry.MicrosecondsPerQuarterNote,
                entry.MicrosecondsAtTick));
        }

        auto const from = sequence.Events.size() > 5 ? sequence.Events.size() - 5 : size_t{ 0 };

        Log::Comment(L"  last events:");

        for (size_t index = from; index < sequence.Events.size(); ++index)
        {
            Log::Comment(String().Format(
                L"    tick %u  kind %d  track %d",
                sequence.Events[index].Tick,
                static_cast<int>(sequence.Events[index].Kind),
                static_cast<int>(sequence.Events[index].TrackIndex)));
        }

        // Where a runaway delta time first shows up, which is what a huge duration means.
        uint32_t biggestJump = 0;
        size_t jumpIndex = 0;

        for (size_t index = 1; index < sequence.Events.size(); ++index)
        {
            auto const jump = sequence.Events[index].Tick - sequence.Events[index - 1].Tick;

            if (jump > biggestJump)
            {
                biggestJump = jump;
                jumpIndex = index;
            }
        }

        // The shape of the timeline. A healthy file climbs smoothly.
        Log::Comment(L"  tick progression:");

        for (int step = 0; step <= 10; ++step)
        {
            auto const index = (sequence.Events.size() - 1) * step / 10;

            Log::Comment(String().Format(
                L"    %3d%%  event %6d  tick %u",
                step * 10,
                static_cast<int>(index),
                sequence.Events[index].Tick));
        }

        // The first delta time too large to be musical, at 192 ticks per quarter note a jump of
        // 100000 is well over a minute of silence.
        for (size_t index = 1; index < sequence.Events.size(); ++index)
        {
            if (sequence.Events[index].Tick - sequence.Events[index - 1].Tick > 100000)
            {
                Log::Comment(String().Format(
                    L"  first jump over 100000 ticks: event %d, %u -> %u",
                    static_cast<int>(index),
                    sequence.Events[index - 1].Tick,
                    sequence.Events[index].Tick));
                break;
            }
        }

        if (biggestJump > 0)
        {
            Log::Comment(String().Format(
                L"  biggest tick jump: %u at event %d (of %d)",
                biggestJump,
                static_cast<int>(jumpIndex),
                static_cast<int>(sequence.Events.size())));

            auto const first = jumpIndex > 3 ? jumpIndex - 3 : size_t{ 0 };
            auto const last = (jumpIndex + 3) < sequence.Events.size() ? jumpIndex + 3 : sequence.Events.size() - 1;

            for (size_t index = first; index <= last; ++index)
            {
                auto const bytes = sequence.BytesOf(sequence.Events[index]);

                std::wstring hex{};

                for (size_t byteIndex = 0; byteIndex < bytes.size() && byteIndex < 8; ++byteIndex)
                {
                    wchar_t pair[4]{};
                    ::swprintf_s(pair, L"%02X ", bytes[byteIndex]);
                    hex += pair;
                }

                Log::Comment(String().Format(
                    L"    [%d] tick %u  kind %d  bytes %s",
                    static_cast<int>(index),
                    sequence.Events[index].Tick,
                    static_cast<int>(sequence.Events[index].Kind),
                    hex.c_str()));
            }
        }
    }

    // Aggregate rather than per event, because a corpus run reads millions of events and a
    // VERIFY for each one would spend all its time logging.
    struct CorpusTally
    {
        int Files{ 0 };
        int Playable{ 0 };
        int Truncated{ 0 };
        int InvariantViolations{ 0 };

        int64_t TotalBytes{ 0 };
        int64_t TotalEvents{ 0 };
        int64_t TotalNotes{ 0 };

        // A file claiming to be hours long is either corrupt or was mis-parsed. Either way the
        // player would show it, so the rate matters.
        int OverTenMinutes{ 0 };
        int OverOneHour{ 0 };
        std::vector<std::wstring> LongExamples{};

        int FilesWithChords{ 0 };
        int64_t TotalChordSymbols{ 0 };
        std::vector<std::wstring> ChordExamples{};

        std::map<int, int> StatusCounts{};

        int64_t LargestBytes{ 0 };
        std::wstring LargestFile{};

        int64_t MostEvents{ 0 };
        std::wstring MostEventsFile{};

        double LongestSeconds{ 0.0 };
        std::wstring LongestFile{};

        double SlowestMilliseconds{ 0.0 };
        std::wstring SlowestFile{};
    };

    void CheckInvariants(MidiSequence const& sequence, CorpusTally& tally) noexcept
    {
        for (auto const& event : sequence.Events)
        {
            if (static_cast<size_t>(event.ByteOffset) + event.ByteCount > sequence.EventBytes.size() ||
                event.TrackIndex >= sequence.Tracks.size())
            {
                ++tally.InvariantViolations;
                return;
            }
        }

        for (auto const& note : sequence.Notes)
        {
            if (note.EndTick < note.StartTick || note.NoteNumber > 127 || note.Channel > 15)
            {
                ++tally.InvariantViolations;
                return;
            }
        }

        if (sequence.TempoMap.empty() || sequence.TempoMap.front().Tick != 0 ||
            sequence.TimeSignatureMap.empty() || sequence.TimeSignatureMap.front().Tick != 0)
        {
            ++tally.InvariantViolations;
            return;
        }

        // The maps have to be sorted, or a seek lands in the wrong place.
        for (size_t index = 1; index < sequence.TempoMap.size(); ++index)
        {
            if (sequence.TempoMap[index].Tick <= sequence.TempoMap[index - 1].Tick ||
                sequence.TempoMap[index].MicrosecondsAtTick < sequence.TempoMap[index - 1].MicrosecondsAtTick)
            {
                ++tally.InvariantViolations;
                return;
            }
        }

        for (size_t index = 1; index < sequence.Events.size(); ++index)
        {
            if (sequence.Events[index].Tick < sequence.Events[index - 1].Tick)
            {
                ++tally.InvariantViolations;
                return;
            }
        }
    }

    void ReadCorpusFolder(std::wstring const& folder, CorpusTally& tally, int limit) noexcept
    {
        WIN32_FIND_DATAW data{};

        auto const pattern = folder + L"\\*";
        HANDLE const find = ::FindFirstFileW(pattern.c_str(), &data);

        if (find == INVALID_HANDLE_VALUE)
        {
            return;
        }

        do
        {
            if (limit > 0 && tally.Files >= limit)
            {
                break;
            }

            std::wstring const name{ data.cFileName };

            if (name == L"." || name == L"..")
            {
                continue;
            }

            auto const full = folder + L"\\" + name;

            if ((data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0)
            {
                // A reparse point could point anywhere, including back at this folder.
                if ((data.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT) == 0)
                {
                    ReadCorpusFolder(full, tally, limit);
                }

                continue;
            }

            if (!IsStandardMidiFileExtension(full))
            {
                continue;
            }

            auto const bytes = (static_cast<int64_t>(data.nFileSizeHigh) << 32) | data.nFileSizeLow;

            LARGE_INTEGER frequency{};
            LARGE_INTEGER start{};
            LARGE_INTEGER finish{};

            ::QueryPerformanceFrequency(&frequency);
            ::QueryPerformanceCounter(&start);

            MidiSequence sequence{};
            auto const result = ReadStandardMidiFile(full, sequence);

            ::QueryPerformanceCounter(&finish);

            auto const milliseconds = frequency.QuadPart == 0
                ? 0.0
                : (static_cast<double>(finish.QuadPart - start.QuadPart) * 1000.0)
                    / static_cast<double>(frequency.QuadPart);

            ++tally.Files;
            tally.TotalBytes += bytes;
            ++tally.StatusCounts[static_cast<int>(result.Status)];

            if (bytes > tally.LargestBytes)
            {
                tally.LargestBytes = bytes;
                tally.LargestFile = full;
            }

            if (milliseconds > tally.SlowestMilliseconds)
            {
                tally.SlowestMilliseconds = milliseconds;
                tally.SlowestFile = full;
            }

            if (!result.Succeeded())
            {
                continue;
            }

            ++tally.Playable;

            if (result.Truncated)
            {
                ++tally.Truncated;
            }

            tally.TotalEvents += static_cast<int64_t>(sequence.Events.size());
            tally.TotalNotes += static_cast<int64_t>(sequence.Notes.size());

            int64_t chords = 0;

            for (auto const& text : sequence.TextEvents)
            {
                if (text.Kind != TextKind::ChordSymbol)
                {
                    continue;
                }

                ++chords;

                if (tally.ChordExamples.size() < 24)
                {
                    std::wstring wide{};

                    for (auto const character : text.Text)
                    {
                        wide.push_back(static_cast<wchar_t>(static_cast<unsigned char>(character)));
                    }

                    tally.ChordExamples.push_back(wide);
                }
            }

            if (chords > 0)
            {
                ++tally.FilesWithChords;
                tally.TotalChordSymbols += chords;
            }

            if (static_cast<int64_t>(sequence.Events.size()) > tally.MostEvents)
            {
                tally.MostEvents = static_cast<int64_t>(sequence.Events.size());
                tally.MostEventsFile = full;
            }

            auto const seconds = static_cast<double>(sequence.DurationMicroseconds) / 1000000.0;

            if (seconds > 600.0)
            {
                ++tally.OverTenMinutes;
            }

            if (seconds > 3600.0)
            {
                ++tally.OverOneHour;

                if (tally.LongExamples.size() < 10)
                {
                    tally.LongExamples.push_back(full);
                }
            }

            if (seconds > tally.LongestSeconds)
            {
                tally.LongestSeconds = seconds;
                tally.LongestFile = full;
            }

            CheckInvariants(sequence, tally);
        } while (::FindNextFileW(find, &data) != 0);

        ::FindClose(find);
    }
}

void SmfReaderTests::ReadsTheSampleFilesShippedWithWindows()
{
    wchar_t windowsFolder[MAX_PATH]{};

    if (::GetWindowsDirectoryW(windowsFolder, MAX_PATH) == 0)
    {
        Log::Result(TestResults::Skipped, L"Could not resolve the Windows folder.");
        return;
    }

    std::wstring const media = std::wstring{ windowsFolder } + L"\\Media\\";

    wchar_t const* const names[]{ L"onestop.mid", L"flourish.mid", L"town.mid" };

    int found = 0;

    for (auto const name : names)
    {
        auto const path = media + name;

        if (::GetFileAttributesW(path.c_str()) == INVALID_FILE_ATTRIBUTES)
        {
            continue;
        }

        ++found;
        ReportFile(path);
    }

    if (found == 0)
    {
        Log::Result(TestResults::Skipped, L"No sample MIDI files are installed on this machine.");
    }
}

void SmfReaderTests::ReadsACorpusOfRealFiles()
{
    wchar_t root[1024]{};

    if (::GetEnvironmentVariableW(L"MIDI_PLAYER_TEST_CORPUS", root, ARRAYSIZE(root)) == 0)
    {
        Log::Result(TestResults::Skipped, L"Set MIDI_PLAYER_TEST_CORPUS to a folder of MIDI files to run this.");
        return;
    }

    int limit = 0;

    wchar_t limitText[32]{};

    if (::GetEnvironmentVariableW(L"MIDI_PLAYER_TEST_CORPUS_LIMIT", limitText, ARRAYSIZE(limitText)) != 0)
    {
        limit = ::_wtoi(limitText);
    }

    CorpusTally tally{};

    auto const started = ::GetTickCount64();

    ReadCorpusFolder(std::wstring{ root }, tally, limit);

    auto const elapsedSeconds = static_cast<double>(::GetTickCount64() - started) / 1000.0;

    Log::Comment(String().Format(
        L"Read %d files (%.1f MB) in %.1f s. Playable %d, truncated %d, invariant violations %d.",
        tally.Files,
        static_cast<double>(tally.TotalBytes) / (1024.0 * 1024.0),
        elapsedSeconds,
        tally.Playable,
        tally.Truncated,
        tally.InvariantViolations));

    Log::Comment(String().Format(
        L"Totals: %lld events, %lld notes.", tally.TotalEvents, tally.TotalNotes));

    Log::Comment(String().Format(
        L"Suspicious durations: over 10 min %d (%.3f%%), over 1 hour %d (%.3f%%)",
        tally.OverTenMinutes,
        tally.Playable == 0 ? 0.0 : (100.0 * tally.OverTenMinutes / tally.Playable),
        tally.OverOneHour,
        tally.Playable == 0 ? 0.0 : (100.0 * tally.OverOneHour / tally.Playable)));

    for (auto const& example : tally.LongExamples)
    {
        Log::Comment(String().Format(L"  over an hour: %s", example.c_str()));
    }

    Log::Comment(String().Format(
        L"Chord symbols: %lld in %d files (%.3f%% of playable files)",
        tally.TotalChordSymbols,
        tally.FilesWithChords,
        tally.Playable == 0 ? 0.0 : (100.0 * tally.FilesWithChords / tally.Playable)));

    std::wstring chordSample{};

    for (auto const& example : tally.ChordExamples)
    {
        chordSample += example;
        chordSample += L" ";
    }

    Log::Comment(String().Format(L"  first symbols seen: %s", chordSample.c_str()));

    for (auto const& [status, count] : tally.StatusCounts)
    {
        Log::Comment(String().Format(
            L"  status %d: %d files (%.2f%%)",
            status,
            count,
            tally.Files == 0 ? 0.0 : (100.0 * count / tally.Files)));
    }

    Log::Comment(String().Format(L"Largest:     %.2f MB  %s",
        static_cast<double>(tally.LargestBytes) / (1024.0 * 1024.0), tally.LargestFile.c_str()));
    Log::Comment(String().Format(L"Most events: %lld  %s", tally.MostEvents, tally.MostEventsFile.c_str()));
    Log::Comment(String().Format(L"Longest:     %.1f s  %s", tally.LongestSeconds, tally.LongestFile.c_str()));
    Log::Comment(String().Format(L"Slowest:     %.1f ms  %s", tally.SlowestMilliseconds, tally.SlowestFile.c_str()));

    VERIFY_IS_GREATER_THAN(tally.Files, 0);

    // The point of the run. Anything the reader hands back has to be internally consistent, no
    // matter what was in the file.
    VERIFY_ARE_EQUAL(0, tally.InvariantViolations);
}

void SmfReaderTests::ReadsOneNamedFile()
{
    wchar_t path[1024]{};

    if (::GetEnvironmentVariableW(L"MIDI_PLAYER_TEST_FILE", path, ARRAYSIZE(path)) == 0)
    {
        Log::Result(TestResults::Skipped, L"Set MIDI_PLAYER_TEST_FILE to look at one file in detail.");
        return;
    }

    ReportFileInDetail(std::wstring{ path });
}
