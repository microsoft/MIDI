// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// Further information: https://github.com/microsoft/MIDI/
// ============================================================================

// Windows MIDI Services sample code
//
// FOCUS OF THIS SAMPLE: sending a .syx file to a device.
//
// Under WinMM you would have read the file into a buffer, filled in a MIDIHDR,
// called midiOutPrepareHeader, midiOutLongMsg, and then midiOutUnprepareHeader,
// and you would have been responsible for not freeing the buffer too early.
//
// Here, MidiSystemExclusiveSender reads the MIDI 1.0 bytestream from a stream,
// converts it into SysEx 7 UMP messages, and paces the transfer for you.
//
// >>> YOU MUST SET SysExFilePath BELOW BEFORE THIS SAMPLE WILL DO ANYTHING <<<
//
// We deliberately do not generate SysEx data or pick a device for you. Sending
// arbitrary System Exclusive data to a random device is a good way to change
// settings you did not mean to change, or to put a device into a mode you did
// not expect. Point this at hardware you own and understand.

using Windows.Devices.Midi2;
using Windows.Devices.Midi2.Diagnostics;
using Windows.Devices.Midi2.Utilities.Messages;
using Windows.Devices.Midi2.Utilities.SysExTransfer;
using Windows.Storage;
using Windows.Storage.Streams;

// ============================================================================
// SET THESE VALUES
// ============================================================================

// Full path to the .syx file you want to send. This file should contain raw
// MIDI 1.0 bytestream SysEx data, including the 0xF0 start and 0xF7 end bytes.
string sysExFilePath = "";

// The endpoint to send to. Leave empty to use a diagnostic loopback endpoint so
// that running the sample unchanged cannot disturb your hardware.
string destinationEndpointId = "";

// Group 1 is index 0. Set this to the group your device expects.
const byte DestinationGroupIndex = 0;

// ============================================================================

if (string.IsNullOrEmpty(sysExFilePath))
{
    Console.WriteLine("Set sysExFilePath at the top of this file to the .syx file you want to send.");
    return 1;
}

if (!MidiApi.EnsureServiceAvailable())
{
    Console.WriteLine("Could not demand-start the MIDI service.");
    return 1;
}

if (string.IsNullOrEmpty(destinationEndpointId))
{
    destinationEndpointId = MidiDiagnostics.DiagnosticsLoopbackAEndpointDeviceId;
}

// Open the file and get an input stream over it. The sender reads the bytestream
// from here, so a large dump does not have to be held in memory.
StorageFile file;

try
{
    file = await StorageFile.GetFileFromPathAsync(sysExFilePath);
}
catch (Exception ex)
{
    Console.WriteLine($"Could not open the file: {ex.Message}");
    return 1;
}

using (IRandomAccessStreamWithContentType fileStream = await file.OpenReadAsync())
{
    IInputStream inputStream = fileStream.GetInputStreamAt(0);

    using (MidiSession session = MidiSession.Create("SysEx File Sender Sample"))
    {
        MidiEndpointConnection connection = session.CreateEndpointConnection(destinationEndpointId);

        if (connection == null || !connection.Open())
        {
            Console.WriteLine($"Could not open a connection to {destinationEndpointId}");
            return 1;
        }

        MidiGroup destinationGroup = new MidiGroup(DestinationGroupIndex);

        // The converter state holds the partial-message and running status state
        // for this transfer. Each transfer needs its own, and it must live for the
        // whole transfer, otherwise a continuation buffer is parsed as a new message.
        MidiBytestreamToUmpMessageConverterState converterState =
            new MidiBytestreamToUmpMessageConverterState();

        Console.WriteLine($"Sending {sysExFilePath}");
        Console.WriteLine($"     to {destinationEndpointId}");
        Console.WriteLine($"  group {destinationGroup.DisplayValue}");
        Console.WriteLine();

        // The two numeric arguments pace the transfer: send 10 messages, then wait
        // 5 milliseconds, and repeat. Many older devices need this time to keep up
        // with a large dump. Pass zero for either value to send with no pacing.
        await MidiSystemExclusiveSender.SendBinarySysEx7ByteDataAsync(
            connection,
            destinationGroup,
            inputStream,
            10,
            5,
            converterState);

        Console.WriteLine("Transfer complete.");

        session.DisconnectEndpointConnection(connection.ConnectionId);
    }
}

return 0;
