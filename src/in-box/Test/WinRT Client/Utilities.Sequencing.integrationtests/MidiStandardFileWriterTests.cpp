// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#include "stdafx.h"
#include "MidiStandardFileWriterTests.h"

using namespace winrt::Windows::Devices::Midi2;
using namespace winrt::Windows::Devices::Midi2::Utilities::Sequencing;
using namespace winrt::Windows::Devices::Midi2::Utilities::Files;

namespace
{
    using SequencingTests::ReadTestSequence;
    using SequencingTests::ReadBytes;
    using SequencingTests::WriteSequenceBytes;

    // Writes the sequence, reads the result back and hands over the reloaded sequence. This is
    // the whole point of the writer, so nearly every test starts here.
    MidiSequence RoundTrip(
        _In_ MidiSequence const& sequence,
        _In_ MidiFileWriteOptions const& options = nullptr)
    {
        auto const written = WriteSequenceBytes(sequence, options);

        VERIFY_IS_NOT_NULL(written.Result);
        VERIFY_IS_TRUE(written.Result.Succeeded());
        VERIFY_IS_TRUE(written.Bytes.size() > 14);

        auto const read = ReadBytes(written.Bytes);

        VERIFY_IS_NOT_NULL(read);
        VERIFY_IS_TRUE(read.Succeeded());

        return read.Sequence();
    }

    void VerifySameSequence(_In_ MidiSequence const& before, _In_ MidiSequence const& after)
    {
        VERIFY_ARE_EQUAL(before.EventCount(), after.EventCount());
        VERIFY_ARE_EQUAL(before.NoteCount(), after.NoteCount());
        VERIFY_ARE_EQUAL(before.LastTick(), after.LastTick());
        VERIFY_ARE_EQUAL(before.DurationMicroseconds(), after.DurationMicroseconds());
        VERIFY_ARE_EQUAL(before.UsedChannelMask(), after.UsedChannelMask());
        VERIFY_ARE_EQUAL(before.LowestNoteNumber(), after.LowestNoteNumber());
        VERIFY_ARE_EQUAL(before.HighestNoteNumber(), after.HighestNoteNumber());
        VERIFY_ARE_EQUAL(before.TicksPerQuarterNote(), after.TicksPerQuarterNote());
        VERIFY_ARE_EQUAL(before.UsesSmpteTiming(), after.UsesSmpteTiming());
        VERIFY_ARE_EQUAL(before.IsKaraoke(), after.IsKaraoke());
        VERIFY_ARE_EQUAL(before.Title(), after.Title());
        VERIFY_ARE_EQUAL(before.Copyright(), after.Copyright());

        VERIFY_ARE_EQUAL(before.Tracks().Size(), after.Tracks().Size());
        VERIFY_ARE_EQUAL(before.TempoMap().Size(), after.TempoMap().Size());
        VERIFY_ARE_EQUAL(before.TimeSignatureMap().Size(), after.TimeSignatureMap().Size());
        VERIFY_ARE_EQUAL(before.TextEvents().Size(), after.TextEvents().Size());
        VERIFY_ARE_EQUAL(before.LyricLines().Size(), after.LyricLines().Size());
    }

    std::vector<MidiSequenceNote> AllNotes(_In_ MidiSequence const& sequence)
    {
        std::vector<MidiSequenceNote> notes{};

        auto const count = sequence.NoteCount();

        if (count == 0)
        {
            return notes;
        }

        notes.resize(count);

        auto const filled = sequence.FillNotesInTickRange(
            0, 0xFFFFFFFF, 0, winrt::array_view<MidiSequenceNote>{ notes });

        notes.resize(filled);

        return notes;
    }

    void VerifySameNotes(_In_ MidiSequence const& before, _In_ MidiSequence const& after)
    {
        auto const left = AllNotes(before);
        auto const right = AllNotes(after);

        VERIFY_ARE_EQUAL(left.size(), right.size());

        for (size_t index = 0; index < left.size(); ++index)
        {
            VERIFY_ARE_EQUAL(left[index].StartTick, right[index].StartTick);
            VERIFY_ARE_EQUAL(left[index].EndTick, right[index].EndTick);
            VERIFY_ARE_EQUAL(left[index].TrackIndex, right[index].TrackIndex);
            VERIFY_ARE_EQUAL(left[index].ChannelIndex, right[index].ChannelIndex);
            VERIFY_ARE_EQUAL(left[index].NoteNumber, right[index].NoteNumber);
            VERIFY_ARE_EQUAL(left[index].Velocity, right[index].Velocity);
        }
    }

    MidiChannel ChannelOf(uint8_t index)
    {
        return MidiChannel{ static_cast<uint8_t>(index) };
    }

    // Every file the reader can read, so a single test covers the whole fixture set rather than
    // whichever ones somebody remembered to name.
    constexpr wchar_t const* ReadableTestFiles[]
    {
        L"plain-scale.mid",
        L"multi-track.mid",
        L"meter-changes.mid",
        L"with-lyrics.mid",
        L"without-lyrics.mid",
        L"with-chords.mid",
        L"without-chords.mid",
        L"soft-karaoke.mid",
        L"system-messages.mid",
        L"running-status.mid",
        L"smpte-timing.mid",
        L"hanging-notes.mid",
        L"long-sysex.mid"
    };
}

void MidiStandardFileWriterTests::RoundTripsEveryTestFile()
{
    for (auto const* const name : ReadableTestFiles)
    {
        LOG_OUTPUT(L"%s", name);

        auto const original = ReadTestSequence(name);
        auto const reloaded = RoundTrip(original);

        VerifySameSequence(original, reloaded);
        VerifySameNotes(original, reloaded);
    }
}

void MidiStandardFileWriterTests::RoundTripsNotesExactly()
{
    auto const original = ReadTestSequence(L"plain-scale.mid");

    VERIFY_IS_TRUE(original.NoteCount() > 0);

    VerifySameNotes(original, RoundTrip(original));
}

void MidiStandardFileWriterTests::RoundTripsTempoAndMeterChanges()
{
    auto const original = ReadTestSequence(L"meter-changes.mid");
    auto const reloaded = RoundTrip(original);

    auto const beforeTempo = original.TempoMap();
    auto const afterTempo = reloaded.TempoMap();

    VERIFY_IS_TRUE(beforeTempo.Size() > 0);
    VERIFY_ARE_EQUAL(beforeTempo.Size(), afterTempo.Size());

    for (uint32_t index = 0; index < beforeTempo.Size(); ++index)
    {
        VERIFY_ARE_EQUAL(beforeTempo.GetAt(index).Tick, afterTempo.GetAt(index).Tick);
        VERIFY_ARE_EQUAL(
            beforeTempo.GetAt(index).MicrosecondsPerQuarterNote,
            afterTempo.GetAt(index).MicrosecondsPerQuarterNote);
        VERIFY_ARE_EQUAL(
            beforeTempo.GetAt(index).MicrosecondsAtTick,
            afterTempo.GetAt(index).MicrosecondsAtTick);
    }

    auto const beforeMeter = original.TimeSignatureMap();
    auto const afterMeter = reloaded.TimeSignatureMap();

    VERIFY_IS_TRUE(beforeMeter.Size() > 1);
    VERIFY_ARE_EQUAL(beforeMeter.Size(), afterMeter.Size());

    for (uint32_t index = 0; index < beforeMeter.Size(); ++index)
    {
        VERIFY_ARE_EQUAL(beforeMeter.GetAt(index).Tick, afterMeter.GetAt(index).Tick);
        VERIFY_ARE_EQUAL(beforeMeter.GetAt(index).Numerator, afterMeter.GetAt(index).Numerator);
        VERIFY_ARE_EQUAL(beforeMeter.GetAt(index).Denominator, afterMeter.GetAt(index).Denominator);
        VERIFY_ARE_EQUAL(beforeMeter.GetAt(index).TicksPerBar, afterMeter.GetAt(index).TicksPerBar);
        VERIFY_ARE_EQUAL(beforeMeter.GetAt(index).BarNumberAtTick, afterMeter.GetAt(index).BarNumberAtTick);
    }
}

void MidiStandardFileWriterTests::RoundTripsTrackNamesAndInstruments()
{
    auto const original = ReadTestSequence(L"multi-track.mid");
    auto const reloaded = RoundTrip(original);

    auto const before = original.Tracks();
    auto const after = reloaded.Tracks();

    VERIFY_IS_TRUE(before.Size() > 1);
    VERIFY_ARE_EQUAL(before.Size(), after.Size());

    for (uint32_t index = 0; index < before.Size(); ++index)
    {
        VERIFY_ARE_EQUAL(before.GetAt(index).TrackIndex(), after.GetAt(index).TrackIndex());
        VERIFY_ARE_EQUAL(before.GetAt(index).Name(), after.GetAt(index).Name());
        VERIFY_ARE_EQUAL(before.GetAt(index).InstrumentName(), after.GetAt(index).InstrumentName());
        VERIFY_ARE_EQUAL(before.GetAt(index).SuggestedDeviceName(), after.GetAt(index).SuggestedDeviceName());
        VERIFY_ARE_EQUAL(before.GetAt(index).UsedChannelMask(), after.GetAt(index).UsedChannelMask());
        VERIFY_ARE_EQUAL(before.GetAt(index).NoteCount(), after.GetAt(index).NoteCount());
        VERIFY_ARE_EQUAL(before.GetAt(index).EventCount(), after.GetAt(index).EventCount());
        VERIFY_ARE_EQUAL(before.GetAt(index).LastTick(), after.GetAt(index).LastTick());
    }
}

void MidiStandardFileWriterTests::RoundTripsLyricsAndMarkers()
{
    auto const original = ReadTestSequence(L"with-lyrics.mid");
    auto const reloaded = RoundTrip(original);

    auto const before = original.TextEvents();
    auto const after = reloaded.TextEvents();

    VERIFY_IS_TRUE(before.Size() > 0);
    VERIFY_ARE_EQUAL(before.Size(), after.Size());

    for (uint32_t index = 0; index < before.Size(); ++index)
    {
        VERIFY_ARE_EQUAL(before.GetAt(index).Tick(), after.GetAt(index).Tick());
        VERIFY_ARE_EQUAL(before.GetAt(index).TrackIndex(), after.GetAt(index).TrackIndex());
        VERIFY_ARE_EQUAL(
            static_cast<int32_t>(before.GetAt(index).Kind()),
            static_cast<int32_t>(after.GetAt(index).Kind()));
        VERIFY_ARE_EQUAL(before.GetAt(index).Text(), after.GetAt(index).Text());
    }

    auto const beforeLines = original.LyricLines();
    auto const afterLines = reloaded.LyricLines();

    VERIFY_IS_TRUE(beforeLines.Size() > 0);
    VERIFY_ARE_EQUAL(beforeLines.Size(), afterLines.Size());

    for (uint32_t index = 0; index < beforeLines.Size(); ++index)
    {
        VERIFY_ARE_EQUAL(beforeLines.GetAt(index).StartTick(), afterLines.GetAt(index).StartTick());
        VERIFY_ARE_EQUAL(beforeLines.GetAt(index).Text(), afterLines.GetAt(index).Text());
    }
}

void MidiStandardFileWriterTests::RoundTripsChordSymbolsCarriedInSystemExclusive()
{
    // A chord symbol is not a meta event: it lives in a manufacturer system exclusive message.
    // Getting it back proves the dump itself survived, bytes and all.
    auto const original = ReadTestSequence(L"with-chords.mid");
    auto const reloaded = RoundTrip(original);

    auto const before = original.GetChordSymbolAtTick(original.LastTick());
    auto const after = reloaded.GetChordSymbolAtTick(reloaded.LastTick());

    VERIFY_IS_NOT_NULL(before);
    VERIFY_IS_NOT_NULL(after);
    VERIFY_ARE_EQUAL(before.Text(), after.Text());
    VERIFY_ARE_EQUAL(before.Tick(), after.Tick());

    VerifySameSequence(original, reloaded);
}

void MidiStandardFileWriterTests::RoundTripsSmpteTiming()
{
    auto const original = ReadTestSequence(L"smpte-timing.mid");

    VERIFY_IS_TRUE(original.UsesSmpteTiming());

    auto const reloaded = RoundTrip(original);

    VERIFY_IS_TRUE(reloaded.UsesSmpteTiming());

    VerifySameSequence(original, reloaded);
    VerifySameNotes(original, reloaded);
}

void MidiStandardFileWriterTests::WritingTwiceGivesTheSameBytes()
{
    // The writer has to have a fixed point, or a file would drift every time it was opened and
    // saved. Once through the reader, everything the reader repairs is already repaired.
    for (auto const* const name : ReadableTestFiles)
    {
        auto const original = ReadTestSequence(name);

        auto const first = WriteSequenceBytes(original);

        VERIFY_IS_TRUE(first.Result.Succeeded());

        auto const read = ReadBytes(first.Bytes);

        VERIFY_IS_TRUE(read.Succeeded());

        auto const second = WriteSequenceBytes(read.Sequence());

        VERIFY_IS_TRUE(second.Result.Succeeded());

        if (first.Bytes != second.Bytes)
        {
            LOG_OUTPUT(L"%s differed on the second write: %u then %u bytes",
                name, static_cast<uint32_t>(first.Bytes.size()), static_cast<uint32_t>(second.Bytes.size()));
        }

        VERIFY_IS_TRUE(first.Bytes == second.Bytes);
    }
}

void MidiStandardFileWriterTests::WriteSingleTrackMergesEverythingOntoOneTrack()
{
    auto const original = ReadTestSequence(L"multi-track.mid");

    VERIFY_IS_TRUE(original.Tracks().Size() > 1);

    MidiFileWriteOptions options{};

    options.WriteSingleTrack(true);

    auto const reloaded = RoundTrip(original, options);

    VERIFY_ARE_EQUAL(uint32_t{ 1 }, reloaded.Tracks().Size());
    VERIFY_ARE_EQUAL(
        static_cast<int32_t>(MidiSequenceFormat::SingleTrack),
        static_cast<int32_t>(reloaded.Format()));

    // The music is all still there, on one track instead of four.
    VERIFY_ARE_EQUAL(original.EventCount(), reloaded.EventCount());
    VERIFY_ARE_EQUAL(original.NoteCount(), reloaded.NoteCount());
    VERIFY_ARE_EQUAL(original.LastTick(), reloaded.LastTick());
    VERIFY_ARE_EQUAL(original.UsedChannelMask(), reloaded.UsedChannelMask());
}

void MidiStandardFileWriterTests::RunningStatusIsSmallerAndReadsTheSame()
{
    // A run of messages sharing one status byte, which is the only case running status can
    // shrink. A scale alternates note on and note off, so it would save nothing at all.
    MidiSequenceBuilder builder{};

    auto const track = builder.AddTrack(L"Controller sweep");

    for (uint32_t step = 0; step < 128; ++step)
    {
        std::array<uint32_t, 1> controlChange{ 0x20B00100u | step };

        builder.AddMessages(track, step * 10, controlChange);
    }

    auto const original = builder.GetSequence();

    MidiFileWriteOptions options{};

    options.UseRunningStatus(true);

    auto const plain = WriteSequenceBytes(original);
    auto const packed = WriteSequenceBytes(original, options);

    VERIFY_IS_TRUE(plain.Result.Succeeded());
    VERIFY_IS_TRUE(packed.Result.Succeeded());

    LOG_OUTPUT(L"plain %u bytes, running status %u bytes",
        static_cast<uint32_t>(plain.Bytes.size()), static_cast<uint32_t>(packed.Bytes.size()));

    // One status byte saved on every message after the first.
    VERIFY_ARE_EQUAL(plain.Bytes.size() - 127, packed.Bytes.size());

    auto const reloaded = RoundTrip(original, options);

    VERIFY_ARE_EQUAL(original.EventCount(), reloaded.EventCount());
    VERIFY_ARE_EQUAL(original.LastTick(), reloaded.LastTick());
}

void MidiStandardFileWriterTests::RefusesToGoOverTheMaximumFileBytes()
{
    auto const original = ReadTestSequence(L"plain-scale.mid");

    MidiFileWriteOptions options{};

    options.MaximumFileBytes(32);

    auto const written = WriteSequenceBytes(original, options);

    VERIFY_IS_NOT_NULL(written.Result);
    VERIFY_IS_FALSE(written.Result.Succeeded());
    VERIFY_ARE_EQUAL(
        static_cast<int32_t>(MidiFileWriteStatus::TooLarge),
        static_cast<int32_t>(written.Result.Status()));
}

void MidiStandardFileWriterTests::WritesASequenceBuiltInMemory()
{
    MidiSequenceBuilder builder{};

    builder.TicksPerQuarterNote(480);

    auto const track = builder.AddTrack(L"Built in memory");

    builder.AddTempoChange(0, 96.0);
    builder.AddTimeSignature(0, 3, 4);
    builder.AddNote(0, 240, track, ChannelOf(0), 60, 100);
    builder.AddNote(480, 240, track, ChannelOf(0), 64, 80);
    builder.AddNote(960, 480, track, ChannelOf(3), 67, 20);

    auto const built = builder.GetSequence();
    auto const reloaded = RoundTrip(built);

    VERIFY_ARE_EQUAL(uint16_t{ 480 }, reloaded.TicksPerQuarterNote());
    VERIFY_ARE_EQUAL(uint32_t{ 3 }, reloaded.NoteCount());
    VERIFY_ARE_EQUAL(winrt::hstring{ L"Built in memory" }, reloaded.Tracks().GetAt(0).Name());
    VERIFY_ARE_EQUAL(96.0, reloaded.GetBeatsPerMinuteAtTick(0));
    VERIFY_ARE_EQUAL(uint8_t{ 3 }, reloaded.TimeSignatureMap().GetAt(0).Numerator);
    VERIFY_ARE_EQUAL(uint8_t{ 4 }, reloaded.TimeSignatureMap().GetAt(0).Denominator);

    VerifySameNotes(built, reloaded);
}

void MidiStandardFileWriterTests::TranslatesMidi2NoteOnToMidi1()
{
    MidiSequenceBuilder builder{};

    auto const track = builder.AddTrack(L"MIDI 2.0");

    // Channel voice 64 bit, note on, channel 2, note 60, velocity 0xFF00 of 0xFFFF.
    std::array<uint32_t, 2> noteOn{ 0x40923C00, 0xFF000000 };
    std::array<uint32_t, 2> noteOff{ 0x40823C00, 0x00000000 };

    builder.AddMessages(track, 0, noteOn);
    builder.AddMessages(track, 480, noteOff);

    auto const reloaded = RoundTrip(builder.GetSequence());

    auto const notes = AllNotes(reloaded);

    VERIFY_ARE_EQUAL(size_t{ 1 }, notes.size());
    VERIFY_ARE_EQUAL(uint8_t{ 60 }, notes[0].NoteNumber);
    VERIFY_ARE_EQUAL(uint8_t{ 2 }, notes[0].ChannelIndex);
    VERIFY_ARE_EQUAL(uint32_t{ 0 }, notes[0].StartTick);
    VERIFY_ARE_EQUAL(uint32_t{ 480 }, notes[0].EndTick);

    // 0xFF00 scaled down to seven bits keeps the top bits, which is 0x7F.
    VERIFY_ARE_EQUAL(uint8_t{ 0x7F }, notes[0].Velocity);
}

void MidiStandardFileWriterTests::NeverTurnsAMidi2NoteOnIntoANoteOff()
{
    MidiSequenceBuilder builder{};

    auto const track = builder.AddTrack(L"Quiet");

    // A MIDI 2.0 note on really can carry a velocity of zero. MIDI 1.0 would read that as a note
    // off, so the note would vanish instead of sounding quietly.
    std::array<uint32_t, 2> noteOn{ 0x40903C00, 0x00000000 };
    std::array<uint32_t, 2> noteOff{ 0x40803C00, 0x00000000 };

    builder.AddMessages(track, 0, noteOn);
    builder.AddMessages(track, 240, noteOff);

    auto const notes = AllNotes(RoundTrip(builder.GetSequence()));

    VERIFY_ARE_EQUAL(size_t{ 1 }, notes.size());
    VERIFY_ARE_EQUAL(uint8_t{ 1 }, notes[0].Velocity);
    VERIFY_ARE_EQUAL(uint32_t{ 240 }, notes[0].EndTick);
}

void MidiStandardFileWriterTests::TranslatesMidi2ControlChangeToMidi1()
{
    MidiSequenceBuilder builder{};

    auto const track = builder.AddTrack(L"Controllers");

    // Control change, channel 0, controller 7, value 0x80000000 of 0xFFFFFFFF.
    std::array<uint32_t, 2> volume{ 0x40B00700, 0x80000000 };

    builder.AddMessages(track, 0, volume);
    builder.AddNote(0, 240, track, ChannelOf(0), 60, 100);

    auto const written = WriteSequenceBytes(builder.GetSequence());

    VERIFY_IS_TRUE(written.Result.Succeeded());
    VERIFY_ARE_EQUAL(uint32_t{ 0 }, written.Result.SkippedEventCount());

    auto const read = ReadBytes(written.Bytes);

    VERIFY_IS_TRUE(read.Succeeded());

    // Two note halves plus the controller.
    VERIFY_ARE_EQUAL(uint32_t{ 3 }, read.Sequence().EventCount());
}

void MidiStandardFileWriterTests::CountsMessagesMidi1CannotExpress()
{
    MidiSequenceBuilder builder{};

    auto const track = builder.AddTrack(L"Not expressible");

    // Registered per note controller, and a per note pitch bend. Neither has a MIDI 1.0 form.
    std::array<uint32_t, 2> perNoteController{ 0x40003C01, 0x40000000 };
    std::array<uint32_t, 2> perNotePitchBend{ 0x40603C00, 0x80000000 };

    builder.AddMessages(track, 0, perNoteController);
    builder.AddMessages(track, 0, perNotePitchBend);
    builder.AddNote(0, 240, track, ChannelOf(0), 60, 100);

    auto const written = WriteSequenceBytes(builder.GetSequence());

    VERIFY_IS_TRUE(written.Result.Succeeded());
    VERIFY_ARE_EQUAL(uint32_t{ 2 }, written.Result.SkippedEventCount());

    // The note still made it, which is the point of counting rather than failing.
    auto const read = ReadBytes(written.Bytes);

    VERIFY_IS_TRUE(read.Succeeded());
    VERIFY_ARE_EQUAL(uint32_t{ 1 }, read.Sequence().NoteCount());
}

void MidiStandardFileWriterTests::ReassemblesSystemExclusiveFromUniversalPackets()
{
    MidiSequenceBuilder builder{};

    auto const track = builder.AddTrack(L"Dump");

    // A seven byte payload split across a start and an end packet, group zero.
    std::array<uint32_t, 2> start{ 0x30160011, 0x22334455 };
    std::array<uint32_t, 2> end{ 0x30316677, 0x00000000 };

    builder.AddMessages(track, 0, start);
    builder.AddMessages(track, 0, end);

    // Something after it, so the file is not one dump on its own.
    builder.AddNote(480, 240, track, ChannelOf(0), 60, 100);

    auto const written = WriteSequenceBytes(builder.GetSequence());

    VERIFY_IS_TRUE(written.Result.Succeeded());
    VERIFY_ARE_EQUAL(uint32_t{ 0 }, written.Result.SkippedEventCount());

    auto const read = ReadBytes(written.Bytes);

    VERIFY_IS_TRUE(read.Succeeded());

    // One reassembled dump plus the two halves of the note.
    VERIFY_ARE_EQUAL(uint32_t{ 3 }, read.Sequence().EventCount());
    VERIFY_ARE_EQUAL(uint32_t{ 1 }, read.Sequence().NoteCount());
}

void MidiStandardFileWriterTests::AbsoluteTimingIsLaidOutOnAMusicalTimeline()
{
    MidiSequenceBuilder builder{};

    builder.TimingMode(MidiSequenceTimingMode::Absolute);

    auto const track = builder.AddTrack(L"Real time");

    // A tick is a microsecond here, so this note starts half a second in and lasts a quarter of
    // a second.
    builder.AddNote(500000, 250000, track, ChannelOf(0), 60, 100);

    auto const built = builder.GetSequence();

    MidiFileWriteOptions options{};

    options.AbsoluteTimingTicksPerQuarterNote(960);

    auto const reloaded = RoundTrip(built, options);

    VERIFY_ARE_EQUAL(uint16_t{ 960 }, reloaded.TicksPerQuarterNote());

    auto const notes = AllNotes(reloaded);

    VERIFY_ARE_EQUAL(size_t{ 1 }, notes.size());

    // A file is always musically timed, so what has to survive is the wall clock position, not
    // the tick number. At 120 beats per minute a quarter note is half a second.
    VERIFY_ARE_EQUAL(uint64_t{ 500000 }, reloaded.ConvertTickToMicroseconds(notes[0].StartTick));
    VERIFY_ARE_EQUAL(uint64_t{ 750000 }, reloaded.ConvertTickToMicroseconds(notes[0].EndTick));
}

void MidiStandardFileWriterTests::ReportsNothingToWriteForAnEmptySequence()
{
    MidiSequenceBuilder builder{};

    auto const written = WriteSequenceBytes(builder.GetSequence());

    VERIFY_IS_NOT_NULL(written.Result);
    VERIFY_IS_FALSE(written.Result.Succeeded());
    VERIFY_ARE_EQUAL(
        static_cast<int32_t>(MidiFileWriteStatus::NothingToWrite),
        static_cast<int32_t>(written.Result.Status()));
}

void MidiStandardFileWriterTests::ReportsAnErrorForANullSequence()
{
    streams::InMemoryRandomAccessStream stream{};

    auto const result = MidiStandardFileWriter::WriteAsync(stream, nullptr).get();

    VERIFY_IS_NOT_NULL(result);
    VERIFY_IS_FALSE(result.Succeeded());
    VERIFY_ARE_EQUAL(
        static_cast<int32_t>(MidiFileWriteStatus::WriteError),
        static_cast<int32_t>(result.Status()));
}

void MidiStandardFileWriterTests::WritesToAFileOnDisk()
{
    auto const original = ReadTestSequence(L"multi-track.mid");

    // The test host has no package identity, so ApplicationData is not available here. The
    // ordinary temporary folder is.
    auto temporaryPath = std::filesystem::temp_directory_path().wstring();

    while (!temporaryPath.empty() && (temporaryPath.back() == L'\\' || temporaryPath.back() == L'/'))
    {
        temporaryPath.pop_back();
    }

    auto const folder = storage::StorageFolder::GetFolderFromPathAsync(winrt::hstring{ temporaryPath }).get();

    auto file = folder.CreateFileAsync(
        L"midi-standard-file-writer-test.mid",
        storage::CreationCollisionOption::ReplaceExisting).get();

    auto const written = MidiStandardFileWriter::WriteToFileAsync(file, original).get();

    VERIFY_IS_NOT_NULL(written);
    VERIFY_IS_TRUE(written.Succeeded());
    VERIFY_IS_TRUE(written.ByteCount() > 14);
    VERIFY_ARE_EQUAL(original.Tracks().Size(), written.TrackCount());

    auto const read = MidiStandardFileReader::ReadFromFileAsync(file).get();

    VERIFY_IS_NOT_NULL(read);
    VERIFY_IS_TRUE(read.Succeeded());

    VerifySameSequence(original, read.Sequence());
    VerifySameNotes(original, read.Sequence());

    file.DeleteAsync().get();
}
