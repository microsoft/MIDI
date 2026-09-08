// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// Further information: https://github.com/microsoft/MIDI/
// ============================================================================

// Windows MIDI Services sample code
//
// FOCUS OF THIS SAMPLE: receiving a System Exclusive message and writing it to
// a .syx file.
//
// Under WinMM you would have allocated MIDIHDR buffers, called
// midiInPrepareHeader and midiInAddBuffer for each one, handled MIM_LONGDATA,
// requeued the buffer, and reassembled a message which arrived across several
// buffers.
//
// MidiSystemExclusiveReceiver does the reassembly. It raises BytesReceived when
// a message completes, or earlier if maximumBytesPerEvent bytes accumulate
// first, which is what keeps a very large dump from being buffered in memory
// all at once. When that happens, IsPartial is true on the event args.
//
// >>> SET sysExFilePath BELOW BEFORE RUNNING <<<

using Windows.Devices.Midi2;
using Windows.Devices.Midi2.Diagnostics;
using Windows.Devices.Midi2.Utilities.SysExTransfer;

// ============================================================================
// SET THESE VALUES
// ============================================================================

// Where to write what we receive. The file is overwritten if it already exists.
string sysExFilePath = "";

// The endpoint to listen to. Leave empty to use a diagnostic loopback endpoint.
// Pair this with the sysex-file-sender sample pointed at the same loopback to
// see the whole path work without any hardware.
string sourceEndpointId = "";

// Group 1 is index 0.
const byte SourceGroupIndex = 0;

// Raise an event at least every this many bytes, even mid-message.
const uint MaximumBytesPerEvent = 4096;

// ============================================================================

if (string.IsNullOrEmpty(sysExFilePath))
{
    Console.WriteLine("Set sysExFilePath at the top of this file.");
    return 1;
}

if (!MidiApi.EnsureServiceAvailable())
{
    Console.WriteLine("Could not demand-start the MIDI service.");
    return 1;
}

if (string.IsNullOrEmpty(sourceEndpointId))
{
    sourceEndpointId = MidiDiagnostics.DiagnosticsLoopbackAEndpointDeviceId;
}

using (FileStream outputFile = new FileStream(sysExFilePath, FileMode.Create, FileAccess.Write))
{
    using (MidiSession session = MidiSession.Create("SysEx File Receiver Sample"))
    {
        MidiEndpointConnection connection = session.CreateEndpointConnection(sourceEndpointId);

        if (connection == null)
        {
            Console.WriteLine($"Could not create a connection to {sourceEndpointId}");
            return 1;
        }

        MidiGroup sourceGroup = new MidiGroup(SourceGroupIndex);

        MidiSystemExclusiveReceiver receiver =
            new MidiSystemExclusiveReceiver(connection, sourceGroup, MaximumBytesPerEvent);

        // This handler runs on a service callback thread, so keep it short.
        // Writing to a file is already more work than you want here for a
        // latency-sensitive application; a real one would hand the bytes to a worker.
        void BytesReceivedHandler(MidiSystemExclusiveReceiver sender, MidiSystemExclusiveReceivedEventArgs args)
        {
            IReadOnlyList<byte> bytes = args.Bytes;

            foreach (byte dataByte in bytes)
            {
                outputFile.WriteByte(dataByte);
            }

            string state = args.IsPartial ? "partial message" : "message complete";

            Console.WriteLine($"Received {bytes.Count} byte(s) ({state}). Total messages: {sender.CountMessagesReceived}");
        }

        receiver.BytesReceived += BytesReceivedHandler;

        // Open the connection after the receiver is wired up. A device which
        // starts transmitting the moment you open it can otherwise get ahead of you.
        if (!connection.Open())
        {
            Console.WriteLine("Could not open the connection.");
            return 1;
        }

        if (!receiver.Start())
        {
            Console.WriteLine("Could not start the receiver.");
            return 1;
        }

        Console.WriteLine($"Listening on {sourceEndpointId}");
        Console.WriteLine($"Writing to {sysExFilePath}");
        Console.WriteLine();
        Console.WriteLine("Press Enter to stop.");
        Console.WriteLine();

        Console.ReadLine();

        // Stop flushes anything still buffered, which may raise more events
        // before it returns, so the file stays open until after this call.
        receiver.Stop();

        receiver.BytesReceived -= BytesReceivedHandler;

        Console.WriteLine();
        Console.WriteLine($"{receiver.CountBytesReceived} byte(s) in {receiver.CountMessagesReceived} message(s).");

        session.DisconnectEndpointConnection(connection.ConnectionId);
    }
}

return 0;
