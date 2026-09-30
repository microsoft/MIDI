// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// Further information: https://github.com/microsoft/MIDI/
// ============================================================================

// Windows MIDI Services sample code
//
// FOCUS OF THIS SAMPLE: reading the channel list and the program lists a device
// publishes through MIDI Capability Inquiry, and using an entry from a program
// list to choose the sound on a channel.
//
// The General MIDI synthesizer that comes with Windows MIDI Services publishes
// both. Its channel list says what each channel is playing, and which program
// list that channel chooses from: one list of instruments, and one of drum kits
// for channel 10. A workstation with factory, user and expansion banks has the
// same shape, so code written against the synthesizer carries over.
//
// The capability-inquiry-browse sample walks everything a device publishes.
// This one goes further into these two lists. It sends one program change,
// which makes no sound, and puts the channel back afterward.

using Windows.Devices.Midi2;
using Windows.Devices.Midi2.CapabilityInquiry;
using Windows.Devices.Midi2.Transports.Synth;
using Windows.Devices.Midi2.Utilities.Messages;

// The endpoint to ask. Leave empty to use the General MIDI synthesizer.
string endpointId = "";

// How many program list entries to ask for at a time.
const uint ProgramListPageSize = 64;

// The channel to choose a sound for, and the category to choose it from.
// Channel numbers in a channel list start at 1.
const ushort ChannelToChange = 1;
const string CategoryToChoose = "Organ";

if (!MidiApi.EnsureServiceAvailable())
{
    Console.WriteLine("Could not demand-start the MIDI service.");
    return 1;
}

// The synthesizer has exactly one endpoint, so it can be named without
// enumerating. The id is empty when the synthesizer is switched off.
if (string.IsNullOrEmpty(endpointId))
{
    endpointId = MidiSynthManager.EndpointDeviceId;
}

if (string.IsNullOrEmpty(endpointId))
{
    Console.WriteLine("The General MIDI synthesizer is not available. It may be switched off in MIDI Settings.");
    return 1;
}

using var session = MidiSession.Create("Capability Inquiry Program List Sample");

var connection = session.CreateEndpointConnection(endpointId);

if (connection == null || !connection.Open())
{
    Console.WriteLine($"Could not open a connection to {endpointId}");
    return 1;
}

// Disposing the session withdraws its identifier, which tells the device to
// stop keeping track of it.
using var capabilityInquiry = MidiCapabilityInquirySession.Create(connection);

if (capabilityInquiry == null)
{
    Console.WriteLine("Could not start a capability inquiry session.");
    return 1;
}

Console.WriteLine("Looking for devices...");

var responders = await capabilityInquiry.DiscoverAsync();
var responder = responders.FirstOrDefault(found => found.SupportsPropertyExchange);

if (responder == null)
{
    Console.WriteLine("Nothing on this endpoint publishes lists.");
    return 0;
}

// Identifies the responder for as long as this session lasts. It changes every
// time the device starts, so never store it.
var muid = responder.Muid;

// Checking the resource list first saves asking for something the device can
// only refuse. A device that sends no resource list has not said it lacks
// anything, so only rule a resource out when a list came back.
var resources = await capabilityInquiry.GetResourceListAsync(muid);

if (resources != null && !resources.SupportsResource("ChannelList"))
{
    Console.WriteLine("This device does not publish a channel list.");
    return 0;
}

var channels = await capabilityInquiry.GetChannelListAsync(muid);

if (channels == null)
{
    Console.WriteLine("The device did not send its channel list.");
    return 0;
}

// A channel list is how things stood when it was sent. To hear about changes
// someone else makes, such as a program change from a keyboard, subscribe to it
// with SubscribeAsync when its resource list entry says CanSubscribe.
Console.WriteLine();
Console.WriteLine(" Channels");

foreach (var entry in channels.Entries)
{
    var link = ProgramListLinkFor(entry);

    Console.WriteLine($"  {entry.Channel,3}  {entry.ProgramTitle,-16}{ProgramNumbers(entry.BankMsb, entry.BankLsb, entry.ProgramChange)}  from {link?.Title ?? "no list"}");
}

// Each distinct list the channels point at, once, however many channels share it.
Console.WriteLine();
Console.WriteLine(" Program lists");

var programLists = new Dictionary<string, List<MidiProgramListEntry>>();

foreach (var link in channels.GetProgramListLinks())
{
    Console.WriteLine($"  {link.Title} ({link.ResourceId})");

    var entries = await ReadProgramList(capabilityInquiry, muid, link.ResourceId);

    PrintByCategory(entries);

    programLists[link.ResourceId] = entries;
}

// Choose a sound from the list the channel itself points at. A drum channel
// points at drum kits, so this never offers it a piano.
Console.WriteLine();
Console.WriteLine($" Choosing a sound for channel {ChannelToChange}");

var before = channels.GetEntryForChannel(ChannelToChange);
var channelLink = before == null ? null : ProgramListLinkFor(before);

if (before == null || channelLink == null)
{
    Console.WriteLine("  The device does not say which list this channel uses.");
    return 0;
}

var choices = programLists.GetValueOrDefault(channelLink.ResourceId) ?? new List<MidiProgramListEntry>();

var program = choices.FirstOrDefault(entry => entry.Categories.Contains(CategoryToChoose)) ?? choices.FirstOrDefault();

if (program == null)
{
    Console.WriteLine("  That list is empty.");
    return 0;
}

Console.WriteLine($"  Channel {ChannelToChange} plays {before.ProgramTitle} {ProgramNumbers(before.BankMsb, before.BankLsb, before.ProgramChange)}");
Console.WriteLine($"  Sending {DisplayName(program)} {ProgramNumbers(program.BankMsb, program.BankLsb, program.ProgramChange)}");

SendProgram(connection, ChannelToChange, program.BankMsb, program.BankLsb, program.ProgramChange);

// The device handles messages in the order they arrive, so a channel list asked
// for after the program change already shows it.
var updated = await capabilityInquiry.GetChannelListAsync(muid);
var after = updated?.GetEntryForChannel(ChannelToChange);

if (after != null)
{
    Console.WriteLine($"  Channel {ChannelToChange} now plays {after.ProgramTitle} {ProgramNumbers(after.BankMsb, after.BankLsb, after.ProgramChange)}");
}

// The synthesizer is shared by every application on the PC, so put the channel
// back the way it was.
SendProgram(connection, ChannelToChange, before.BankMsb, before.BankLsb, before.ProgramChange);

Console.WriteLine($"  Put channel {ChannelToChange} back to {before.ProgramTitle}");

Console.WriteLine();
Console.WriteLine("Done.");

return 0;


// Bank select MSB, bank select LSB and program change, exactly as they are sent.
static string ProgramNumbers(byte bankMsb, byte bankLsb, byte programChange)
{
    return $"[{bankMsb,3} {bankLsb,3} {programChange,3}]";
}

// A sound set often gives a program and its variations the same title. The tags
// are what tell them apart.
static string DisplayName(MidiProgramListEntry entry)
{
    return entry.Tags.Count > 0 ? $"{entry.Title} ({string.Join(", ", entry.Tags)})" : entry.Title;
}

// The program list a channel chooses from, or null when it names none.
static MidiResourceLink? ProgramListLinkFor(MidiChannelListEntry entry)
{
    return entry.Links.FirstOrDefault(link => link.Resource == "ProgramList");
}

// Reads a whole program list a page at a time. GetProgramListAsync runs this
// same loop for you. Page it yourself when you want to show the first entries
// while the rest are still on their way.
static async Task<List<MidiProgramListEntry>> ReadProgramList(
    MidiCapabilityInquirySession capabilityInquiry,
    MidiUniqueId responder,
    string resourceId)
{
    var entries = new List<MidiProgramListEntry>();

    uint offset = 0;

    while (true)
    {
        var page = await capabilityInquiry.GetProgramListPageAsync(responder, resourceId, offset, ProgramListPageSize);

        // Null when the device did not answer, or turned the request down.
        if (page == null || page.Entries.Count == 0)
        {
            break;
        }

        entries.AddRange(page.Entries);

        Console.WriteLine($"    {entries.Count} of {page.TotalCount}");

        // A device that does not page sends the whole list at once, and says
        // there is no more.
        if (!page.HasMoreEntries)
        {
            break;
        }

        offset = page.NextOffset;
    }

    return entries;
}

// Categories are optional, and a device may use names of its own, so treat them
// as text to show, and be ready for entries that have none.
static void PrintByCategory(List<MidiProgramListEntry> entries)
{
    foreach (var group in entries.GroupBy(entry => entry.Categories.Count > 0 ? entry.Categories[0] : "(none)"))
    {
        var members = group.ToList();

        string examples = string.Join(", ", members.Take(2).Select(DisplayName)) + (members.Count > 2 ? ", ..." : "");

        Console.WriteLine($"    {group.Key,-22}{members.Count,4}  {examples}");
    }
}

// Bank select is two control changes, 0 for the high byte and 32 for the low,
// and it takes effect at the next program change. The values from a program
// list go on the wire exactly as they are. They are already zero based.
static void SendProgram(MidiEndpointConnection connection, ushort channelListNumber, byte bankMsb, byte bankLsb, byte program)
{
    // A channel list counts channels from 1 to 256, across all sixteen groups.
    // Split that into the group and the channel within it.
    var group = new MidiGroup((byte)((channelListNumber - 1) / 16));
    var channel = new MidiChannel((byte)((channelListNumber - 1) % 16));

    ulong now = MidiClock.TimestampConstantSendImmediately;

    connection.SendSingleMessagePacket(MidiMessageBuilder.BuildMidi1ChannelVoiceMessage(
        now, group, Midi1ChannelVoiceMessageStatus.ControlChange, channel, 0, bankMsb));

    connection.SendSingleMessagePacket(MidiMessageBuilder.BuildMidi1ChannelVoiceMessage(
        now, group, Midi1ChannelVoiceMessageStatus.ControlChange, channel, 32, bankLsb));

    connection.SendSingleMessagePacket(MidiMessageBuilder.BuildMidi1ChannelVoiceMessage(
        now, group, Midi1ChannelVoiceMessageStatus.ProgramChange, channel, program, 0));
}
