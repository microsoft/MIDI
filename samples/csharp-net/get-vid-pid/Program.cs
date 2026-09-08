// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// Further information: https://github.com/microsoft/MIDI/
// ============================================================================

// Windows MIDI Services sample code
//
// FOCUS OF THIS SAMPLE: getting the USB VID and PID for an endpoint.
//
// Under WinMM you would have sent a DRV_QUERYDEVICEINTERFACE message to the
// driver and then parsed the device interface string yourself. Here, the
// identifiers are already parsed and waiting for you as properties.
//
// There are two different places to look, and they do not mean the same thing:
//
//   MidiParentDeviceInformation       - the USB VID/PID of the physical parent
//                                       device, parsed from the device instance
//                                       id. Usually what you are looking for.
//
//   MidiEndpointTransportSuppliedInfo - VendorId/ProductId as supplied by the
//                                       transport. Populated for endpoints on
//                                       the UMP USB driver.
//
// Endpoints which are not USB devices have no VID/PID at all. Those properties
// are ushort, not nullable, so zero means "not applicable" rather than
// "vendor zero".

using Windows.Devices.Midi2;
using Windows.Devices.Midi2.Enumeration;

// VID and PID are conventionally displayed as four hex digits.
static string FormatUsbId(ushort id)
{
    if (id == 0)
    {
        return "(not reported)";
    }

    return "0x" + id.ToString("X4");
}

if (!MidiApi.EnsureServiceAvailable())
{
    Console.WriteLine("Could not demand-start the MIDI service.");
    return 1;
}

IReadOnlyList<MidiEndpointDeviceInformation> endpoints =
    MidiEndpointDeviceInformation.FindAll(
        MidiEndpointDeviceInformationSortOrder.Name,
        MidiEndpointDeviceInformationFilters.AllStandardEndpoints);

Console.WriteLine($"{endpoints.Count} endpoint(s) found.");
Console.WriteLine();

foreach (MidiEndpointDeviceInformation endpoint in endpoints)
{
    Console.WriteLine(endpoint.Name);

    // The parent is the physical device the endpoint belongs to. It is null for
    // endpoints with no parent device, such as app-to-app MIDI.
    MidiParentDeviceInformation parent = endpoint.GetParentDeviceInformation();

    if (parent != null)
    {
        // EnumeratorName tells you what kind of bus this is. "USB" means the VID
        // and PID below are meaningful.
        Console.WriteLine($"  Enumerator:    {parent.EnumeratorName}");
        Console.WriteLine($"  Parent name:   {parent.Name}");
        Console.WriteLine($"  USB VID:       {FormatUsbId(parent.UsbVendorId)}");
        Console.WriteLine($"  USB PID:       {FormatUsbId(parent.UsbProductId)}");

        string serialNumber = parent.UsbSerialNumber;

        if (!string.IsNullOrEmpty(serialNumber))
        {
            Console.WriteLine($"  USB serial:    {serialNumber}");
        }
    }
    else
    {
        Console.WriteLine("  No parent device. This endpoint is not backed by physical hardware.");
    }

    // The transport may also report the identifiers. For a device on the UMP USB
    // driver these come from the USB descriptors rather than from the device
    // instance id, so they are worth checking when the parent has none.
    MidiEndpointTransportSuppliedInfo transportInfo = endpoint.GetTransportSuppliedInfo();

    Console.WriteLine($"  Transport:     {transportInfo.TransportCode}");
    Console.WriteLine($"  Transport VID: {FormatUsbId(transportInfo.VendorId)}");
    Console.WriteLine($"  Transport PID: {FormatUsbId(transportInfo.ProductId)}");

    Console.WriteLine();
}

return 0;
