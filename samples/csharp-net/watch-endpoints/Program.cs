// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// Further information: https://github.com/microsoft/MIDI/
// ============================================================================

// Windows MIDI Services sample code
//
// FOCUS OF THIS SAMPLE: reacting to endpoints arriving, leaving and changing.
//
// WinMM had no notification at all, so applications polled midiInGetNumDevs on a
// timer. Do not port that habit. Use the watcher.
//
// Updated is the event people forget, and it matters more here than it did with
// the older APIs. A MIDI 2.0 endpoint answers discovery after it first appears,
// so its name, function blocks and protocol can all change moments later. An
// application which builds its device list once and never handles Updated will
// keep showing whatever the endpoint looked like before the device finished
// telling us what it is.
//
// There is also no point at which endpoint information is guaranteed final. The
// service's discovery timeout only bounds how long it waits for a device which
// never answers; a device can still send more later. So handle Updated for the
// whole lifetime of the watcher, not just for a window after startup.

using Windows.Devices.Midi2;
using Windows.Devices.Midi2.Enumeration;

if (!MidiApi.EnsureServiceAvailable())
{
    Console.WriteLine("Could not demand-start the MIDI service.");
    return 1;
}

MidiEndpointDeviceWatcher watcher =
    MidiEndpointDeviceWatcher.Create(MidiEndpointDeviceInformationFilters.AllStandardEndpoints);

void AddedHandler(MidiEndpointDeviceWatcher sender, MidiEndpointDeviceInformationAddedEventArgs args)
{
    Console.WriteLine($"[added]    {args.AddedDevice.Name}");
    Console.WriteLine($"           {args.AddedDevice.EndpointDeviceId}");
}

void RemovedHandler(MidiEndpointDeviceWatcher sender, MidiEndpointDeviceInformationRemovedEventArgs args)
{
    Console.WriteLine($"[removed]  {args.RemovedDevice.Name}");
    Console.WriteLine($"           {args.RemovedDevice.EndpointDeviceId}");
}

void UpdatedHandler(MidiEndpointDeviceWatcher sender, MidiEndpointDeviceInformationUpdatedEventArgs args)
{
    Console.WriteLine($"[updated]  {args.UpdatedDevice.Name}");

    // The args tell you what changed, so you can rebuild only what you need. At least one of
    // these is always set, so there is no "something else changed" case to write code for.
    //
    // A device coming online produces a sequence of these rather than one. Treat each as
    // "re-read what I care about", not as "this is the final state".
    if (args.IsNameUpdated)
    {
        Console.WriteLine("           name changed");
    }

    if (args.AreFunctionBlocksUpdated)
    {
        // This is the one which should make you rebuild your port list, because
        // the group layout the user sees comes from the function blocks.
        Console.WriteLine("           function blocks changed");

        // Function block names arrive as separate messages from the block information, so a
        // block can legitimately be reported here before its name has been received. Take the
        // name on a later update rather than caching the blank.
        foreach (var block in args.UpdatedDevice.GetDeclaredFunctionBlocks())
        {
            var name = string.IsNullOrEmpty(block.Name) ? "(name not received yet)" : block.Name;
            Console.WriteLine($"             block {block.Number}: {name}");
        }
    }

    if (args.IsEndpointInformationUpdated)
    {
        Console.WriteLine("           endpoint information changed");
    }

    if (args.IsEndpointDiscoveryStateUpdated)
    {
        // True once the service has finished gathering in-protocol information, or gave up
        // waiting. It is a hint, not a barrier: more updates can still follow, and it stays
        // false if discovery was abandoned because the device went away. Do not block on it.
        Console.WriteLine($"           discovery complete: {args.UpdatedDevice.IsEndpointDiscoveryComplete}");
    }

    if (args.IsMidi1PortMappingUpdated)
    {
        // The MIDI 1.0 ports for this endpoint have been rebuilt. Do not enumerate them from
        // here: the ports are separate device interfaces whose arrival is not ordered against
        // this event. Use MidiLegacyPortDeviceWatcher, as in the watch-midi1-ports sample.
        Console.WriteLine("           MIDI 1.0 port mapping changed");
    }

    if (args.IsDevicePresenceUpdated)
    {
        Console.WriteLine("           device presence changed");
    }
}

void EnumerationCompletedHandler(MidiEndpointDeviceWatcher sender, object args)
{
    // This is the moment to hand a first complete list to your user interface.
    // It is not the moment to stop listening.
    Console.WriteLine();
    Console.WriteLine($"[enumeration completed] {sender.EnumeratedEndpointDevices.Count} endpoint(s) known so far.");
    Console.WriteLine();
}

// Wire up handlers before calling Start, or you will miss the initial additions.
watcher.Added += AddedHandler;
watcher.Removed += RemovedHandler;
watcher.Updated += UpdatedHandler;
watcher.EnumerationCompleted += EnumerationCompletedHandler;

Console.WriteLine("Starting watcher. Plug and unplug devices to see events.");
Console.WriteLine("Press Enter to stop.");
Console.WriteLine();

watcher.Start();

Console.ReadLine();

watcher.Stop();

watcher.Added -= AddedHandler;
watcher.Removed -= RemovedHandler;
watcher.Updated -= UpdatedHandler;
watcher.EnumerationCompleted -= EnumerationCompletedHandler;

return 0;
