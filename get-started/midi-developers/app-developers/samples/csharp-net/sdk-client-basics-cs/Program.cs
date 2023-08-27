using Windows.Devices.Midi2;
using Microsoft.Devices.Midi2;
using Windows.Devices.Enumeration;
using System;

// for comments on what each step does, please see the C++/WinRT sample
// the code is almost identical

Console.WriteLine("Checking for MIDI Services");

var checkResult = MidiServices.CheckForWindowsMidiServices();

if (checkResult == WindowsMidiServicesCheckResult.PresentAndUsable)
{
    Console.WriteLine("MIDI Services Present and usable");
    Console.WriteLine("Creating session settings");

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

            DeviceInformation selectedEndpointInformation = null;

            foreach (var device in endpointDevices)
            {
                Console.WriteLine("  " + device.Name);
                Console.WriteLine("    " + device.Id);
                Console.WriteLine();

                // we'll have a more deterministic way to do this in the future
                if (device.Id.Contains("LOOPBACK"))
                {
                    selectedEndpointInformation = device;
                }
            }

            if (selectedEndpointInformation == null)
            {
                Console.WriteLine("No loopback device found. This is not normal. Exiting");
                return;
            }

            Console.WriteLine("Connecting to UMP Endpoint: " + selectedEndpointInformation.Name);

            var endpoint = session.ConnectBidirectionalEndpoint(selectedEndpointInformation.Id);

            endpoint.MessageReceived += (sender, args) =>
            {
                var ump = args.GetUmp();

                Console.WriteLine();
                Console.WriteLine("Received UMP");
                Console.WriteLine("- Current Timestamp: " + MidiClock.GetMidiTimestamp());
                Console.WriteLine("- UMP Timestamp: " + ump.Timestamp);
                Console.WriteLine("- UMP Type: " + ump.MessageType);

                // if you wish to cast the IMidiUmp to a specific Ump Type, you can do so using .as<T>.

                if (ump is MidiUmp32)
                {
                    var ump32 =ump as MidiUmp32;

                    Console.WriteLine("Word 0: {0:X}", ump32.Word0);
                }
            };

            Console.WriteLine("Opening endpoint connection (this sends out the required discovery messages which will loop back)..");

            // once you have wired up all your event handlers, added any filters/listeners, etc.
            // You can open the connection. Doing this will query the cache for the in-protocol 
            // endpoint information and function blocks. If not there, it will send out the requests
            // which will come back asynchronously with responses.
            endpoint.Open();


            Console.WriteLine("Creating MIDI 1.0 Channel Voice 32-bit UMP...");

            MidiUmp32 ump32 = new MidiUmp32();

            ump32.MessageType = MidiUmpMessageType.Midi1ChannelVoice32;
            endpoint.SendUmp((IMidiUmp)ump32);

            Console.WriteLine("Wait for the message to arrive, and then press enter to cleanup.");
            Console.ReadLine();

            session.DisconnectEndpointConnection(endpoint.Id);
        }
        else
        {
            Console.WriteLine("No MIDI Endpoints were found.");
        }

    }

}
else
{
    if (checkResult == WindowsMidiServicesCheckResult.NotPresent)
    {
        Console.WriteLine("MIDI Services Not Present");
    }
    else if (checkResult == WindowsMidiServicesCheckResult.IncompatibleVersion)
    {
        Console.WriteLine("MIDI Present, but is not a compatible version.");
        Console.WriteLine("Here you would prompt the user to install the latest version from " + MidiServices.LatestMidiServicesInstallUri.ToString());

    }
}
