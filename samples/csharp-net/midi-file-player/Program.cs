// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// Further information: https://github.com/microsoft/MIDI/
// ============================================================================

// Windows MIDI Services sample code
//
// FOCUS OF THIS SAMPLE: reading a Standard MIDI File and playing it, and setting
// the volume of the General MIDI synthesizer while it plays.
//
// MidiStandardFileReader turns a .mid file into a MidiSequence, with its tracks,
// tempo changes, meter, text and notes. MidiSequencePlayer plays the sequence.
// Under WinMM, the MCI sequencer could play a file, but it did not show you
// what was in it.
//
// The synthesizer has a volume of its own, set with MidiSynthConfig. It is
// separate from the volume messages in a file, so a file that resets the
// synthesizer cannot undo it, and changing it does not interrupt what is
// playing. The synthesizer is shared by every application on the PC, so this
// sample puts the volume back the way it found it before it exits.
//
// Pass the path of a .mid file on the command line. Without one, this plays a
// file that comes with Windows.

using Windows.Devices.Midi2;
using Windows.Devices.Midi2.ServiceConfig;
using Windows.Devices.Midi2.Transports.Synth;
using Windows.Devices.Midi2.Utilities.Files;
using Windows.Devices.Midi2.Utilities.Sequencing;
using Windows.Storage;

// The endpoint to play on. Leave empty to use the General MIDI synthesizer. The
// volume keys only work on the synthesizer.
string destinationEndpointId = "";

// The synthesizer's volume while this sample plays, and how far each key press
// moves it. The service keeps the volume inside the range the synthesizer
// supports, which is -60 to +12 decibels today.
const double StartingVolumeDecibels = -6.0;
const double VolumeStepDecibels = 3.0;

// GetFileFromPathAsync needs a full path.
string path = Path.GetFullPath(args.Length > 0
    ? args[0]
    : Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.Windows), "Media", "flourish.mid"));

Console.WriteLine($"Reading {path}");

StorageFile file;

try
{
    file = await StorageFile.GetFileFromPathAsync(path);
}
catch (Exception ex)
{
    Console.WriteLine($"Could not open the file: {ex.Message}");
    return 1;
}

// A MIDI file usually comes from somewhere you do not control, so the reader
// checks every length a file gives it against the bytes that are really there.
// MidiFileReadOptions sets the limits, if the defaults do not suit.
var result = await MidiStandardFileReader.ReadFromFileAsync(file);

if (!result.Succeeded)
{
    Console.WriteLine($"Could not read the file: {result.Status}");
    return 1;
}

// A file that ends early, or stops making sense partway through, is read up to
// that point, and what was read still plays.
if (result.Truncated)
{
    Console.WriteLine("  The file ends early. Playing what could be read.");
}

PrintSequence(result);

if (!MidiApi.EnsureServiceAvailable())
{
    Console.WriteLine("Could not demand-start the MIDI service.");
    return 1;
}

// The synthesizer has exactly one endpoint, so it can be named without
// enumerating. The id is empty when the synthesizer is switched off.
bool playingOnSynth = string.IsNullOrEmpty(destinationEndpointId);

string endpointId = playingOnSynth ? MidiSynthManager.EndpointDeviceId : destinationEndpointId;

if (string.IsNullOrEmpty(endpointId))
{
    Console.WriteLine("The General MIDI synthesizer is not available. It may be switched off in MIDI Settings.");
    return 1;
}

using var session = MidiSession.Create("MIDI File Player Sample");

var connection = session.CreateEndpointConnection(endpointId);

if (connection == null || !connection.Open())
{
    Console.WriteLine($"Could not open a connection to {endpointId}");
    return 1;
}

var synthStatus = MidiSynthManager.GetStatus();
bool controlVolume = playingOnSynth && synthStatus != null;

// Ctrl+C stops playback the same way Esc does, so the volume still gets put back.
using var stop = new CancellationTokenSource();

Console.CancelKeyPress += (sender, e) =>
{
    e.Cancel = true;
    stop.Cancel();
};

double originalVolume = synthStatus?.VolumeDecibels ?? 0.0;
double volume = originalVolume;

if (controlVolume)
{
    volume = SetSynthVolume(StartingVolumeDecibels);

    Console.WriteLine();
    Console.WriteLine($"Synthesizer volume set to {volume:F1} dB. It was {originalVolume:F1} dB.");
}

using var ended = new ManualResetEventSlim(false);

// The player borrows the connection and never closes it. Disposing the player
// closes the player only.
using var player = new MidiSequencePlayer(connection, new MidiGroup(0));

// Raised on the player's own thread when the sequence reaches its end. Keep the
// handler short, and do not call back into the player from it.
player.PlaybackEnded += (sender, e) => ended.Set();

// Preparing converts the whole sequence once, so this is the slow call and Play
// is not.
await player.SetSequenceAsync(result.Sequence);

Console.WriteLine();
Console.WriteLine(controlVolume
    ? "Space pauses and resumes. + and - change the volume. Esc stops."
    : "Space pauses and resumes. Esc stops.");
Console.WriteLine();

player.Play();

while (!ended.Wait(100))
{
    while (Console.KeyAvailable)
    {
        var key = Console.ReadKey(intercept: true);

        if (key.Key == ConsoleKey.Escape)
        {
            stop.Cancel();
        }
        else if (key.Key == ConsoleKey.Spacebar)
        {
            // Pausing silences what is sounding. Playing again sends each
            // channel's sound and controllers first, so it resumes correctly.
            if (player.State == MidiSequencePlayerState.Playing)
            {
                player.Pause();
            }
            else
            {
                player.Play();
            }
        }
        else if (controlVolume && (key.KeyChar == '+' || key.KeyChar == '='))
        {
            volume = SetSynthVolume(volume + VolumeStepDecibels);
        }
        else if (controlVolume && key.KeyChar == '-')
        {
            volume = SetSynthVolume(volume - VolumeStepDecibels);
        }
    }

    if (stop.IsCancellationRequested)
    {
        // Stopping silences what is sounding. It does not raise PlaybackEnded.
        player.Stop();
        break;
    }

    // There is no position event, on purpose. Read the position as often as your
    // display needs it: an event for every frame would cost more.
    var position = player.Position;

    string line = $"\r  bar {position.Bar,3} beat {position.Beat}   " +
        $"{MinutesAndSeconds(position.Microseconds)} / {MinutesAndSeconds(position.DurationMicroseconds)}   " +
        $"{position.BeatsPerMinute,5:F1} BPM";

    if (controlVolume)
    {
        line += $"   volume {volume:+0.0;-0.0} dB";
    }

    Console.Write(line + (position.State == MidiSequencePlayerState.Paused ? "   paused" : "         "));
}

Console.WriteLine();
Console.WriteLine();

if (controlVolume)
{
    volume = SetSynthVolume(originalVolume);

    Console.WriteLine($"Synthesizer volume put back to {volume:F1} dB.");
}

return 0;


static string MinutesAndSeconds(ulong microseconds)
{
    ulong seconds = microseconds / 1_000_000;

    return $"{seconds / 60}:{seconds % 60:D2}";
}

// Bit zero is channel 1.
static string ChannelNumbers(ushort channelMask)
{
    return string.Join(", ", Enumerable.Range(0, 16).Where(channel => (channelMask & (1 << channel)) != 0).Select(channel => channel + 1));
}

static void PrintSequence(MidiFileReadResult result)
{
    var sequence = result.Sequence;

    // Title and copyright come from text in the file, and many files have neither.
    if (!string.IsNullOrEmpty(sequence.Title))
    {
        Console.WriteLine($"  Title      {sequence.Title}");
    }

    if (!string.IsNullOrEmpty(sequence.Copyright))
    {
        Console.WriteLine($"  Copyright  {sequence.Copyright}");
    }

    Console.WriteLine($"  Length     {MinutesAndSeconds(sequence.DurationMicroseconds)}");

    // A file does not have one tempo. It has a map of them, and many files change
    // tempo as they go, so show where it starts and how often it changes.
    var tempoMap = sequence.TempoMap;

    string tempo = $"  Tempo      {tempoMap[0].BeatsPerMinute:F1} BPM at the start";

    if (tempoMap.Count > 1)
    {
        tempo += $", changes after that: {tempoMap.Count - 1}";
    }

    Console.WriteLine(tempo);

    var meter = sequence.TimeSignatureMap[0];

    Console.WriteLine($"  Meter      {meter.Numerator}/{meter.Denominator} at the start");
    Console.WriteLine($"  Notes      {sequence.NoteCount}");

    if (sequence.IsKaraoke)
    {
        Console.WriteLine($"  Karaoke    lines of words: {sequence.LyricLines.Count}");
    }

    // A file that says it has more tracks than it holds is common enough to be
    // worth reporting.
    Console.WriteLine(result.DeclaredTrackCount == result.ReadTrackCount
        ? $"  Tracks     {result.ReadTrackCount}"
        : $"  Tracks     {result.ReadTrackCount} (the file says {result.DeclaredTrackCount})");

    Console.WriteLine($"     #  {"Name",-24}{"Notes",6}  Channels");

    foreach (var track in sequence.Tracks)
    {
        // A track with no notes is usually the one that holds the tempo map.
        if (track.NoteCount == 0)
        {
            continue;
        }

        string name = string.IsNullOrEmpty(track.Name) ? "(no name)" : track.Name;

        Console.WriteLine($"    {track.TrackIndex + 1,2}  {name,-24}{track.NoteCount,6}  {ChannelNumbers(track.UsedChannelMask)}");
    }
}

// Sets the synthesizer's volume, and returns the volume it really took.
static double SetSynthVolume(double decibels)
{
    // Start from the current settings and change only the volume. A config holds
    // every setting, so one made from nothing would put all of the others back to
    // their defaults.
    var config = new MidiSynthConfig(MidiSynthManager.GetStatus())
    {
        VolumeDecibels = decibels
    };

    // SendUpdate changes the running service and saves nothing, so the customer's
    // own setting comes back when the service restarts. Call SaveUpdate as well
    // only when the customer asked for the change to stay.
    var response = MidiServiceTransportPluginConfigManager.SendUpdate(config);

    if (response.Status != MidiServiceConfigResponseStatus.Success)
    {
        Console.WriteLine();
        Console.WriteLine("The service did not accept the volume change.");
    }

    // A value out of range is moved to the nearest limit rather than refused, so
    // read back what was used.
    return MidiSynthManager.GetStatus()?.VolumeDecibels ?? decibels;
}
