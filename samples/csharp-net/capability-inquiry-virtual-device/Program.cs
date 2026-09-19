// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// Further information: https://github.com/microsoft/MIDI/
// ============================================================================

// Windows MIDI Services sample code
//
// FOCUS OF THIS SAMPLE: being a device that answers MIDI Capability Inquiry.
//
// A virtual device already answers endpoint discovery on your behalf. This
// shows the same thing for capability inquiry: you hand it the lists you want
// to publish and it answers Discovery, property exchange capabilities, get
// requests and profile inquiries itself.
//
// What it declares it can do follows from what you gave it. Publish a resource
// and it declares property exchange; set a profile and it declares profile
// configuration. Declaring a category and then answering nothing is worse than
// not declaring it, so there is no flag for you to get wrong.
//
// Run this, then run the capability-inquiry-browse sample against the endpoint
// name printed below, or open any application that reads patch lists.
//
// See also the virtual-device-app-winui sample, which covers creating a virtual
// device and responding to stream messages.

using Windows.Devices.Midi2;
using Windows.Devices.Midi2.CapabilityInquiry;
using Windows.Devices.Midi2.Enumeration;
using Windows.Devices.Midi2.Transports.Virtual;

namespace capability_inquiry_virtual_device_cs
{
    internal class Program
    {
        private const string DeviceName = "Capability Inquiry Sample Device";

        // Programs this device says it has. A real instrument would build these
        // from whatever it actually holds.
        private static readonly string[] ProgramTitles =
        {
            "Grand Piano", "Bright Piano", "Electric Piano", "Harpsichord",
            "Celesta", "Glockenspiel", "Music Box", "Vibraphone",
            "Church Organ", "Drawbar Organ", "Nylon Guitar", "Steel Guitar",
            "Fretless Bass", "Violin", "Cello", "String Ensemble"
        };

        static void Main(string[] args)
        {
            if (!MidiApi.EnsureServiceAvailable())
            {
                Console.WriteLine("Could not demand-start the MIDI service.");
                return;
            }

            var declaredEndpointInfo = new MidiDeclaredEndpointInfo
            {
                Name = DeviceName,
                ProductInstanceId = "CISAMPLE0001",
                SpecificationVersionMajor = 1,
                SpecificationVersionMinor = 1,
                SupportsMidi10Protocol = true,
                SupportsMidi20Protocol = true,
                HasStaticFunctionBlocks = true
            };

            // A device states who it is through several different carriers and
            // they are required to agree. The responder declares this same
            // identity in its capability inquiry replies, so there is nothing
            // to keep in step by hand.
            var declaredDeviceIdentity = new MidiDeclaredDeviceIdentity(
                0x00, 0x00, 0x41,       // manufacturer system exclusive id
                0x0B, 0x00,             // device family
                0x01, 0x00,             // model
                0x01, 0x00, 0x00, 0x00  // software revision
            );

            var config = new MidiVirtualDeviceCreationConfig(
                DeviceName,
                "Answers capability inquiry",
                "Windows MIDI Services Samples",
                declaredEndpointInfo,
                declaredDeviceIdentity,
                new MidiEndpointUserSuppliedInfo());

            using var session = MidiSession.Create("Capability Inquiry Virtual Device Sample");

            var device = MidiVirtualDeviceManager.CreateVirtualDevice(config);

            if (device == null)
            {
                Console.WriteLine("Could not create the virtual device.");
                return;
            }

            var connection = session.CreateEndpointConnection(device.DeviceEndpointDeviceId);

            if (connection == null)
            {
                Console.WriteLine("Could not create the device-side connection.");
                return;
            }

            if (connection.AddMessageProcessingPlugin(device) != MidiMessageProcessingPluginAddResult.Succeeded)
            {
                Console.WriteLine("Could not attach the virtual device to the connection.");
                return;
            }


            // -----------------------------------------------------------------
            // Everything this device will answer capability inquiry with.

            var responder = device.CapabilityInquiry;

            var deviceInfo = new MidiDeviceInfo(
                declaredDeviceIdentity, "Microsoft", "Windows MIDI Services", DeviceName)
            {
                Version = "1.0"
            };

            responder.DeviceInfo = deviceInfo;
            responder.ChannelList = BuildChannelList();

            // A device may publish more than one program list, told apart by
            // resource id. The channel list above points at this one.
            responder.SetProgramList("main", BuildProgramList());

            // A profile this device supports but has not turned on. An
            // initiator sees it in the disabled list and can offer to enable it.
            var pianoProfile = MidiProfileId.CreateStandardDefined(0x01, 0x01, 0x01, 0x01);

            responder.SetProfiles(0x7F, null, new List<MidiProfileId> { pianoProfile });

            // Anything the API has no type for is published as the JSON text of
            // the resource, and answered the same way as the ones it does have.
            responder.SetResource("StateList", string.Empty,
                """[{"title":"Performance A","resId":"a"},{"title":"Performance B","resId":"b"}]""");

            // Anything the responder did not answer itself arrives here with its
            // payload intact, which is where you implement what the API does not.
            responder.MessageReceived += (s, e) =>
            {
                Console.WriteLine(
                    $"Unhandled capability inquiry message, type 0x{(int)e.Message.MessageType:X2}, " +
                    $"{e.Message.Data.Count} bytes");
            };

            // Nothing above has been said out loud yet. Answering Discovery is
            // this device saying it implements capability inquiry, so it only
            // says it once there is something to answer with.
            responder.IsEnabled = true;


            if (!connection.Open())
            {
                Console.WriteLine("Could not open the device-side connection.");
                return;
            }

            Console.WriteLine("Virtual device running.");
            Console.WriteLine($"  Endpoint name : {DeviceName}");
            Console.WriteLine($"  Identifier    : {responder.GetMuid(0)}");
            Console.WriteLine();
            Console.WriteLine("Press Enter to stop.");

            Console.ReadLine();

            connection.RemoveMessageProcessingPlugin(device.PluginId);
        }

        private static MidiProgramList BuildProgramList()
        {
            var programs = new MidiProgramList();

            byte programChange = 0;

            foreach (var title in ProgramTitles)
            {
                var entry = new MidiProgramListEntry
                {
                    Title = title,

                    // Zero based, and sent on the wire exactly as they appear here.
                    BankMsb = 0,
                    BankLsb = 0,
                    ProgramChange = programChange++
                };

                // A sound set commonly gives a program and its bank variation
                // the same title, so tags are often the only thing telling them
                // apart.
                entry.Tags.Add("Sample");

                programs.Entries.Add(entry);
            }

            return programs;
        }

        private static MidiChannelList BuildChannelList()
        {
            var channels = new MidiChannelList();

            for (ushort channel = 1; channel <= 16; channel++)
            {
                var entry = new MidiChannelListEntry
                {
                    // One based, and the numbering runs across all sixteen
                    // groups rather than restarting, which is why this is not a
                    // MidiChannel.
                    Channel = channel,
                    Title = $"Part {channel}",
                    ProgramTitle = ProgramTitles[0]
                };

                // The route from a channel to the programs it can select.
                // Without this an application has no way to know which list
                // applies where.
                entry.Links.Add(new MidiResourceLink
                {
                    Resource = "ProgramList",
                    ResourceId = "main",
                    Title = "Sample Programs"
                });

                channels.Entries.Add(entry);
            }

            return channels;
        }
    }
}
