// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// Further information: https://github.com/microsoft/MIDI/
// ============================================================================

// Windows MIDI Services sample code
//
// FOCUS OF THIS SAMPLE: measuring how fast you can send messages.
//
// This exists mostly so you can see the shape of a tight send loop, and to make
// one point which catches people porting from WinMM: sending is much faster than
// it used to be, and the wire is usually what limits you, not the API.
//
// Two things worth copying from here:
//
//   1. Use MidiClock.TimestampConstantSendImmediately when you want the message
//      to go out now. Scheduling has a cost, and you do not want it in a
//      throughput loop.
//
//   2. Ask the connection how much it will accept per call, per connection.
//      Never hard-code that number. It is not guaranteed to be the same for
//      every endpoint or every release of Windows.
//
// Sending to a diagnostic loopback measures the API and service path without a
// physical device in the way. Point it at real hardware and you are measuring
// the cable, which is a different and much slower thing.

using Windows.Devices.Midi2;
using Windows.Devices.Midi2.Diagnostics;
using Windows.Devices.Midi2.Utilities.Messages;

// The endpoint to send to. Leave empty to use a diagnostic loopback endpoint.
string destinationEndpointId = "";

const int MessageCount = 10000;

if (!MidiApi.EnsureServiceAvailable())
{
    Console.WriteLine("Could not demand-start the MIDI service.");
    return 1;
}

if (string.IsNullOrEmpty(destinationEndpointId))
{
    destinationEndpointId = MidiDiagnostics.DiagnosticsLoopbackAEndpointDeviceId;
}

using (MidiSession session = MidiSession.Create("Send Speed Sample"))
{
    MidiEndpointConnection connection = session.CreateEndpointConnection(destinationEndpointId);

    if (connection == null || !connection.Open())
    {
        Console.WriteLine($"Could not open a connection to {destinationEndpointId}");
        return 1;
    }

    Console.WriteLine($"Max words per transmission for this connection: {connection.GetSupportedMaxMidiWordsPerTransmission()}");
    Console.WriteLine($"Sending {MessageCount} messages.");
    Console.WriteLine();

    MidiGroup group = new MidiGroup(0);

    // Build the message once, outside the loop. The only thing which would change
    // per message is the timestamp, and we are not scheduling here.
    MidiMessage32 message = MidiMessageBuilder.BuildMidi1ChannelVoiceMessage(
        MidiClock.TimestampConstantSendImmediately,
        group,
        Midi1ChannelVoiceMessageStatus.NoteOn,
        new MidiChannel(0),
        60,
        100);

    ulong startTimestamp = MidiClock.Now;

    int failureCount = 0;

    for (int i = 0; i < MessageCount; i++)
    {
        MidiSendMessageResults result = connection.SendSingleMessagePacket(message);

        if (MidiEndpointConnection.SendMessageFailed(result))
        {
            failureCount++;
        }
    }

    ulong duration = MidiClock.Now - startTimestamp;

    double milliseconds = MidiClock.ConvertTimestampTicksToMilliseconds(duration);

    Console.WriteLine($"Elapsed:            {milliseconds:F3} ms");
    Console.WriteLine($"Average per message: {(milliseconds * 1000.0) / MessageCount:F3} microseconds");
    Console.WriteLine($"Failures:            {failureCount}");

    session.DisconnectEndpointConnection(connection.ConnectionId);
}

return 0;
