// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// Further information: https://github.com/microsoft/MIDI/
// ============================================================================

// Windows MIDI Services sample code
//
// FOCUS OF THIS SAMPLE: getting a one-time list of endpoints and their metadata.
//
// This is the closest equivalent to calling midiInGetNumDevs and then
// midiInGetDevCaps for each index. It is fine for a one-shot listing like this,
// but an application with a device picker should use MidiEndpointDeviceWatcher
// instead. See the watch-endpoints sample for why.

using Windows.Devices.Midi2;
using Windows.Devices.Midi2.Enumeration;

if (!MidiApi.EnsureServiceAvailable())
{
    Console.WriteLine("Could not demand-start the MIDI service.");
    return 1;
}

ulong enumerationStartTime = MidiClock.Now;

// AllStandardEndpoints is what an application which is not a diagnostic tool
// should show a musician. The diagnostic loopbacks are excluded by default; we
// add them here only so the sample shows something on a PC with no hardware.
IReadOnlyList<MidiEndpointDeviceInformation> endpoints =
    MidiEndpointDeviceInformation.FindAll(
        MidiEndpointDeviceInformationSortOrder.Name,
        MidiEndpointDeviceInformationFilters.AllStandardEndpoints |
        MidiEndpointDeviceInformationFilters.DiagnosticLoopback);

ulong enumerationDuration = MidiClock.Now - enumerationStartTime;

Console.WriteLine($"Enumeration took {MidiClock.ConvertTimestampTicksToMilliseconds(enumerationDuration)} milliseconds.");
Console.WriteLine($"{endpoints.Count} endpoint(s) returned.");
Console.WriteLine();

foreach (MidiEndpointDeviceInformation endpoint in endpoints)
{
    Console.WriteLine(endpoint.Name);

    // The id is what you store. The name is display metadata and users can
    // change it, so persisting the name will break when they do.
    Console.WriteLine($"  Id:            {endpoint.EndpointDeviceId}");

    MidiEndpointTransportSuppliedInfo transportInfo = endpoint.GetTransportSuppliedInfo();

    Console.WriteLine($"  Transport:     {transportInfo.TransportCode}");
    Console.WriteLine($"  Multi-client:  {(transportInfo.SupportsMultiClient ? "yes" : "no")}");

    // Function blocks supersede group terminal blocks. When a device declares
    // function blocks, use those and ignore the group terminal blocks, rather
    // than showing the union of the two. See the porting guidance for why.
    IReadOnlyList<MidiFunctionBlock> functionBlocks = endpoint.GetDeclaredFunctionBlocks();

    if (functionBlocks.Count > 0)
    {
        Console.WriteLine($"  Function blocks ({functionBlocks.Count}):");

        foreach (MidiFunctionBlock functionBlock in functionBlocks)
        {
            Console.WriteLine($"    {functionBlock.Number}: {functionBlock.Name} " +
                $"(first group {functionBlock.FirstGroup.DisplayValue}, {functionBlock.GroupCount} group(s))");
        }
    }
    else
    {
        IReadOnlyList<MidiGroupTerminalBlock> groupTerminalBlocks = endpoint.GetGroupTerminalBlocks();

        if (groupTerminalBlocks.Count > 0)
        {
            Console.WriteLine($"  Group terminal blocks ({groupTerminalBlocks.Count}):");

            foreach (MidiGroupTerminalBlock groupTerminalBlock in groupTerminalBlocks)
            {
                Console.WriteLine($"    {groupTerminalBlock.Number}: {groupTerminalBlock.Name} " +
                    $"(first group {groupTerminalBlock.FirstGroup.DisplayValue}, {groupTerminalBlock.GroupCount} group(s))");
            }
        }
    }

    Console.WriteLine();
}

return 0;
