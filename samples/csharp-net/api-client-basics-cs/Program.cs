using Windows.Devices.Midi2;
using Windows.Devices.Enumeration;
using System;

// for comments on what each step does, please see the C++/WinRT sample
// the code is almost identical

Console.WriteLine("Creating session");

using (var session = MidiSession.CreateSession("Sample Session"))
{
    var endpointAId = MidiEndpointDeviceInformation.DiagnosticsLoopbackAEndpointId;
    var endpointBId = MidiEndpointDeviceInformation.DiagnosticsLoopbackBEndpointId;

    Console.WriteLine("Connecting to Sender UMP Endpoint: " + endpointAId);
    Console.WriteLine("Connecting to Receiver UMP Endpoint: " + endpointBId);


    using (var sendEndpoint = session.CreateEndpointConnection(endpointAId))
    using (var receiveEndpoint = session.CreateEndpointConnection(endpointBId))
    {
        // c# allows local functions. This is nicer than anonymous because we can unregister it by name
        void MessageReceivedHandler(object sender, MidiMessageReceivedEventArgs args)
        {
            var ump = args.GetMessagePacket();

            Console.WriteLine();
            Console.WriteLine("Received UMP");
            Console.WriteLine("- Current Timestamp: " + MidiClock.Now);
            Console.WriteLine("- UMP Timestamp:     " + ump.Timestamp);
            Console.WriteLine("- UMP Msg Type:      " + ump.MessageType);
            Console.WriteLine("- UMP Packet Type:   " + ump.PacketType);
            Console.WriteLine("- Message:           " + MidiMessageUtility.GetMessageFriendlyNameFromFirstWord(args.PeekFirstWord()));

            if (ump is MidiMessage32)
            {
                var ump32 = ump as MidiMessage32;

                if (ump32 != null)
                    Console.WriteLine("- Word 0:            0x{0:X}", ump32.Word0);
            }
        };

        receiveEndpoint.MessageReceived += MessageReceivedHandler;

        Console.WriteLine("Opening endpoint connection");

        // once you have wired up all your event handlers, added any filters/listeners, etc.
        // You can open the connection. Doing this will query the cache for the in-protocol 
        // endpoint information and function blocks. If not there, it will send out the requests
        // which will come back asynchronously with responses.
        receiveEndpoint.Open();
        sendEndpoint.Open();


        Console.WriteLine("Creating MIDI 1.0 Channel Voice 32-bit UMP...");

        var ump32 = MidiMessageBuilder.BuildMidi1ChannelVoiceMessage(
            MidiClock.Now, // use current timestamp
            5,      // group 5
            Midi1ChannelVoiceMessageStatus.NoteOn,  // 9
            3,      // channel 3
            120,    // note 120 - hex 0x78
            100);   // velocity 100 hex 0x64

        sendEndpoint.SendMessagePacket((IMidiUniversalPacket)ump32);  // could also use the SendWords methods, etc.

        Console.WriteLine(" ** Wait for the message to arrive, and then press enter to cleanup. ** ");
        Console.ReadLine();

        // you should unregister the event handler as well
        receiveEndpoint.MessageReceived -= MessageReceivedHandler;

        // not strictly necessary
        session.DisconnectEndpointConnection(sendEndpoint.ConnectionId);
        session.DisconnectEndpointConnection(receiveEndpoint.ConnectionId);
    }

}

