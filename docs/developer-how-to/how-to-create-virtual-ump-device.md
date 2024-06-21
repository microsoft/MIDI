---
layout: page
title: Create Virtual Devices
parent: Developer How-to
has_children: false
---

# How to Create Virtual UMP Devices at Runtime

If you develop an application which should appear as a new MIDI device to other applications on Windows, you want to create a Virtual UMP Device. Your app may be a controller app, a sound generator/synthesizer, or a bridge to accessibility or other controllers. Anything a hardware MIDI device can do is open to you here.

## Steps to Create a Virtual Device

1. [Check for and bootstrap Windows MIDI Services](./how-to-check-for-windows-midi-services.md)
2. Create a MIDI session
3. Define the Virtual MIDI Device, its function blocks, and other properties
4. Create the Device and get the `EndpointDeviceId` for the device application
5. Connect to the Device like any other MIDI connection
6. Wire up event handlers for message received and optionally for protocol negotiation
7. Open the connection
8. Respond to any protocol negotiation or message received events
9. When the application no longer needs to expose the virtual device, close the connection.

On the service-side, the Virtual Device works like any other native UMP MIDI 2.0 device, including for endpoint metadata capture and protocol negotiation.

More details available in the Endpoints documentation.

## Code

We'll assume you've already seen how to get to the point of being able to open MIDI connections within a session.

```cpp
```


## Compatibility

Virtual UMP devices are not currently visible to the WinMM MIDI 1.0 API (the API used by most MIDI 1.0 apps on Windows). There are enumeration complexities with that API when devices are added and removed at runtime, which is part of why we needed to create a new API anyway. It's possible these devices will never be visibile to WinMM MIDI 1.0, but we are investigating options. For full functionality, we recommend using the Windows MIDI Services SDK.

## Sample Code

* [C# WinUI Sample](https://github.com/microsoft/MIDI/tree/main/samples/csharp-net/virtual-device-app-winui)
* [C# WPF Sample (in-progress)](https://github.com/microsoft/MIDI/tree/main/samples/csharp-net/virtual-device-app-wpf)
