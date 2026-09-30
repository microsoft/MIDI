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

using Windows.Devices.Midi2;
using Windows.Devices.Midi2.Utilities.Files;
using Windows.Devices.Midi2.Utilities.Messages;
using Windows.Devices.Midi2.Utilities.Sequencing;
using Windows.Storage;
using Windows.Storage.Streams;

// The name of the file to write in your temporary folder.
const string OutputFileName = "windows-midi-services-sample.mid";

const ushort TicksPerQuarterNote = 480;
const uint TicksPerBar = TicksPerQuarterNote * 4;

// Channel 1 is index 0.
const byte MelodyChannelIndex = 0;
const byte ChordsChannelIndex = 1;

// General MIDI program numbers, zero based.
const byte Flute = 73;
const byte DrawbarOrgan = 16;

const byte VolumeController = 7;

var sequence = BuildSequence();

// A desktop app can open a folder by its path. A packaged app would usually
// ask with a FileSavePicker instead, which hands back the StorageFile.
StorageFile file;

try
{
    var folder = await StorageFolder.GetFolderFromPathAsync(Path.GetTempPath());

    file = await folder.CreateFileAsync(OutputFileName, CreationCollisionOption.ReplaceExisting);
}
catch (Exception ex)
{
    Console.WriteLine($"Could not create the file: {ex.Message}");
    return 1;
}

var options = new MidiFileWriteOptions
{
    // Leave out a status byte that repeats the one before it. Every reader
    // handles this, and it makes a file with a lot of notes noticeably smaller.
    UseRunningStatus = true
};

// The whole file is built in memory before any of it is written, so a failure
// never leaves half a file behind.
var result = await MidiStandardFileWriter.WriteToFileAsync(file, sequence, options);

if (!result.Succeeded)
{
    Console.WriteLine($"Could not write the file: {result.Status}");
    return 1;
}

Console.WriteLine($"Wrote {file.Path}");
Console.WriteLine($"  {result.TrackCount} tracks, {result.ByteCount} bytes");

// Messages MIDI 1.0 has no way to hold. Zero for any sequence that came from a
// Standard MIDI File in the first place.
Console.WriteLine($"  {result.SkippedEventCount} messages left out, because MIDI 1.0 cannot hold them");

// Reading the file back is the simplest way to see what went in. It will not be
// the same bytes, but it is the same music: the same events at the same ticks,
// and the same tempo, meter, track names and text.
var readResult = await MidiStandardFileReader.ReadFromFileAsync(file);

if (!readResult.Succeeded)
{
    Console.WriteLine($"Could not read the file back: {readResult.Status}");
    return 1;
}

var readBack = readResult.Sequence;

Console.WriteLine();
Console.WriteLine($"  {"",-14}{"built",8}{"read back",12}");
PrintRow("Tracks", (ulong)sequence.Tracks.Count, (ulong)readBack.Tracks.Count);
PrintRow("Notes", sequence.NoteCount, readBack.NoteCount);
PrintRow("Events", sequence.EventCount, readBack.EventCount);
PrintRow("Milliseconds", sequence.DurationMicroseconds / 1000, readBack.DurationMicroseconds / 1000);

Console.WriteLine();
Console.WriteLine("Tracks in the file:");

foreach (var track in readBack.Tracks)
{
    Console.WriteLine($"  {track.Name}, {track.NoteCount} notes");
}

// The same sequence again as a single track, and into memory rather than a
// file. Any IRandomAccessStream will do, so this is also how you would send a
// file somewhere without saving it first.
using (var memory = new InMemoryRandomAccessStream())
{
    var singleTrackOptions = new MidiFileWriteOptions
    {
        // A format 0 file. Some hardware players only take this kind. Which
        // track each message came from is lost.
        WriteSingleTrack = true,
        UseRunningStatus = true
    };

    var memoryResult = await MidiStandardFileWriter.WriteAsync(memory, sequence, singleTrackOptions);

    if (memoryResult.Succeeded)
    {
        Console.WriteLine();
        Console.WriteLine($"As a single track, in memory: {memoryResult.TrackCount} track, {memoryResult.ByteCount} bytes");
    }
}

Console.WriteLine();
Console.WriteLine("Done.");

return 0;


// Adds any message MidiMessageBuilder makes, whatever its size, to a track.
static void AddMessage(MidiSequenceBuilder builder, ushort trackIndex, uint tick, IMidiUniversalPacket message)
{
    builder.AddMessages(trackIndex, tick, message.GetAllWords().ToArray());
}

static MidiSequence BuildSequence()
{
    // See the sequence-builder sample for more about building a sequence.
    var builder = new MidiSequenceBuilder();

    var group = new MidiGroup(0);

    builder.TicksPerQuarterNote = TicksPerQuarterNote;
    builder.AddTempoChange(0, 110.0);
    builder.AddTimeSignature(0, 4, 4);

    // Track names go into the file, and most music software shows them.
    ushort melody = builder.AddTrack("Melody");
    ushort chords = builder.AddTrack("Chords");

    // System exclusive goes into the file exactly as it is.
    byte[] generalMidiSystemOn = { 0xF0, 0x7E, 0x7F, 0x09, 0x01, 0xF7 };

    builder.AddSystemExclusive(melody, 0, generalMidiSystemOn);

    // A MIDI 1.0 message in Universal MIDI Packet form is written as the same
    // MIDI 1.0 message.
    AddMessage(builder, melody, 0, MidiMessageBuilder.BuildMidi1ChannelVoiceMessage(
        0, group, Midi1ChannelVoiceMessageStatus.ProgramChange, new MidiChannel(MelodyChannelIndex), Flute, 0));

    AddMessage(builder, chords, 0, MidiMessageBuilder.BuildMidi1ChannelVoiceMessage(
        0, group, Midi1ChannelVoiceMessageStatus.ProgramChange, new MidiChannel(ChordsChannelIndex), DrawbarOrgan, 0));

    // A MIDI 2.0 control change carries a 32-bit value. MIDI 1.0 has the same
    // controller with a 7-bit value, so the writer scales it down and keeps it.
    AddMessage(builder, chords, 0, MidiMessageBuilder.BuildMidi2ChannelVoiceMessage(
        0, group, Midi2ChannelVoiceMessageStatus.ControlChange, new MidiChannel(ChordsChannelIndex),
        (ushort)(VolumeController << 8), 0xA0000000));

    byte[] tune = { 72, 74, 76, 79, 81, 79, 76, 74 };

    for (uint index = 0; index < tune.Length; index++)
    {
        builder.AddNote(index * TicksPerQuarterNote, TicksPerQuarterNote, melody, new MidiChannel(MelodyChannelIndex), tune[index], 96);
    }

    builder.AddNote(2 * TicksPerBar, TicksPerBar, melody, new MidiChannel(MelodyChannelIndex), 72, 96);

    byte[][] chordNotes = { new byte[] { 60, 64, 67 }, new byte[] { 60, 65, 69 }, new byte[] { 60, 64, 67 } };

    for (uint bar = 0; bar < chordNotes.Length; bar++)
    {
        foreach (byte note in chordNotes[bar])
        {
            builder.AddNote(bar * TicksPerBar, TicksPerBar, chords, new MidiChannel(ChordsChannelIndex), note, 72);
        }
    }

    // A MIDI 2.0 per-note pitch bend moves one note of a chord and leaves the
    // others where they are. MIDI 1.0 has nothing like it, so the writer leaves
    // out this bend, and the one that puts the note back, and counts them.
    const ushort BentNote = 64 << 8;

    AddMessage(builder, chords, 2 * TicksPerQuarterNote, MidiMessageBuilder.BuildMidi2ChannelVoiceMessage(
        0, group, Midi2ChannelVoiceMessageStatus.PerNotePitchBend, new MidiChannel(ChordsChannelIndex), BentNote, 0xA0000000));

    AddMessage(builder, chords, TicksPerBar, MidiMessageBuilder.BuildMidi2ChannelVoiceMessage(
        0, group, Midi2ChannelVoiceMessageStatus.PerNotePitchBend, new MidiChannel(ChordsChannelIndex), BentNote, 0x80000000));

    return builder.GetSequence();
}

static void PrintRow(string label, ulong built, ulong readBack)
{
    Console.WriteLine($"  {label,-14}{built,8}{readBack,12}");
}
