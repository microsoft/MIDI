// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// Further information: https://github.com/microsoft/MIDI/
// ============================================================================

// Windows MIDI Services sample code
//
// FOCUS OF THIS SAMPLE: asking a device what it can do, and reading the lists
// it publishes, using MIDI Capability Inquiry.
//
// Capability Inquiry is how a device tells you its manufacturer and model, what
// its channels are set to, and what patches it has, without you needing to know
// anything about that make of device. A patch browser in a sequencer is built
// on exactly this.
//
// The session does the awkward parts: it draws an identifier which must not
// survive a restart, numbers its own requests, puts chunked replies back
// together, asks for further pages of a long list until there are no more, and
// gives up on a device that is not going to answer. What is left here is the
// question you wanted to ask.
//
// Run it against the General MIDI synthesizer, which ships with Windows MIDI
// Services and answers all of this, or point it at a MIDI 2.0 instrument.

using Windows.Devices.Midi2;
using Windows.Devices.Midi2.CapabilityInquiry;

namespace capability_inquiry_browse_cs
{
    internal class Program
    {
        // The in-box General MIDI synthesizer. Replace this with any endpoint
        // id, or leave it and run the sample as it stands.
        private const string EndpointId =
            @"\\?\swd#midisrv#midiu_gmsynth_gm1#{e7cce071-3c03-423f-88d3-f1045d02552b}";

        static void Main(string[] args)
        {
            if (!MidiApi.EnsureServiceAvailable())
            {
                Console.WriteLine("Could not demand-start the MIDI service.");
                return;
            }

            using var session = MidiSession.Create("Capability Inquiry Browse Sample");

            var connection = session.CreateEndpointConnection(EndpointId);

            if (connection == null || !connection.Open())
            {
                Console.WriteLine($"Could not open a connection to {EndpointId}");
                return;
            }

            // The session does not take ownership of the connection and does not
            // close it. Several sessions may share one connection, each with its
            // own identifier, which is how you talk to more than one function
            // block on the same endpoint.
            using var capabilityInquiry = MidiCapabilityInquirySession.Create(connection);

            if (capabilityInquiry == null)
            {
                Console.WriteLine("Could not start a capability inquiry session.");
                return;
            }

            // A device announcing a change sends it to everybody, so these
            // arrive whether or not anything was asked for.
            capabilityInquiry.ProfileStateChanged += (s, e) =>
            {
                Console.WriteLine($"Profile state changed: {e.Message.ProfileId}");
            };

            Console.WriteLine("Looking for devices...");

            // Unlike every other request this waits out the whole timeout,
            // because there is no way to know how many devices are out there
            // until they have all had a chance to answer.
            var responders = capabilityInquiry.DiscoverAsync().GetAwaiter().GetResult();

            if (responders.Count == 0)
            {
                Console.WriteLine("Nothing answered. This endpoint does not implement capability inquiry.");
                return;
            }

            foreach (var responder in responders)
            {
                Console.WriteLine();
                Console.WriteLine($"Responder {responder.Muid} on function block {responder.FunctionBlockNumber}");
                Console.WriteLine($"  Receivable system exclusive size: {responder.ReceivableMaximumSystemExclusiveSize} bytes");

                if (responder.SupportsProfiles)
                {
                    // 0x7F addresses the whole function block. A channel is
                    // 0x00 to 0x0F and a group is 0x7E.
                    var profiles = capabilityInquiry.GetProfilesAsync(responder.Muid, 0x7F)
                        .GetAwaiter().GetResult();

                    Console.WriteLine();
                    Console.WriteLine(" Profiles");

                    foreach (var profileId in profiles.EnabledProfiles)
                    {
                        Console.WriteLine($"  {profileId}  enabled");
                    }

                    foreach (var profileId in profiles.DisabledProfiles)
                    {
                        Console.WriteLine($"  {profileId}  available");
                    }
                }

                if (!responder.SupportsPropertyExchange)
                {
                    Console.WriteLine("  This responder does not offer property exchange.");
                    continue;
                }

                Console.WriteLine();
                Console.WriteLine(" Resources");
                PrintResourceList(capabilityInquiry.GetResourceListAsync(responder.Muid).GetAwaiter().GetResult());

                Console.WriteLine();
                Console.WriteLine(" Device info");
                PrintDeviceInfo(capabilityInquiry.GetDeviceInfoAsync(responder.Muid).GetAwaiter().GetResult());

                Console.WriteLine();
                Console.WriteLine(" Channels");

                var channels = capabilityInquiry.GetChannelListAsync(responder.Muid).GetAwaiter().GetResult();
                PrintChannelList(channels);

                Console.WriteLine();
                Console.WriteLine(" Programs");

                // A device may publish several program lists and point each
                // channel at the one it uses. Following the links is how you
                // reach all of them, and the list de-duplicates itself so a
                // sixteen channel device is not asked for the same collection
                // sixteen times.
                bool askedForAList = false;

                if (channels != null)
                {
                    foreach (var link in channels.GetProgramListLinks())
                    {
                        Console.WriteLine($"  From {link.Title} ({link.ResourceId})");

                        // This asks for pages until the device says there are
                        // no more, so what comes back is the whole list however
                        // long it is.
                        PrintProgramList(
                            capabilityInquiry.GetProgramListAsync(responder.Muid, link.ResourceId)
                                .GetAwaiter().GetResult());

                        askedForAList = true;
                    }
                }

                if (!askedForAList)
                {
                    // A device with a single list does not need a channel list
                    // to point at it, so ask for it directly rather than
                    // deciding it has none.
                    PrintProgramList(
                        capabilityInquiry.GetProgramListAsync(responder.Muid, string.Empty)
                            .GetAwaiter().GetResult());
                }
            }

            Console.WriteLine();
            Console.WriteLine("Done.");
        }

        private static void PrintDeviceInfo(MidiDeviceInfo? info)
        {
            if (info == null)
            {
                Console.WriteLine("  The device did not publish DeviceInfo.");
                return;
            }

            Console.WriteLine($"  Manufacturer : {info.Manufacturer}");
            Console.WriteLine($"  Family       : {info.Family}");
            Console.WriteLine($"  Model        : {info.Model}");
            Console.WriteLine($"  Version      : {info.Version}");
        }

        private static void PrintResourceList(MidiResourceList? resources)
        {
            if (resources == null)
            {
                Console.WriteLine("  The device did not publish a ResourceList.");
                return;
            }

            foreach (var entry in resources.Entries)
            {
                var line = $"  {entry.Resource,-20}";

                // CanSet is a string rather than a flag because the set is
                // open: the specification defines three values and a device may
                // declare its own.
                if (entry.CanSet != MidiResourceListEntry.CanSetNone)
                {
                    line += $" writable({entry.CanSet})";
                }

                if (entry.CanPaginate)
                {
                    line += " paged";
                }

                if (entry.CanSubscribe)
                {
                    line += " subscribable";
                }

                Console.WriteLine(line);
            }
        }

        private static void PrintChannelList(MidiChannelList? channels)
        {
            if (channels == null)
            {
                Console.WriteLine("  The device did not publish a ChannelList.");
                return;
            }

            foreach (var entry in channels.Entries)
            {
                // Channel numbers here run from 1 to 256, not 1 to 16: the
                // specification counts from the first channel of the first
                // group and runs across all sixteen groups.
                Console.WriteLine($"  Channel {entry.Channel,3}  {entry.Title,-24}  {entry.ProgramTitle}");
            }
        }

        private static void PrintProgramList(MidiProgramList? programs)
        {
            if (programs == null)
            {
                Console.WriteLine("  The device did not publish a ProgramList.");
                return;
            }

            Console.WriteLine($"  {programs.Entries.Count} programs");

            int shown = 0;

            foreach (var entry in programs.Entries)
            {
                if (shown++ >= 20)
                {
                    Console.WriteLine("  ...");
                    break;
                }

                // All three values go on the wire exactly as they arrive here.
                // They are zero based, which the specification's normative text
                // requires even though its own worked example is not.
                Console.WriteLine(
                    $"  {entry.BankMsb,3} {entry.BankLsb,3} {entry.ProgramChange,3}  {entry.Title}");
            }
        }
    }
}
