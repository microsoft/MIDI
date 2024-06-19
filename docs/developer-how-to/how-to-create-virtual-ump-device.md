---
layout: page
title: Create Virtual Devices
parent: Developer How-to
has_children: false
---

# How to Create Virtual UMP Devices at Runtime

If you develop an application which should appear as a new MIDI device to other applications on Windows, you want to create a Virtual UMP Device. Your app may be a controller app, a sound generator/synthesizer, or a bridge to accessibility or other controllers. Anything a hardware MIDI device can do is open to you here.

## Steps to Create a Virtual Device

1. Create a MIDI session
2. Define the Virtual MIDI Device, its function blocks, and other properties
3. Create the Device and get the `EndpointDeviceId` for the device application
4. Connect to the Device like any other MIDI connection
5. Wire up event handlers for message received and optionally for protocol negotiation
6. Open the connection
7. Respond to any protocol negotiation or message received events
8. When the application no longer needs to expose the virtual device, close the connection.

On the service-side, the Virtual Device works like any other native UMP MIDI 2.0 device, including for endpoint metadata capture and protocol negotiation.

More details available in the Endpoints documentation.

### Example

TODO




## Sample Code

* [C# Sample](https://github.com/microsoft/MIDI/tree/main/samples/csharp-net/app-to-app-midi-cs)


> Note: Virtual UMP devices are not currently visible to the WinMM MIDI 1.0 API (the API used by most MIDI 1.0 apps on Windows). There are enumeration complexities with that API when devices are added and removed at runtime, which is part of why we needed to create a new API anyway. It's possible these devices will never be visibile to WinMM MIDI 1.0. For full functionality, we recommend using the new Windows MIDI Services SDK.
