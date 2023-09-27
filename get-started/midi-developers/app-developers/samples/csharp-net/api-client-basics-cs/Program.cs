using Windows.Devices.Midi2;
using Windows.Devices.Enumeration;
using System;

// for comments on what each step does, please see the C++/WinRT sample
// the code is almost identical

Console.WriteLine("Creating session");

using (var session = MidiSession.CreateSession("Sample Session"))
{
    Console.WriteLine("Creating Device Selector.");

    var deviceSelector = MidiBidirectionalEndpointConnection.GetDeviceSelector();

    Console.WriteLine("Enumerating through Windows.Devices.Enumeration.");

    var endpointDevices = await DeviceInformation.FindAllAsync(deviceSelector);

    // this currently requires you have a USB MIDI 1.0 device. If you have nothing connected, just remove this check for now
    // That will change once MIDI 2.0 device selectors have been created
    if (endpointDevices.Count > 0)
    {
        Console.WriteLine("Devices found:");

        DeviceInformation selectedOutEndpointInformation = null;
        DeviceInformation selectedInEndpointInformation = null;

        foreach (var device in endpointDevices)
        {
            Console.WriteLine("  " + device.Name);
            Console.WriteLine("    " + device.Id);
            Console.WriteLine();

            // we'll have a more deterministic way to do this in the future
            if (device.Id.Contains("LOOPBACK_BIDI_A"))
            {
                selectedOutEndpointInformation = device;
            }
            else if (device.Id.Contains("LOOPBACK_BIDI_B"))
            {
                selectedInEndpointInformation = device;
            }
        }

        if (selectedOutEndpointInformation == null || selectedInEndpointInformation == null)
        {
            Console.WriteLine("Loopback endpoints were not found. This is not normal. Exiting");
            return;
        }

        Console.WriteLine("Connecting to Sender UMP Endpoint: " + selectedOutEndpointInformation.Name);
        Console.WriteLine("Connecting to Receiver UMP Endpoint: " + selectedInEndpointInformation.Name);


        using (var sendEndpoint = session.ConnectBidirectionalEndpoint(selectedOutEndpointInformation.Id))
        using (var receiveEndpoint = session.ConnectBidirectionalEndpoint(selectedInEndpointInformation.Id))
        {
            // c# allows local functions. This is nicer than anonymous because we can unregister it by name
            void MessageReceivedHandler(object sender, MidiMessageReceivedEventArgs args)
            {
                var ump = args.GetUmp();

                Console.WriteLine();
                Console.WriteLine("Received UMP");
                Console.WriteLine("- Current Timestamp: " + MidiClock.GetMidiTimestamp());
                Console.WriteLine("- UMP Timestamp:     " + ump.Timestamp);
                Console.WriteLine("- UMP Msg Type:      " + ump.MessageType);
                Console.WriteLine("- UMP Packet Type:   " + ump.UmpPacketType);

                // if you wish to cast the IMidiUmp to a specific Ump Type, you can do so using .as<T>.

                if (ump is MidiUmp32)
                {
                    var ump32 = ump as MidiUmp32;

                    Console.WriteLine("- Word 0:            0x{0:X}", ump32.Word0);
                }
            };

            receiveEndpoint.MessageReceived += MessageReceivedHandler;

            Console.WriteLine("Opening endpoint connection (this sends out the required discovery messages which will loop back)..");

            // once you have wired up all your event handlers, added any filters/listeners, etc.
            // You can open the connection. Doing this will query the cache for the in-protocol 
            // endpoint information and function blocks. If not there, it will send out the requests
            // which will come back asynchronously with responses.
            receiveEndpoint.Open();
            sendEndpoint.Open();


            Console.WriteLine("Creating MIDI 1.0 Channel Voice 32-bit UMP...");

            var ump32 = MidiMessageBuilder.BuildMidi1ChannelVoiceMessage(
                MidiClock.GetMidiTimestamp(), // use current timestamp
                5,      // group 5
                Midi1ChannelVoiceMessageStatus.NoteOn,  // 9
                3,      // channel 3
                120,    // note 120 - hex 0x78
                100);   // velocity 100 hex 0x64

            sendEndpoint.SendUmp((IMidiUmp)ump32);  // could also use the SendWords methods, etc.

            Console.WriteLine(" ** Wait for the message to arrive, and then press enter to cleanup. ** ");
            Console.ReadLine();

            // you should unregister the event handler as well
            receiveEndpoint.MessageReceived -= MessageReceivedHandler;

            // not strictly necessary
            session.DisconnectEndpointConnection(sendEndpoint.ConnectionId);
            session.DisconnectEndpointConnection(receiveEndpoint.ConnectionId);
        }

    }
    else
    {
        Console.WriteLine("No MIDI Endpoints were found.");
    }

}

