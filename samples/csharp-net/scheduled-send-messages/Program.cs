// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// Further information: https://github.com/microsoft/MIDI/
// ============================================================================

// Windows MIDI Services sample code
//
// FOCUS OF THIS SAMPLE: scheduling messages to be sent in the future.
//
// There is no WinMM equivalent for this. Under WinMM, if you wanted a note to
// start 500 milliseconds from now, you had to keep a timer in your application
// and call midiOutShortMsg when it fired, which meant your timing was only as
// good as your thread scheduling on a busy machine.
//
// Here, you give the service a timestamp along with the message and the service
// sends it at that time.
//
// The timestamp is not a duration and it is not relative to when you opened the
// connection. It is an absolute value on the same clock MidiClock.Now reads,
// counted from system boot in timestamp ticks. To schedule, take Now and offset
// it, which is what the MidiClock.OffsetTimestampBy... functions are for.

using Windows.Devices.Midi2;
using Windows.Devices.Midi2.Diagnostics;
using Windows.Devices.Midi2.Utilities.Messages;

// The endpoint to send to. Leave empty to use a diagnostic loopback endpoint.
string destinationEndpointId = "";

// Group 1 is index 0.
const byte DestinationGroupIndex = 0;

if (!MidiApi.EnsureServiceAvailable())
{
    Console.WriteLine("Could not demand-start the MIDI service.");
    return 1;
}

if (string.IsNullOrEmpty(destinationEndpointId))
{
    destinationEndpointId = MidiDiagnostics.DiagnosticsLoopbackAEndpointDeviceId;
}

using (MidiSession session = MidiSession.Create("Scheduled Send Sample"))
{
    MidiEndpointConnection connection = session.CreateEndpointConnection(destinationEndpointId);

    if (connection == null || !connection.Open())
    {
        Console.WriteLine($"Could not open a connection to {destinationEndpointId}");
        return 1;
    }

    MidiGroup group = new MidiGroup(DestinationGroupIndex);

    // Read the clock once and schedule everything relative to that single value.
    // Calling Now again for each message would let the gap between the calls
    // creep into your timing.
    ulong startTimestamp = MidiClock.Now;

    Console.WriteLine("Scheduling four notes, 500 milliseconds apart.");
    Console.WriteLine($"Current timestamp is {startTimestamp}");
    Console.WriteLine();

    byte[] noteNumbers = { 60, 64, 67, 72 };

    for (int i = 0; i < noteNumbers.Length; i++)
    {
        uint offsetMilliseconds = (uint)i * 500;

        // Offset the timestamp we captured, rather than reading the clock again.
        ulong noteOnTimestamp =
            MidiClock.OffsetTimestampByMilliseconds(startTimestamp, offsetMilliseconds);

        // The note off is scheduled 400 milliseconds after its note on, so the
        // notes do not run into each other.
        ulong noteOffTimestamp =
            MidiClock.OffsetTimestampByMilliseconds(startTimestamp, offsetMilliseconds + 400);

        MidiMessage32 noteOn = MidiMessageBuilder.BuildMidi1ChannelVoiceMessage(
            noteOnTimestamp,
            group,
            Midi1ChannelVoiceMessageStatus.NoteOn,
            new MidiChannel(0),
            noteNumbers[i],
            100);

        MidiMessage32 noteOff = MidiMessageBuilder.BuildMidi1ChannelVoiceMessage(
            noteOffTimestamp,
            group,
            Midi1ChannelVoiceMessageStatus.NoteOff,
            new MidiChannel(0),
            noteNumbers[i],
            0);

        connection.SendSingleMessagePacket(noteOn);
        connection.SendSingleMessagePacket(noteOff);

        Console.WriteLine($"Note {noteNumbers[i]} scheduled for +{offsetMilliseconds} ms (timestamp {noteOnTimestamp})");
    }

    // Everything above returned immediately. The messages are held by the service
    // and sent at their timestamps, so we have to stay alive long enough for the
    // last one to go out.
    Console.WriteLine();
    Console.WriteLine("All messages handed to the service. Waiting for the last one.");

    Thread.Sleep(2500);

    Console.WriteLine("Done.");

    session.DisconnectEndpointConnection(connection.ConnectionId);
}

return 0;
