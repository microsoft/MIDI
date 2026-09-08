// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// Further information: https://github.com/microsoft/MIDI/
// ============================================================================

// Windows MIDI Services sample code
//
// FOCUS OF THIS SAMPLE: converting MIDI 1.0 bytes you already have in memory
// into UMP words, and sending them.
//
// This is the path to use when your application already holds MIDI 1.0
// bytestream data, which is the usual situation when porting from WinMM. You
// hand the bytes to MidiMessageConverter and send the words it gives back.
//
// The converter state object is the important part. It remembers that you are
// in the middle of a System Exclusive message between calls. Without it, a
// continuation buffer is parsed as though it were the start of a new message.
// If you are sending one complete message at a time you can use the overload
// without a state object, but keeping one costs nothing and is correct in both
// cases.

using Windows.Devices.Midi2;
using Windows.Devices.Midi2.Diagnostics;
using Windows.Devices.Midi2.Utilities.Messages;

// The endpoint to send to. Leave empty to use a diagnostic loopback endpoint so
// the sample is safe to run unchanged.
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

using (MidiSession session = MidiSession.Create("SysEx Send Bytes Sample"))
{
    MidiEndpointConnection connection = session.CreateEndpointConnection(destinationEndpointId);

    if (connection == null || !connection.Open())
    {
        Console.WriteLine($"Could not open a connection to {destinationEndpointId}");
        return 1;
    }

    MidiGroup destinationGroup = new MidiGroup(DestinationGroupIndex);

    // A Universal Non-Real Time Identity Request. This asks a device to say what
    // it is, and is one of the few System Exclusive messages which is safe to
    // send to hardware you do not know anything about.
    //
    //   F0    start of System Exclusive
    //   7E    Universal Non-Real Time
    //   7F    device id, 7F meaning "all devices"
    //   06    General Information sub-id
    //   01    Identity Request
    //   F7    end of System Exclusive
    byte[] midi1Bytes = { 0xF0, 0x7E, 0x7F, 0x06, 0x01, 0xF7 };

    MidiBytestreamToUmpMessageConverterState converterState =
        new MidiBytestreamToUmpMessageConverterState();

    // allowRunningStatus is false here because our buffer carries a complete
    // message with its own status byte. Set it to true when you are feeding a
    // stream which relies on running status, as MIDI 1.0 hardware often does.
    IList<uint> words = MidiMessageConverter.ConvertMidi1CompleteMessageBytesToUmpWords(
        destinationGroup,
        midi1Bytes,
        false,
        converterState);

    Console.WriteLine($"{midi1Bytes.Length} MIDI 1.0 byte(s) became {words.Count} UMP word(s):");

    foreach (uint word in words)
    {
        Console.WriteLine($"  0x{word:X8}");
    }

    // All of these words belong to one logical transfer, so they go out together
    // with a single timestamp. Use the constant rather than a literal zero, so the
    // intent is obvious and you are not depending on the value staying what it is.
    MidiSendMessageResults result =
        connection.SendMultipleMessagesWordList(MidiClock.TimestampConstantSendImmediately, words);

    Console.WriteLine();
    Console.WriteLine(MidiEndpointConnection.SendMessageSucceeded(result) ? "Sent." : "Send failed.");

    session.DisconnectEndpointConnection(connection.ConnectionId);
}

return 0;
