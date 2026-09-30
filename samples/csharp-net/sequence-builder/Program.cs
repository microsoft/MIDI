// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// Further information: https://github.com/microsoft/MIDI/
// ============================================================================

// Windows MIDI Services sample code
//
// FOCUS OF THIS SAMPLE: building a sequence in memory with MidiSequenceBuilder,
// and playing it.
//
// A MidiSequence usually comes from a file. The builder makes the same thing out
// of messages your application decides on: a phrase behind a button, a backing
// part worked out while someone plays, a test pattern. The player works with it
// without knowing where it came from, and so does anything that draws notes.
//
// This builds two sequences. The first counts in beats, so a tempo change moves
// everything after it. The second counts in microseconds, for the times when a
// pause has to last a set time, whatever the tempo.
//
// Both play on the General MIDI synthesizer that comes with Windows MIDI
// Services, so you will hear them on your default audio device.

using Windows.Devices.Midi2;
using Windows.Devices.Midi2.Transports.Synth;
using Windows.Devices.Midi2.Utilities.Messages;
using Windows.Devices.Midi2.Utilities.Sequencing;

// The endpoint to play on. Leave empty to use the General MIDI synthesizer.
string destinationEndpointId = "";

// A tick is the smallest step of musical time. 480 to the quarter note divides
// evenly into eighths, sixteenths and triplets.
const ushort TicksPerQuarterNote = 480;
const uint TicksPerEighthNote = TicksPerQuarterNote / 2;
const uint TicksPerHalfNote = TicksPerQuarterNote * 2;
const uint TicksPerBar = TicksPerQuarterNote * 4;

// Channel 1 is index 0.
const byte PianoChannelIndex = 0;
const byte StringsChannelIndex = 1;
const byte BassChannelIndex = 2;
const byte GuitarChannelIndex = 3;

// General MIDI program numbers. They are zero based, which is how they are sent.
const byte AcousticGrandPiano = 0;
const byte NylonStringGuitar = 24;
const byte AcousticBass = 32;
const byte StringEnsemble = 48;

const byte ExpressionController = 11;

const byte OnBeatVelocity = 88;
const byte OffBeatVelocity = 64;

if (!MidiApi.EnsureServiceAvailable())
{
    Console.WriteLine("Could not demand-start the MIDI service.");
    return 1;
}

// The synthesizer has exactly one endpoint, so it can be named without
// enumerating. The id is empty when the synthesizer is switched off.
if (string.IsNullOrEmpty(destinationEndpointId))
{
    destinationEndpointId = MidiSynthManager.EndpointDeviceId;
}

if (string.IsNullOrEmpty(destinationEndpointId))
{
    Console.WriteLine("The General MIDI synthesizer is not available. It may be switched off in MIDI Settings.");
    return 1;
}

var builder = new MidiSequenceBuilder();

var song = BuildMusicalSequence(builder);

Console.WriteLine("Built a sequence that counts in beats:");
PrintSequence(song);
PrintTempoMap(song);

using var session = MidiSession.Create("Sequence Builder Sample");

var connection = session.CreateEndpointConnection(destinationEndpointId);

if (connection == null || !connection.Open())
{
    Console.WriteLine($"Could not open a connection to {destinationEndpointId}");
    return 1;
}

// The player borrows the connection rather than opening one of its own, and
// never closes it. One connection can serve a player and anything else your
// application sends.
using var player = new MidiSequencePlayer(connection, new MidiGroup(0));

Console.WriteLine();
Console.WriteLine("Playing...");
await PlayToEnd(player, song);

var strum = BuildRealTimeSequence(builder);

Console.WriteLine();
Console.WriteLine("Built a sequence that counts in microseconds:");
PrintSequence(strum);

// A player can take one sequence after another.
Console.WriteLine();
Console.WriteLine("Playing...");
await PlayToEnd(player, strum);

Console.WriteLine();
Console.WriteLine("Done.");

return 0;


// The builder has a method for notes, because a note needs an end as well as a
// start. Every other message goes in as Universal MIDI Packet words, which
// MidiMessageBuilder makes for you. The group in the words is replaced by the
// group the sequence is played on, so what you put there does not matter.
static void AddChannelVoiceMessage(
    MidiSequenceBuilder builder,
    ushort trackIndex,
    uint tick,
    Midi1ChannelVoiceMessageStatus status,
    byte channelIndex,
    byte data1,
    byte data2)
{
    var message = MidiMessageBuilder.BuildMidi1ChannelVoiceMessage(
        0,
        new MidiGroup(0),
        status,
        new MidiChannel(channelIndex),
        data1,
        data2);

    builder.AddMessages(trackIndex, tick, new[] { message.Word0 });
}

// The same, for a MIDI 2.0 control change. Its value is 32 bits rather than the
// 7 bits MIDI 1.0 allows. The words are stored and played exactly as built.
static void AddMidi2ControlChange(
    MidiSequenceBuilder builder,
    ushort trackIndex,
    uint tick,
    byte channelIndex,
    byte controller,
    uint value)
{
    var message = MidiMessageBuilder.BuildMidi2ChannelVoiceMessage(
        0,
        new MidiGroup(0),
        Midi2ChannelVoiceMessageStatus.ControlChange,
        new MidiChannel(channelIndex),
        (ushort)(controller << 8),      // the controller number is the high byte
        value);

    builder.AddMessages(trackIndex, tick, new[] { message.Word0, message.Word1 });
}

static MidiSequence BuildMusicalSequence(MidiSequenceBuilder builder)
{
    // One bar each: C, A minor, F and G. A fifth bar goes home to C.
    (byte[] Notes, byte Bass)[] progression =
    {
        (new byte[] { 60, 64, 67 }, 48),
        (new byte[] { 57, 60, 64 }, 45),
        (new byte[] { 53, 57, 60 }, 41),
        (new byte[] { 55, 59, 62 }, 43),
    };

    builder.TimingMode = MidiSequenceTimingMode.Musical;
    builder.TicksPerQuarterNote = TicksPerQuarterNote;

    // Tempo and meter belong to the whole sequence, not to a track.
    builder.AddTimeSignature(0, 4, 4);
    builder.AddTempoChange(0, 100.0);

    // Slow down a little on each beat of the fourth bar, and a little more for
    // the last. Because this sequence counts in beats, everything after a tempo
    // change moves with it.
    for (uint beat = 0; beat < 4; beat++)
    {
        builder.AddTempoChange(3 * TicksPerBar + beat * TicksPerQuarterNote, 96.0 - beat * 5.0);
    }

    uint lastBarTick = (uint)progression.Length * TicksPerBar;

    builder.AddTempoChange(lastBarTick, 76.0);

    // The index each call returns is what you pass to the methods below.
    ushort piano = builder.AddTrack("Piano");
    ushort strings = builder.AddTrack("Strings");
    ushort bass = builder.AddTrack("Bass");

    AddChannelVoiceMessage(builder, piano, 0, Midi1ChannelVoiceMessageStatus.ProgramChange, PianoChannelIndex, AcousticGrandPiano, 0);
    AddChannelVoiceMessage(builder, strings, 0, Midi1ChannelVoiceMessageStatus.ProgramChange, StringsChannelIndex, StringEnsemble, 0);
    AddChannelVoiceMessage(builder, bass, 0, Midi1ChannelVoiceMessageStatus.ProgramChange, BassChannelIndex, AcousticBass, 0);

    for (uint bar = 0; bar < progression.Length; bar++)
    {
        uint barTick = bar * TicksPerBar;
        var chord = progression[bar];

        // Eighth notes, up through the chord to the octave and back down.
        byte[] arpeggio =
        {
            chord.Notes[0], chord.Notes[1], chord.Notes[2], (byte)(chord.Notes[0] + 12),
            chord.Notes[2], chord.Notes[1], chord.Notes[0], chord.Notes[1]
        };

        for (uint step = 0; step < arpeggio.Length; step++)
        {
            // AddNote writes the note on and the note off together, so a note
            // added this way can never be left sounding.
            builder.AddNote(
                barTick + step * TicksPerEighthNote,
                TicksPerEighthNote,
                piano,
                new MidiChannel(PianoChannelIndex),
                arpeggio[step],
                step % 2 == 0 ? OnBeatVelocity : OffBeatVelocity);
        }

        foreach (byte note in chord.Notes)
        {
            builder.AddNote(barTick, TicksPerBar, strings, new MidiChannel(StringsChannelIndex), note, 72);
        }

        builder.AddNote(barTick, TicksPerHalfNote, bass, new MidiChannel(BassChannelIndex), chord.Bass, 96);
        builder.AddNote(barTick + TicksPerHalfNote, TicksPerHalfNote, bass, new MidiChannel(BassChannelIndex), chord.Bass, 80);
    }

    foreach (byte note in new byte[] { 60, 64, 67, 72 })
    {
        builder.AddNote(lastBarTick, TicksPerBar, piano, new MidiChannel(PianoChannelIndex), note, 80);
    }

    builder.AddNote(lastBarTick, TicksPerBar, bass, new MidiChannel(BassChannelIndex), 36, 90);

    // The strings swell from a quarter of full expression to all of it over the
    // first four bars, in sixteenth note steps.
    const uint SwellSteps = 64;
    const uint SwellStartValue = 0x40000000;

    for (uint step = 0; step <= SwellSteps; step++)
    {
        uint value = SwellStartValue + (uint)((ulong)(0xFFFFFFFF - SwellStartValue) * step / SwellSteps);

        AddMidi2ControlChange(builder, strings, step * (lastBarTick / SwellSteps), StringsChannelIndex, ExpressionController, value);
    }

    // Everything added so far, ready to play. You can call this again after
    // adding more; each call makes a new sequence and leaves the builder alone.
    return builder.GetSequence();
}

static MidiSequence BuildRealTimeSequence(MidiSequenceBuilder builder)
{
    // Clear empties the builder so it can be used again. The sequence it made
    // earlier is a separate object and is not affected.
    builder.Clear();

    // Now a tick is a microsecond, and tempo does not apply.
    builder.TimingMode = MidiSequenceTimingMode.Absolute;

    ushort guitar = builder.AddTrack("Guitar");

    // General MIDI System On resets every channel on the device, so music written
    // for General MIDI often starts with it. System exclusive goes in whole, from
    // the F0 to the F7.
    byte[] generalMidiSystemOn = { 0xF0, 0x7E, 0x7F, 0x09, 0x01, 0xF7 };

    builder.AddSystemExclusive(guitar, 0, generalMidiSystemOn);

    // Many devices do not listen while they reset. Give them 200 milliseconds,
    // which is 200 milliseconds whatever the tempo.
    const uint ResetPauseMicroseconds = 200_000;

    AddChannelVoiceMessage(builder, guitar, ResetPauseMicroseconds, Midi1ChannelVoiceMessageStatus.ProgramChange, GuitarChannelIndex, NylonStringGuitar, 0);

    // A strum happens in real time too: one string every 25 milliseconds, low
    // to high, all of them ringing until the same moment.
    byte[] eMajor = { 40, 47, 52, 56, 59, 64 };

    const uint StrumGapMicroseconds = 25_000;
    const uint RingMicroseconds = 2_500_000;

    for (uint index = 0; index < eMajor.Length; index++)
    {
        builder.AddNote(
            ResetPauseMicroseconds + index * StrumGapMicroseconds,
            RingMicroseconds - index * StrumGapMicroseconds,
            guitar,
            new MidiChannel(GuitarChannelIndex),
            eMajor[index],
            90);
    }

    return builder.GetSequence();
}

static void PrintSequence(MidiSequence sequence)
{
    // Input the builder cannot use is left out rather than thrown. If you need
    // to be sure everything went in, compare these counts with what you added.
    Console.WriteLine($"  Tracks: {sequence.Tracks.Count}, notes: {sequence.NoteCount}, events: {sequence.EventCount}");
    Console.WriteLine($"  Plays for {sequence.DurationMicroseconds / 1_000_000.0:F1} seconds");

    foreach (var track in sequence.Tracks)
    {
        Console.WriteLine($"  Track {track.TrackIndex + 1}: {track.Name}, {track.NoteCount} notes");
    }
}

static void PrintTempoMap(MidiSequence sequence)
{
    // The tempo map is how ticks become time. It is worked out once, when the
    // sequence is made, so converting between the two never walks the sequence.
    Console.WriteLine("  Tempo:");

    foreach (var change in sequence.TempoMap)
    {
        var position = sequence.GetBarPositionAtTick(change.Tick);

        Console.WriteLine($"    bar {position.Bar} beat {position.Beat}, {change.MicrosecondsAtTick / 1_000_000.0:F1} s in: {change.BeatsPerMinute:F1} BPM");
    }
}

static async Task PlayToEnd(MidiSequencePlayer player, MidiSequence sequence)
{
    // Preparing converts the whole sequence once, so this is the slow call and
    // Play is not.
    await player.SetSequenceAsync(sequence);

    player.Play();

    // The player hands each message to the service ahead of time, with the time
    // it should go out, and the service sends it then. There is nothing to do
    // here but wait.
    while (player.State == MidiSequencePlayerState.Playing)
    {
        await Task.Delay(100);
    }
}
