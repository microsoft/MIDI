// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// Further information: https://github.com/microsoft/MIDI/
// ============================================================================

// Windows MIDI Services sample code
//
// FOCUS OF THIS SAMPLE: creating a MIDI 1.0-style loopback endpoint at runtime.
//
// This is the simpler cousin of the loopback-endpoints sample. Instead of a pair
// of MIDI 2.0 endpoints wired to each other, this creates a single endpoint which
// behaves the way a MIDI 1.0 virtual cable does: what goes in comes back out,
// and it shows up in the MIDI 1.0 port list that older applications see.
//
// Use this one when you want something an existing WinMM or WinRT MIDI 1.0
// application can talk to without any changes.
//
// "Transient" means it lives only while the service is running. Remove what you
// create; if this sample is killed first, the endpoint stays until the service
// restarts and recreating it with the same unique id will fail.

using Windows.Devices.Midi2;
using Windows.Devices.Midi2.Transports.BasicLoopback;

if (!MidiApi.EnsureServiceAvailable())
{
    Console.WriteLine("Could not demand-start the MIDI service.");
    return 1;
}

if (!MidiBasicLoopbackManager.IsTransportAvailable)
{
    Console.WriteLine("The basic loopback transport is not available on this PC.");
    return 1;
}

MidiBasicLoopbackEndpointDefinition definition = new MidiBasicLoopbackEndpointDefinition();
definition.Name = "Sample App Basic Loopback";
definition.Description = "Created by the C# loopback-basic-endpoints sample.";

// If you leave the unique id blank, one is generated for you.
definition.UniqueId = "cs-sample-basic-loopback";

MidiBasicLoopbackCreationConfig creationConfig = new MidiBasicLoopbackCreationConfig(definition);

MidiBasicLoopbackCreationResponse response =
    MidiBasicLoopbackManager.CreateTransientLoopback(creationConfig);

if (!response.Success)
{
    Console.WriteLine("Failed to create the loopback endpoint.");
    Console.WriteLine($"Error code:    {response.ErrorCode}");
    Console.WriteLine($"Error message: {response.ErrorMessage}");
    Console.WriteLine();
    Console.WriteLine("This happens if a previous run was killed before removing it.");
    Console.WriteLine("Restart the MIDI service, or change the unique id above.");
    return 1;
}

Console.WriteLine("Endpoint created.");
Console.WriteLine($"  {definition.Name}");
Console.WriteLine($"  {response.CreatedLoopbackEntry.EndpointDeviceId}");
Console.WriteLine();

Guid associationId = response.CreatedLoopbackEntry.AssociationId;

Console.WriteLine("This endpoint is now visible to MIDI applications, including older");
Console.WriteLine("WinMM and WinRT MIDI 1.0 applications, as a MIDI 1.0 port.");
Console.WriteLine();
Console.WriteLine("Press Enter to remove it.");
Console.ReadLine();

MidiBasicLoopbackRemovalConfig removalConfig = new MidiBasicLoopbackRemovalConfig(associationId);

MidiBasicLoopbackRemovalResponse removalResponse =
    MidiBasicLoopbackManager.RemoveTransientLoopback(removalConfig);

if (removalResponse.Success)
{
    Console.WriteLine("Endpoint removed.");
}
else
{
    Console.WriteLine("Failed to remove the endpoint.");
    Console.WriteLine($"Error code:    {removalResponse.ErrorCode}");
    Console.WriteLine($"Error message: {removalResponse.ErrorMessage}");
}

return 0;
