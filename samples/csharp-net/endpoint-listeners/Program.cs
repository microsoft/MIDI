// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// Further information: https://github.com/microsoft/MIDI/
// ============================================================================

// Windows MIDI Services sample code
//
// FOCUS OF THIS SAMPLE: receiving messages from only one group.
//
// This is how you emulate a WinMM port. Under WinMM, one handle gave you one
// port, so filtering was implicit. Here a single connection carries up to 16
// groups in each direction, so if your application presents ports to its users
// you have to do the filtering yourself.
//
// MidiGroupEndpointListener does it for you. Add the groups you care about, and
// its MessageReceived fires only for those.
//
// Note PreventFiringMainMessageReceivedEvent below. Set it when you also handle
// the connection's own MessageReceived event, otherwise every message reaches
// your application twice: once through the listener and once through the
// connection.

using Windows.Devices.Midi2;
using Windows.Devices.Midi2.ClientPlugins;
using Windows.Devices.Midi2.Diagnostics;
using Windows.Devices.Midi2.Utilities.Messages;

// The endpoint to listen to. Leave empty to use a diagnostic loopback endpoint.
string endpointId = "";

if (!MidiApi.EnsureServiceAvailable())
{
    Console.WriteLine("Could not demand-start the MIDI service.");
    return 1;
}

if (string.IsNullOrEmpty(endpointId))
{
    endpointId = MidiDiagnostics.DiagnosticsLoopbackAEndpointDeviceId;
}

using (MidiSession session = MidiSession.Create("Endpoint Listeners Sample"))
{
    MidiEndpointConnection connection = session.CreateEndpointConnection(endpointId);

    if (connection == null)
    {
        Console.WriteLine($"Could not create a connection to {endpointId}");
        return 1;
    }

    MidiGroupEndpointListener listener = new MidiGroupEndpointListener();

    // Listen to group 1 (index 0) and group 3 (index 2) only. Anything arriving
    // on another group will not reach our handler.
    listener.IncludedGroups.Add(new MidiGroup(0));
    listener.IncludedGroups.Add(new MidiGroup(2));

    // We are not handling the connection's own MessageReceived event in this
    // sample, so leaving this false would be fine. It is set here because the
    // moment you do handle both, forgetting it means double delivery.
    listener.PreventFiringMainMessageReceivedEvent = true;

    void MessageReceivedHandler(IMidiMessageReceivedEventSource sender, MidiMessageReceivedEventArgs args)
    {
        // PeekFirstWord avoids materializing a packet object when all you need is
        // to look at the message. The group index lives in the second nibble of
        // word 0 for every message type which carries a group.
        uint word0 = args.PeekFirstWord();
        byte groupIndex = (byte)((word0 & 0x0F000000) >> 24);

        Console.WriteLine($"Group {new MidiGroup(groupIndex).DisplayValue}  " +
            $"{args.MessageType}  ({args.PacketType})");
        Console.WriteLine($"  timestamp: {args.Timestamp}");
        Console.WriteLine($"  word 0:    0x{word0:X8}");
    }

    listener.MessageReceived += MessageReceivedHandler;

    // Plugins must be added before Open, and the result must be checked. A plugin
    // which was not added is simply never called, and that is indistinguishable
    // from an endpoint which is not sending anything.
    MidiMessageProcessingPluginAddResult addResult = connection.AddMessageProcessingPlugin(listener);

    if (addResult != MidiMessageProcessingPluginAddResult.Succeeded)
    {
        Console.WriteLine($"Could not add the listener: {addResult}");
        return 1;
    }

    if (!connection.Open())
    {
        Console.WriteLine("Could not open the connection.");
        return 1;
    }

    Console.WriteLine($"Listening to groups 1 and 3 on {endpointId}");
    Console.WriteLine("Send messages to that endpoint to see them here.");
    Console.WriteLine("Press Enter to stop.");
    Console.WriteLine();

    Console.ReadLine();

    listener.MessageReceived -= MessageReceivedHandler;

    session.DisconnectEndpointConnection(connection.ConnectionId);
}

return 0;
