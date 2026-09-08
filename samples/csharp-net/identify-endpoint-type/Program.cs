// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// Further information: https://github.com/microsoft/MIDI/
// ============================================================================

// Windows MIDI Services sample code
//
// FOCUS OF THIS SAMPLE: working out what kind of thing an endpoint actually is.
//
// Under WinMM there was no answer to this. midiInGetDevCaps gave you a name and
// a technology field which said almost nothing, so applications guessed from the
// name, which is why so many of them break when a user renames a device.
//
// Here, every endpoint records which transport created it. Ask the endpoint for
// its transport id, then match that against the list of installed transports to
// get a name and description you can show a user.
//
// The short TransportCode ("KS", "BLE", "NET", "DIAG", and so on) is the stable
// thing to branch on in code. Name and Description are display strings.

using Windows.Devices.Midi2;
using Windows.Devices.Midi2.Enumeration;
using Windows.Devices.Midi2.Reporting;

if (!MidiApi.EnsureServiceAvailable())
{
    Console.WriteLine("Could not demand-start the MIDI service.");
    return 1;
}

// The set of installed transports. This is small and changes only when the
// service configuration changes, so reading it once is fine.
IReadOnlyList<MidiServiceTransportPluginInfo> transports =
    MidiReporting.GetInstalledTransportPlugins();

Console.WriteLine($"{transports.Count} transport(s) installed:");

foreach (MidiServiceTransportPluginInfo transport in transports)
{
    Console.WriteLine($"  {transport.TransportCode} - {transport.Name}");
}

Console.WriteLine();

IReadOnlyList<MidiEndpointDeviceInformation> endpoints =
    MidiEndpointDeviceInformation.FindAll(
        MidiEndpointDeviceInformationSortOrder.Name,
        MidiEndpointDeviceInformationFilters.AllStandardEndpoints);

Console.WriteLine($"{endpoints.Count} endpoint(s):");
Console.WriteLine();

foreach (MidiEndpointDeviceInformation endpoint in endpoints)
{
    MidiEndpointTransportSuppliedInfo transportInfo = endpoint.GetTransportSuppliedInfo();

    Console.WriteLine(endpoint.Name);

    // The transport list is short, so a straight search is clearer here than
    // building a dictionary keyed on the GUID.
    MidiServiceTransportPluginInfo? matchingTransport = null;

    foreach (MidiServiceTransportPluginInfo transport in transports)
    {
        if (transport.TransportId == transportInfo.TransportId)
        {
            matchingTransport = transport;
            break;
        }
    }

    if (matchingTransport != null)
    {
        Console.WriteLine($"  Transport:     {matchingTransport.Name} ({matchingTransport.TransportCode})");
        Console.WriteLine($"  Description:   {matchingTransport.Description}");
    }
    else
    {
        // An endpoint can outlive the transport which created it, for example if
        // a transport was uninstalled while the endpoint record remains.
        Console.WriteLine($"  Transport:     {transportInfo.TransportCode} (not currently installed)");
    }

    // Worth showing alongside the transport, because these two are what most
    // applications actually want to branch on.
    string nativeFormat =
        transportInfo.NativeDataFormat == MidiEndpointNativeDataFormat.UniversalMidiPacketFormat
            ? "UMP"
            : "MIDI 1.0 bytestream";

    Console.WriteLine($"  Native format: {nativeFormat}");
    Console.WriteLine($"  Multi-client:  {(transportInfo.SupportsMultiClient ? "yes" : "no")}");
    Console.WriteLine();
}

return 0;
