// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// Further information: https://github.com/microsoft/MIDI/
// ============================================================================

// Windows MIDI Services sample code
//
// FOCUS OF THIS SAMPLE: saving a sequence as a Standard MIDI File (.mid) with
// MidiStandardFileWriter.
//
// Any MidiSequence can be written out: one you built, one you read from another
// file, or a recording of what a device played. Other music software can open
// the result, and MidiStandardFileReader reads it back.
//
// A Standard MIDI File holds MIDI 1.0. When a sequence holds Universal MIDI
// Packets, the writer translates each one that has a MIDI 1.0 form, and counts
// the ones that do not. This sample puts in one of each, so you can see both.
//
// This sample only works with files, so it never connects to the MIDI service,
// and it makes no sound. The file is written to your temporary folder.

#include <iostream>
#include <iomanip>
#include <array>
#include <vector>
#include <string_view>
#include <filesystem>

#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Foundation.Collections.h>
#include <winrt/Windows.Storage.h>
#include <winrt/Windows.Storage.Streams.h>

#include <winrt/Windows.Devices.Midi2.h>
#include <winrt/Windows.Devices.Midi2.Utilities.Messages.h>
#include <winrt/Windows.Devices.Midi2.Utilities.Sequencing.h>
#include <winrt/Windows.Devices.Midi2.Utilities.Files.h>

using namespace winrt::Windows::Devices::Midi2;
using namespace winrt::Windows::Devices::Midi2::Utilities::Messages;
using namespace winrt::Windows::Devices::Midi2::Utilities::Sequencing;
using namespace winrt::Windows::Devices::Midi2::Utilities::Files;

using namespace winrt::Windows::Storage;
using namespace winrt::Windows::Storage::Streams;


// The name of the file to write in your temporary folder.
const winrt::hstring OutputFileName = L"windows-midi-services-sample.mid";

constexpr uint16_t TicksPerQuarterNote = 480;
constexpr uint32_t TicksPerBar = TicksPerQuarterNote * 4;

// Channel 1 is index 0.
constexpr uint8_t MelodyChannelIndex = 0;
constexpr uint8_t ChordsChannelIndex = 1;

// General MIDI program numbers, zero based.
constexpr uint8_t Flute = 73;
constexpr uint8_t DrawbarOrgan = 16;

constexpr uint8_t VolumeController = 7;


// Adds any message MidiMessageBuilder makes, whatever its size, to a track.
void AddMessage(
    MidiSequenceBuilder const& builder,
    uint16_t const trackIndex,
    uint32_t const tick,
    IMidiUniversalPacket const& message)
{
    std::vector<uint32_t> words;

    for (auto const word : message.GetAllWords())
    {
        words.push_back(word);
    }

    builder.AddMessages(trackIndex, tick, words);
}

MidiSequence BuildSequence()
{
    // See the sequence-builder sample for more about building a sequence.
    MidiSequenceBuilder builder;

    MidiGroup const group{ static_cast<uint8_t>(0) };

    builder.TicksPerQuarterNote(TicksPerQuarterNote);
    builder.AddTempoChange(0, 110.0);
    builder.AddTimeSignature(0, 4, 4);

    // Track names go into the file, and most music software shows them.
    auto const melody = builder.AddTrack(L"Melody");
    auto const chords = builder.AddTrack(L"Chords");

    // System exclusive goes into the file exactly as it is.
    std::array<uint8_t, 6> const generalMidiSystemOn{ 0xF0, 0x7E, 0x7F, 0x09, 0x01, 0xF7 };

    builder.AddSystemExclusive(melody, 0, generalMidiSystemOn);

    // A MIDI 1.0 message in Universal MIDI Packet form is written as the same
    // MIDI 1.0 message.
    AddMessage(builder, melody, 0, MidiMessageBuilder::BuildMidi1ChannelVoiceMessage(
        0, group, Midi1ChannelVoiceMessageStatus::ProgramChange, MidiChannel{ MelodyChannelIndex }, Flute, 0));

    AddMessage(builder, chords, 0, MidiMessageBuilder::BuildMidi1ChannelVoiceMessage(
        0, group, Midi1ChannelVoiceMessageStatus::ProgramChange, MidiChannel{ ChordsChannelIndex }, DrawbarOrgan, 0));

    // A MIDI 2.0 control change carries a 32-bit value. MIDI 1.0 has the same
    // controller with a 7-bit value, so the writer scales it down and keeps it.
    AddMessage(builder, chords, 0, MidiMessageBuilder::BuildMidi2ChannelVoiceMessage(
        0, group, Midi2ChannelVoiceMessageStatus::ControlChange, MidiChannel{ ChordsChannelIndex },
        static_cast<uint16_t>(VolumeController << 8), 0xA0000000));

    std::array<uint8_t, 8> const tune{ 72, 74, 76, 79, 81, 79, 76, 74 };

    for (uint32_t index = 0; index < tune.size(); index++)
    {
        builder.AddNote(index * TicksPerQuarterNote, TicksPerQuarterNote, melody, MidiChannel{ MelodyChannelIndex }, tune[index], 96);
    }

    builder.AddNote(2 * TicksPerBar, TicksPerBar, melody, MidiChannel{ MelodyChannelIndex }, 72, 96);

    std::array<std::array<uint8_t, 3>, 3> const chordNotes{ { { 60, 64, 67 }, { 60, 65, 69 }, { 60, 64, 67 } } };

    for (uint32_t bar = 0; bar < chordNotes.size(); bar++)
    {
        for (auto const note : chordNotes[bar])
        {
            builder.AddNote(bar * TicksPerBar, TicksPerBar, chords, MidiChannel{ ChordsChannelIndex }, note, 72);
        }
    }

    // A MIDI 2.0 per-note pitch bend moves one note of a chord and leaves the
    // others where they are. MIDI 1.0 has nothing like it, so the writer leaves
    // out this bend, and the one that puts the note back, and counts them.
    uint16_t const bentNote = 64 << 8;

    AddMessage(builder, chords, 2 * TicksPerQuarterNote, MidiMessageBuilder::BuildMidi2ChannelVoiceMessage(
        0, group, Midi2ChannelVoiceMessageStatus::PerNotePitchBend, MidiChannel{ ChordsChannelIndex }, bentNote, 0xA0000000));

    AddMessage(builder, chords, TicksPerBar, MidiMessageBuilder::BuildMidi2ChannelVoiceMessage(
        0, group, Midi2ChannelVoiceMessageStatus::PerNotePitchBend, MidiChannel{ ChordsChannelIndex }, bentNote, 0x80000000));

    return builder.GetSequence();
}

std::wstring_view WriteStatusText(MidiFileWriteStatus const status)
{
    switch (status)
    {
    case MidiFileWriteStatus::Success: return L"written";
    case MidiFileWriteStatus::NothingToWrite: return L"the sequence is empty";
    case MidiFileWriteStatus::TooLarge: return L"the file would be larger than the limit in the options";
    case MidiFileWriteStatus::AccessDenied: return L"access denied";
    case MidiFileWriteStatus::WriteError: return L"the file could not be written";
    case MidiFileWriteStatus::OutOfMemory: return L"out of memory";
    default: return L"unknown status";
    }
}

void PrintRow(std::wstring_view const label, uint64_t const built, uint64_t const readBack)
{
    std::wcout << L"  " << std::left << std::setw(14) << label
        << std::right << std::setw(8) << built << std::setw(12) << readBack << std::endl;
}


int main()
{
    winrt::init_apartment();

    auto const sequence = BuildSequence();

    // A desktop app can open a folder by its path. A packaged app would usually
    // ask with a FileSavePicker instead, which hands back the StorageFile.
    StorageFile file{ nullptr };

    try
    {
        auto const folder = StorageFolder::GetFolderFromPathAsync(std::filesystem::temp_directory_path().wstring()).get();

        file = folder.CreateFileAsync(OutputFileName, CreationCollisionOption::ReplaceExisting).get();
    }
    catch (winrt::hresult_error const& ex)
    {
        std::wcout << L"Could not create the file: " << ex.message().c_str() << std::endl;
        return 1;
    }

    MidiFileWriteOptions options;

    // Leave out a status byte that repeats the one before it. Every reader
    // handles this, and it makes a file with a lot of notes noticeably smaller.
    options.UseRunningStatus(true);

    // The whole file is built in memory before any of it is written, so a
    // failure never leaves half a file behind.
    auto const result = MidiStandardFileWriter::WriteToFileAsync(file, sequence, options).get();

    if (!result.Succeeded())
    {
        std::wcout << L"Could not write the file: " << WriteStatusText(result.Status()) << std::endl;
        return 1;
    }

    std::wcout << L"Wrote " << file.Path().c_str() << std::endl;
    std::wcout << L"  " << result.TrackCount() << L" tracks, " << result.ByteCount() << L" bytes" << std::endl;

    // Messages MIDI 1.0 has no way to hold. Zero for any sequence that came from
    // a Standard MIDI File in the first place.
    std::wcout << L"  " << result.SkippedEventCount() << L" messages left out, because MIDI 1.0 cannot hold them" << std::endl;

    // Reading the file back is the simplest way to see what went in. It will
    // not be the same bytes, but it is the same music: the same events at the
    // same ticks, and the same tempo, meter, track names and text.
    auto const readResult = MidiStandardFileReader::ReadFromFileAsync(file).get();

    if (!readResult.Succeeded())
    {
        std::wcout << L"Could not read the file back." << std::endl;
        return 1;
    }

    auto const readBack = readResult.Sequence();

    std::wcout << std::endl;
    std::wcout << L"  " << std::left << std::setw(14) << L"" << std::right << std::setw(8) << L"built" << std::setw(12) << L"read back" << std::endl;

    PrintRow(L"Tracks", sequence.Tracks().Size(), readBack.Tracks().Size());
    PrintRow(L"Notes", sequence.NoteCount(), readBack.NoteCount());
    PrintRow(L"Events", sequence.EventCount(), readBack.EventCount());
    PrintRow(L"Milliseconds", sequence.DurationMicroseconds() / 1000, readBack.DurationMicroseconds() / 1000);

    std::wcout << std::endl << L"Tracks in the file:" << std::endl;

    for (auto const& track : readBack.Tracks())
    {
        std::wcout << L"  " << track.Name().c_str() << L", " << track.NoteCount() << L" notes" << std::endl;
    }

    // The same sequence again as a single track, and into memory rather than a
    // file. Any IRandomAccessStream will do, so this is also how you would send
    // a file somewhere without saving it first.
    InMemoryRandomAccessStream memory;

    MidiFileWriteOptions singleTrackOptions;

    // A format 0 file. Some hardware players only take this kind. Which track
    // each message came from is lost.
    singleTrackOptions.WriteSingleTrack(true);
    singleTrackOptions.UseRunningStatus(true);

    auto const memoryResult = MidiStandardFileWriter::WriteAsync(memory, sequence, singleTrackOptions).get();

    if (memoryResult.Succeeded())
    {
        std::wcout << std::endl << L"As a single track, in memory: "
            << memoryResult.TrackCount() << L" track, " << memoryResult.ByteCount() << L" bytes" << std::endl;
    }

    std::wcout << std::endl << L"Done." << std::endl;

    return 0;
}
