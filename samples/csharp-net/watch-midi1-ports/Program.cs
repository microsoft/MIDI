// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// Further information: https://github.com/microsoft/MIDI/
// ============================================================================

// Windows MIDI Services sample code
//
// FOCUS OF THIS SAMPLE: watching the MIDI 1.0 port list.
//
// This is the one to reach for when you are porting an application which thinks
// in WinMM ports and you are not ready to restructure around endpoints and
// groups yet. It gives you the same port view your users already see in older
// applications, but with proper add and remove notifications instead of polling.
//
// Ports here are the MIDI 1.0 ports the service creates for compatibility. Each
// one maps to a group on a UMP endpoint. Once you are ready, moving to
// MidiEndpointDeviceWatcher gets you the endpoint the ports belong to, which is
// what lets you show a device rather than a scattering of unrelated port names.

using Windows.Devices.Midi2;
using Windows.Devices.Midi2.Enumeration;
using Windows.Devices.Midi2.Enumeration.Legacy;

if (!MidiApi.EnsureServiceAvailable())
{
    Console.WriteLine("Could not demand-start the MIDI service.");
    return 1;
}

MidiLegacyPortDeviceWatcher watcher = MidiLegacyPortDeviceWatcher.Create();

void AddedHandler(MidiLegacyPortDeviceWatcher sender, MidiLegacyPortDeviceInformationAddedEventArgs args)
{
    MidiLegacyPortDeviceInformation port = args.AddedDevice;

    Console.WriteLine($"[added]    {port.Name}");
    Console.WriteLine($"           flow: {port.Flow}, group: {port.Group.DisplayValue}");

    // Number is the WinMM port number this port currently has. It is the value
    // you would have passed to midiInOpen. Note "currently": it can change when
    // devices are added or removed, so do not persist it.
    Console.WriteLine($"           WinMM port number: {port.Number}");
    Console.WriteLine($"           owning endpoint:   {port.AssociatedEndpointDeviceId}");
}

void RemovedHandler(MidiLegacyPortDeviceWatcher sender, MidiLegacyPortDeviceInformationRemovedEventArgs args)
{
    Console.WriteLine($"[removed]  {args.RemovedDevice.Name}");
}

void UpdatedHandler(MidiLegacyPortDeviceWatcher sender, MidiLegacyPortDeviceInformationUpdatedEventArgs args)
{
    Console.WriteLine($"[updated]  {args.UpdatedDevice.Name}");
}

void EnumerationCompletedHandler(MidiLegacyPortDeviceWatcher sender, object args)
{
    Console.WriteLine();
    Console.WriteLine($"[enumeration completed] {sender.CountSourcePorts} source port(s), " +
        $"{sender.CountDestinationPorts} destination port(s).");
    Console.WriteLine();
}

// Wire up handlers before Start, or you will miss the initial additions.
watcher.Added += AddedHandler;
watcher.Removed += RemovedHandler;
watcher.Updated += UpdatedHandler;
watcher.EnumerationCompleted += EnumerationCompletedHandler;

Console.WriteLine("Watching MIDI 1.0 ports. Plug and unplug devices to see events.");
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
