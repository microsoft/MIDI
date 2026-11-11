// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#include "stdafx.h"

using namespace WEX::Common;
using namespace WEX::Logging;
using namespace WEX::TestExecution;

using namespace SequencingTests;

void MidiSequenceReaderTests::ReadsASimpleFile()
{
    auto const result = ReadTestFile(L"plain-scale.mid");

    VERIFY_IS_TRUE(result.Succeeded());
    VERIFY_IS_TRUE(result.Status() == MidiFileReadStatus::Success);
    VERIFY_IS_FALSE(result.Truncated());

    auto const sequence = result.Sequence();

    VERIFY_IS_NOT_NULL(sequence);
    VERIFY_IS_TRUE(sequence.Format() == MidiSequenceFormat::SingleTrack);
    VERIFY_ARE_EQUAL(uint16_t{ 480 }, sequence.TicksPerQuarterNote());
    VERIFY_IS_FALSE(sequence.UsesSmpteTiming());

    VERIFY_ARE_EQUAL(uint32_t{ 16 }, sequence.EventCount());
    VERIFY_ARE_EQUAL(uint32_t{ 8 }, sequence.NoteCount());
    VERIFY_ARE_EQUAL(uint64_t{ 2000000 }, sequence.DurationMicroseconds());

    VERIFY_ARE_EQUAL(uint8_t{ 60 }, sequence.LowestNoteNumber());
    VERIFY_ARE_EQUAL(uint8_t{ 67 }, sequence.HighestNoteNumber());

    VERIFY_ARE_EQUAL(uint32_t{ 1 }, sequence.Tracks().Size());
    VERIFY_ARE_EQUAL(String(L"Scale"), String(sequence.Tracks().GetAt(0).Name().c_str()));
}

void MidiSequenceReaderTests::ReadsAMultiTrackFile()
{
    auto const sequence = ReadTestSequence(L"multi-track.mid");

    VERIFY_IS_TRUE(sequence.Format() == MidiSequenceFormat::MultiTrack);
    VERIFY_ARE_EQUAL(uint32_t{ 4 }, sequence.Tracks().Size());
    VERIFY_ARE_EQUAL(uint32_t{ 18 }, sequence.NoteCount());

    // The conductor track carries the tempo and no notes, which is why a display should not offer
    // to mute it.
    VERIFY_ARE_EQUAL(uint32_t{ 0 }, sequence.Tracks().GetAt(0).NoteCount());
    VERIFY_ARE_EQUAL(String(L"Conductor"), String(sequence.Tracks().GetAt(0).Name().c_str()));

    for (uint32_t index = 1; index < 4; ++index)
    {
        auto const track = sequence.Tracks().GetAt(index);

        VERIFY_ARE_EQUAL(uint32_t{ 6 }, track.NoteCount());
        VERIFY_ARE_EQUAL(static_cast<uint16_t>(1u << (index - 1)), track.UsedChannelMask());
        VERIFY_ARE_EQUAL(String(L"Acoustic Grand Piano"), String(track.InstrumentName().c_str()));
    }
}

void MidiSequenceReaderTests::ReportsTheTempoMap()
{
    auto const sequence = ReadTestSequence(L"multi-track.mid");

    auto const map = sequence.TempoMap();

    VERIFY_ARE_EQUAL(uint32_t{ 2 }, map.Size());

    VERIFY_ARE_EQUAL(uint32_t{ 0 }, map.GetAt(0).Tick);
    VERIFY_ARE_EQUAL(uint32_t{ 500000 }, map.GetAt(0).MicrosecondsPerQuarterNote);
    VERIFY_ARE_EQUAL(120.0, map.GetAt(0).BeatsPerMinute);

    VERIFY_ARE_EQUAL(uint32_t{ 1920 }, map.GetAt(1).Tick);
    VERIFY_ARE_EQUAL(uint32_t{ 300000 }, map.GetAt(1).MicrosecondsPerQuarterNote);
    VERIFY_ARE_EQUAL(200.0, map.GetAt(1).BeatsPerMinute);

    // Tempo is a map, not a property of the file. Asking at a tick is the only correct question.
    VERIFY_ARE_EQUAL(120.0, sequence.GetBeatsPerMinuteAtTick(0));
    VERIFY_ARE_EQUAL(120.0, sequence.GetBeatsPerMinuteAtTick(1919));
    VERIFY_ARE_EQUAL(200.0, sequence.GetBeatsPerMinuteAtTick(1920));
}

void MidiSequenceReaderTests::ReportsTheTimeSignatureMapWithRealDenominators()
{
    auto const sequence = ReadTestSequence(L"meter-changes.mid");

    auto const map = sequence.TimeSignatureMap();

    VERIFY_ARE_EQUAL(uint32_t{ 3 }, map.Size());

    // The file stores the denominator as a power of two; the API reports what a score shows.
    VERIFY_ARE_EQUAL(uint8_t{ 4 }, map.GetAt(0).Numerator);
    VERIFY_ARE_EQUAL(uint8_t{ 4 }, map.GetAt(0).Denominator);
    VERIFY_ARE_EQUAL(uint32_t{ 1920 }, map.GetAt(0).TicksPerBar);

    VERIFY_ARE_EQUAL(uint8_t{ 3 }, map.GetAt(1).Numerator);
    VERIFY_ARE_EQUAL(uint8_t{ 4 }, map.GetAt(1).Denominator);
    VERIFY_ARE_EQUAL(uint32_t{ 1440 }, map.GetAt(1).TicksPerBar);

    VERIFY_ARE_EQUAL(uint8_t{ 7 }, map.GetAt(2).Numerator);
    VERIFY_ARE_EQUAL(uint8_t{ 8 }, map.GetAt(2).Denominator);
    VERIFY_ARE_EQUAL(uint32_t{ 1680 }, map.GetAt(2).TicksPerBar);
}

void MidiSequenceReaderTests::ConvertsBetweenTicksAndMicroseconds()
{
    auto const sequence = ReadTestSequence(L"plain-scale.mid");

    // 480 ticks to the quarter at 120 beats per minute is half a second per quarter.
    VERIFY_ARE_EQUAL(uint64_t{ 0 }, sequence.ConvertTickToMicroseconds(0));
    VERIFY_ARE_EQUAL(uint64_t{ 500000 }, sequence.ConvertTickToMicroseconds(480));
    VERIFY_ARE_EQUAL(uint64_t{ 2000000 }, sequence.ConvertTickToMicroseconds(1920));

    VERIFY_ARE_EQUAL(uint32_t{ 0 }, sequence.ConvertMicrosecondsToTick(0));
    VERIFY_ARE_EQUAL(uint32_t{ 480 }, sequence.ConvertMicrosecondsToTick(500000));
    VERIFY_ARE_EQUAL(uint32_t{ 1920 }, sequence.ConvertMicrosecondsToTick(2000000));

    // Round tripping every quarter note has to land back where it started.
    for (uint32_t tick = 0; tick <= 1920; tick += 120)
    {
        VERIFY_ARE_EQUAL(tick, sequence.ConvertMicrosecondsToTick(sequence.ConvertTickToMicroseconds(tick)));
    }
}

void MidiSequenceReaderTests::ReportsBarAndBeatAcrossMeterChanges()
{
    auto const sequence = ReadTestSequence(L"meter-changes.mid");

    auto first = sequence.GetBarPositionAtTick(0);
    VERIFY_ARE_EQUAL(uint32_t{ 1 }, first.Bar);
    VERIFY_ARE_EQUAL(uint32_t{ 1 }, first.Beat);

    // Second beat of the first 4/4 bar.
    auto second = sequence.GetBarPositionAtTick(480);
    VERIFY_ARE_EQUAL(uint32_t{ 1 }, second.Bar);
    VERIFY_ARE_EQUAL(uint32_t{ 2 }, second.Beat);

    // The 3/4 section starts a new bar.
    auto third = sequence.GetBarPositionAtTick(1920);
    VERIFY_ARE_EQUAL(uint32_t{ 2 }, third.Bar);
    VERIFY_ARE_EQUAL(uint32_t{ 1 }, third.Beat);
}

void MidiSequenceReaderTests::BuildsLyricLinesWhenAFileHasThem()
{
    auto const sequence = ReadTestSequence(L"with-lyrics.mid");

    auto const lines = sequence.LyricLines();

    VERIFY_ARE_EQUAL(uint32_t{ 2 }, lines.Size());
    VERIFY_ARE_EQUAL(String(L"Row row row your boat"), String(lines.GetAt(0).Text().c_str()));
    VERIFY_ARE_EQUAL(String(L"Gently down the stream"), String(lines.GetAt(1).Text().c_str()));

    // The line in effect, not the one starting at that instant.
    auto const early = sequence.GetLyricLineAtTick(lines.GetAt(0).StartTick());
    VERIFY_IS_NOT_NULL(early);
    VERIFY_ARE_EQUAL(String(L"Row row row your boat"), String(early.Text().c_str()));

    auto const later = sequence.GetLyricLineAtTick(lines.GetAt(1).StartTick() + 10);
    VERIFY_IS_NOT_NULL(later);
    VERIFY_ARE_EQUAL(String(L"Gently down the stream"), String(later.Text().c_str()));
}

void MidiSequenceReaderTests::OffersNoLyricLinesWhenAFileHasNone()
{
    auto const sequence = ReadTestSequence(L"without-lyrics.mid");

    VERIFY_ARE_EQUAL(uint32_t{ 0 }, sequence.LyricLines().Size());
    VERIFY_IS_NULL(sequence.GetLyricLineAtTick(0));
    VERIFY_IS_NULL(sequence.GetLyricLineAtTick(100000));
    VERIFY_IS_FALSE(sequence.IsKaraoke());
}

void MidiSequenceReaderTests::ReadsSoftKaraokeWordsFromTextEvents()
{
    auto const sequence = ReadTestSequence(L"soft-karaoke.mid");

    // A karaoke file stores its words in plain text events rather than lyric events, and says so
    // with an @K tag before the first one.
    VERIFY_IS_TRUE(sequence.IsKaraoke());

    auto const lines = sequence.LyricLines();

    VERIFY_ARE_EQUAL(uint32_t{ 2 }, lines.Size());
    VERIFY_ARE_EQUAL(String(L"Twinkle twinkle"), String(lines.GetAt(0).Text().c_str()));
    VERIFY_ARE_EQUAL(String(L"little star"), String(lines.GetAt(1).Text().c_str()));
}

void MidiSequenceReaderTests::FindsChordSymbolsInEffect()
{
    auto const sequence = ReadTestSequence(L"with-chords.mid");

    // Before the first chord there is nothing to show.
    VERIFY_IS_NOT_NULL(sequence.GetChordSymbolAtTick(0));

    VERIFY_ARE_EQUAL(String(L"C"), String(sequence.GetChordSymbolAtTick(0).Text().c_str()));
    VERIFY_ARE_EQUAL(String(L"C"), String(sequence.GetChordSymbolAtTick(479).Text().c_str()));
    VERIFY_ARE_EQUAL(String(L"Am7"), String(sequence.GetChordSymbolAtTick(480).Text().c_str()));
    VERIFY_ARE_EQUAL(String(L"F"), String(sequence.GetChordSymbolAtTick(960).Text().c_str()));
    VERIFY_ARE_EQUAL(String(L"G7"), String(sequence.GetChordSymbolAtTick(1440).Text().c_str()));

    // It stays in effect past the end of the music.
    VERIFY_ARE_EQUAL(String(L"G7"), String(sequence.GetChordSymbolAtTick(1000000).Text().c_str()));
}

void MidiSequenceReaderTests::OffersNoChordsWhenAFileHasNone()
{
    auto const sequence = ReadTestSequence(L"without-chords.mid");

    VERIFY_IS_NULL(sequence.GetChordSymbolAtTick(0));
    VERIFY_IS_NULL(sequence.GetChordSymbolAtTick(960));
}

void MidiSequenceReaderTests::CountsAndFillsNotesInAWindow()
{
    auto const sequence = ReadTestSequence(L"plain-scale.mid");

    // Every note, asked for in one call.
    auto const total = sequence.GetNoteCountInTickRange(0, sequence.LastTick());
    VERIFY_ARE_EQUAL(uint32_t{ 8 }, total);

    std::vector<MidiSequenceNote> notes(total);

    auto const written = sequence.FillNotesInTickRange(0, sequence.LastTick(), 0, notes);

    VERIFY_ARE_EQUAL(total, written);

    // In start order, which is what a display depends on.
    for (uint32_t index = 1; index < written; ++index)
    {
        VERIFY_IS_TRUE(notes[index].StartTick >= notes[index - 1].StartTick);
    }

    VERIFY_ARE_EQUAL(uint8_t{ 60 }, notes[0].NoteNumber);
    VERIFY_ARE_EQUAL(uint8_t{ 100 }, notes[0].Velocity);
    VERIFY_ARE_EQUAL(uint32_t{ 0 }, notes[0].StartTick);
    VERIFY_ARE_EQUAL(uint32_t{ 240 }, notes[0].EndTick);

    // A window covering only the first two notes returns only those.
    VERIFY_ARE_EQUAL(uint32_t{ 2 }, sequence.GetNoteCountInTickRange(0, 300));
}

void MidiSequenceReaderTests::FillRespectsTheCallersArraySize()
{
    auto const sequence = ReadTestSequence(L"plain-scale.mid");

    // Deliberately smaller than the window holds. The call must stop, not overrun.
    std::vector<MidiSequenceNote> notes(3);

    auto const written = sequence.FillNotesInTickRange(0, sequence.LastTick(), 0, notes);

    VERIFY_ARE_EQUAL(uint32_t{ 3 }, written);

    // An empty array is legal and writes nothing.
    std::vector<MidiSequenceNote> none{};
    VERIFY_ARE_EQUAL(uint32_t{ 0 }, sequence.FillNotesInTickRange(0, sequence.LastTick(), 0, none));
}

void MidiSequenceReaderTests::FillHonorsTheStartIndex()
{
    auto const sequence = ReadTestSequence(L"plain-scale.mid");

    std::vector<MidiSequenceNote> notes(10);

    // Poison the whole array so anything written before the start index would show.
    for (auto& note : notes)
    {
        note.NoteNumber = 0xFF;
    }

    auto const written = sequence.FillNotesInTickRange(0, sequence.LastTick(), 4, notes);

    VERIFY_ARE_EQUAL(uint32_t{ 6 }, written);

    for (size_t index = 0; index < 4; ++index)
    {
        VERIFY_ARE_EQUAL(uint8_t{ 0xFF }, notes[index].NoteNumber);
    }

    VERIFY_ARE_EQUAL(uint8_t{ 60 }, notes[4].NoteNumber);

    // A start index past the end writes nothing rather than misbehaving.
    VERIFY_ARE_EQUAL(uint32_t{ 0 }, sequence.FillNotesInTickRange(0, sequence.LastTick(), 100, notes));
}

void MidiSequenceReaderTests::CountsSoundingNotesPerTrack()
{
    auto const sequence = ReadTestSequence(L"multi-track.mid");

    std::vector<uint8_t> counts(sequence.Tracks().Size());

    auto const written = sequence.FillSoundingNoteCountsAtTick(0, counts);

    VERIFY_ARE_EQUAL(sequence.Tracks().Size(), written);

    // The conductor track never sounds anything.
    VERIFY_ARE_EQUAL(uint8_t{ 0 }, counts[0]);

    // The three parts all start together.
    VERIFY_ARE_EQUAL(uint8_t{ 1 }, counts[1]);
    VERIFY_ARE_EQUAL(uint8_t{ 1 }, counts[2]);
    VERIFY_ARE_EQUAL(uint8_t{ 1 }, counts[3]);

    // A smaller array than there are tracks is filled as far as it goes.
    std::vector<uint8_t> shortArray(2);
    VERIFY_ARE_EQUAL(uint32_t{ 2 }, sequence.FillSoundingNoteCountsAtTick(0, shortArray));
}

void MidiSequenceReaderTests::ReturnsNothingForAnInvertedWindow()
{
    auto const sequence = ReadTestSequence(L"plain-scale.mid");

    std::vector<MidiSequenceNote> notes(8);

    VERIFY_ARE_EQUAL(uint32_t{ 0 }, sequence.GetNoteCountInTickRange(900, 100));
    VERIFY_ARE_EQUAL(uint32_t{ 0 }, sequence.FillNotesInTickRange(900, 100, 0, notes));
}

void MidiSequenceReaderTests::ReadsSmpteTimedFiles()
{
    auto const sequence = ReadTestSequence(L"smpte-timing.mid");

    // Ticks are absolute frames here, so tempo does not apply and the duration comes from the
    // frame rate instead.
    VERIFY_IS_TRUE(sequence.UsesSmpteTiming());
    VERIFY_ARE_EQUAL(uint32_t{ 4 }, sequence.NoteCount());
    VERIFY_ARE_EQUAL(uint64_t{ 100000 }, sequence.DurationMicroseconds());
}

void MidiSequenceReaderTests::ReadsRunningStatus()
{
    auto const sequence = ReadTestSequence(L"running-status.mid");

    // Seven of the eight note ons omit their status byte entirely.
    VERIFY_ARE_EQUAL(uint32_t{ 8 }, sequence.NoteCount());
}

void MidiSequenceReaderTests::ReadsSystemMessagesWithoutLosingItsPlace()
{
    auto const sequence = ReadTestSequence(L"system-messages.mid");

    // Clock, active sensing, quarter frame, song select, song position and tune request all sit
    // between the note on and the note off. Getting any of their lengths wrong swallows the note
    // off and the note would never end.
    VERIFY_ARE_EQUAL(uint32_t{ 1 }, sequence.NoteCount());

    std::vector<MidiSequenceNote> notes(1);
    VERIFY_ARE_EQUAL(uint32_t{ 1 }, sequence.FillNotesInTickRange(0, sequence.LastTick(), 0, notes));

    VERIFY_ARE_EQUAL(uint32_t{ 0 }, notes[0].StartTick);
    VERIFY_ARE_EQUAL(uint32_t{ 300 }, notes[0].EndTick);
}

void MidiSequenceReaderTests::ReadsALongSystemExclusive()
{
    auto const sequence = ReadTestSequence(L"long-sysex.mid");

    // Two thousand bytes of payload, which becomes many packets on the wire, plus one note.
    VERIFY_ARE_EQUAL(uint32_t{ 1 }, sequence.NoteCount());
    VERIFY_IS_TRUE(sequence.EventCount() >= 3);
}

void MidiSequenceReaderTests::RefusesSomethingThatIsNotAMidiFile()
{
    auto const result = ReadTestFile(L"malformed\\not-a-midi-file.mid");

    VERIFY_IS_FALSE(result.Succeeded());
    VERIFY_IS_TRUE(result.Status() == MidiFileReadStatus::NotAMidiFile);
    VERIFY_IS_NULL(result.Sequence());
}

void MidiSequenceReaderTests::RefusesAFileWithNothingToPlay()
{
    auto const result = ReadTestFile(L"empty-track.mid");

    VERIFY_IS_FALSE(result.Succeeded());
    VERIFY_IS_TRUE(result.Status() == MidiFileReadStatus::NoPlayableData);
}

void MidiSequenceReaderTests::SalvagesATruncatedFile()
{
    auto const result = ReadTestFile(L"malformed\\truncated.mid");

    // A file with a damaged tail is still worth hearing, so what was read is kept and the caller
    // is told.
    VERIFY_IS_TRUE(result.Truncated());

    if (result.Succeeded())
    {
        VERIFY_IS_NOT_NULL(result.Sequence());
        VERIFY_IS_TRUE(result.Sequence().NoteCount() > 0);
    }
}

void MidiSequenceReaderTests::SurvivesALyingTrackLength()
{
    // The track chunk claims about two billion bytes in a file of thirty one. Nothing may be
    // allocated from that number.
    auto const result = ReadTestFile(L"malformed\\lying-track-length.mid");

    VERIFY_IS_TRUE(result != nullptr);
    VERIFY_IS_TRUE(result.Truncated() || result.Succeeded());
}

void MidiSequenceReaderTests::SurvivesALyingTrackCount()
{
    // The header claims 65535 tracks and the file ends immediately.
    auto const result = ReadTestFile(L"malformed\\lying-track-count.mid");

    VERIFY_IS_TRUE(result != nullptr);
    VERIFY_IS_FALSE(result.Succeeded());
    VERIFY_IS_TRUE(result.DeclaredTrackCount() > result.ReadTrackCount());
}

void MidiSequenceReaderTests::SurvivesAnAbsurdDeltaTime()
{
    // A delta time of about 268 million ticks is not music. The reader should stop rather than
    // report a file lasting weeks.
    auto const result = ReadTestFile(L"malformed\\absurd-delta.mid");

    VERIFY_IS_TRUE(result != nullptr);

    if (result.Succeeded())
    {
        // An hour is already far beyond anything this fixture contains.
        VERIFY_IS_TRUE(result.Sequence().DurationMicroseconds() < 3600ull * 1000000ull);
    }
}

void MidiSequenceReaderTests::RefusesAFileLargerThanTheLimit()
{
    MidiFileReadOptions options{};

    options.MaximumFileBytes(16);

    auto const path = TestFilePath(L"plain-scale.mid");
    auto file = storage::StorageFile::GetFileFromPathAsync(winrt::hstring{ path }).get();

    auto const result = MidiStandardFileReader::ReadFromFileAsync(file, options).get();

    VERIFY_IS_FALSE(result.Succeeded());
    VERIFY_IS_TRUE(result.Status() == MidiFileReadStatus::TooLarge);
}

void MidiSequenceReaderTests::HonorsTheEventCountLimit()
{
    MidiFileReadOptions options{};

    options.MaximumEventCount(50);

    auto const path = TestFilePath(L"black-midi.mid");
    auto file = storage::StorageFile::GetFileFromPathAsync(winrt::hstring{ path }).get();

    auto const result = MidiStandardFileReader::ReadFromFileAsync(file, options).get();

    VERIFY_IS_TRUE(result != nullptr);

    if (result.Succeeded())
    {
        VERIFY_IS_TRUE(result.Sequence().EventCount() <= 50);
        VERIFY_IS_TRUE(result.Truncated());
    }
}

void MidiSequenceReaderTests::FailsOnUnreadableDataWhenAsked()
{
    MidiFileReadOptions options{};

    options.FailOnUnreadableData(true);

    auto const path = TestFilePath(L"malformed\\truncated.mid");
    auto file = storage::StorageFile::GetFileFromPathAsync(winrt::hstring{ path }).get();

    auto const result = MidiStandardFileReader::ReadFromFileAsync(file, options).get();

    VERIFY_IS_FALSE(result.Succeeded());
}

void MidiSequenceReaderTests::ReadsFromAStreamAsWellAsAFile()
{
    auto const bytes = ReadTestFileBytes(L"plain-scale.mid");

    VERIFY_IS_TRUE(bytes.size() > 0);

    auto const result = ReadBytes(bytes);

    VERIFY_IS_TRUE(result.Succeeded());
    VERIFY_ARE_EQUAL(uint32_t{ 8 }, result.Sequence().NoteCount());

    // An empty stream is not a file.
    auto const empty = ReadBytes(std::vector<uint8_t>{});
    VERIFY_IS_FALSE(empty.Succeeded());
}
