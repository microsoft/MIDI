// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// Further information: https://github.com/microsoft/MIDI/
// ============================================================================

// Windows MIDI Services sample code
//
// FOCUS OF THIS SAMPLE: creating a pair of MIDI 2.0 loopback endpoints at runtime.
//
// A loopback is two endpoints wired back to back: anything sent to A arrives at
// B, and the other way round. There was no WinMM equivalent, which is why so
// many Windows musicians ended up installing a third-party virtual cable driver.
//
// "Transient" means the endpoints live only as long as the service is running
// and are not written to the configuration file. If you want loopbacks which
// survive a reboot, the user creates those in the MIDI Settings app.
//
// Remove what you create. If this sample is killed before it gets to the removal
// step, the endpoints stay until the service restarts, and creating them again
// with the same unique ids will fail.

using Windows.Devices.Midi2;
using Windows.Devices.Midi2.Transports.Loopback;

if (!MidiApi.EnsureServiceAvailable())
{
    Console.WriteLine("Could not demand-start the MIDI service.");
    return 1;
}

if (!MidiLoopbackManager.IsTransportAvailable)
{
    Console.WriteLine("The loopback transport is not available on this PC.");
    return 1;
}

MidiLoopbackEndpointDefinition definitionA = new MidiLoopbackEndpointDefinition();
definitionA.Name = "Sample App Loopback A";
definitionA.Description = "Created by the C# loopback-endpoints sample.";
definitionA.UniqueId = "cs-sample-loopback-a";

MidiLoopbackEndpointDefinition definitionB = new MidiLoopbackEndpointDefinition();
definitionB.Name = "Sample App Loopback B";
definitionB.Description = "Created by the C# loopback-endpoints sample.";
definitionB.UniqueId = "cs-sample-loopback-b";

MidiLoopbackCreationConfig creationConfig =
    new MidiLoopbackCreationConfig(definitionA, definitionB);

MidiLoopbackCreationResponse response = MidiLoopbackManager.CreateTransientLoopback(creationConfig);

if (!response.Success)
{
    Console.WriteLine("Failed to create the loopback endpoints.");
    Console.WriteLine("This happens if a previous run was killed before removing them.");
    Console.WriteLine("Restart the MIDI service, or change the unique ids above.");
    return 1;
}

Console.WriteLine("Endpoints created.");
Console.WriteLine();
Console.WriteLine("Loopback endpoint A:");
Console.WriteLine($"  {response.CreatedLoopbackEntry.EndpointA.Name}");
Console.WriteLine($"  {response.CreatedLoopbackEntry.EndpointA.EndpointDeviceId}");
Console.WriteLine();
Console.WriteLine("Loopback endpoint B:");
Console.WriteLine($"  {response.CreatedLoopbackEntry.EndpointB.Name}");
Console.WriteLine($"  {response.CreatedLoopbackEntry.EndpointB.EndpointDeviceId}");
Console.WriteLine();

Guid associationId = response.CreatedLoopbackEntry.AssociationId;

Console.WriteLine("These endpoints are now visible to any MIDI application on this PC.");
Console.WriteLine("Press Enter to remove them.");
Console.ReadLine();

MidiLoopbackRemovalConfig removalConfig = new MidiLoopbackRemovalConfig(associationId);

MidiLoopbackRemovalResponse removalResponse =
    MidiLoopbackManager.RemoveTransientLoopback(removalConfig);

Console.WriteLine(removalResponse.Success ? "Endpoints removed." : "Failed to remove the endpoints.");

return 0;
