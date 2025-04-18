---
layout: kb
title: Understanding Endpoint Device Ids
audience: everyone
description: General information about what makes up a Endpoint Device Id
---

The Endpoint Device Id (also referred to as a Device Interface Id) is the way we identify individual devices and interfaces in Windows. 

Example for one of the built-in loopback endpoints: 

`\\?\swd#midisrv#midiu_diag_loopback_a#{e7cce071-3c03-423f-88d3-f1045d02552b}`

| Part | Description |
| ----- | -- |
| `swd` | Software device. This is any device that is not a physical device connected to the PC, and which is created using the Software Device APIs. All MIDI endpoints are software devices and may or may not have a physical connected device as a parent. |
| `midisrv` | The name of the enumerator. For Windows MIDI Services, this is the MidiSrv Windows Service. For older APIs, this is `mmdevapi` |
| `midi` or `midiu` | Indicates a MIDI interface. |
| `diag` | Abbreviation for the transport which created this device interface. |
| `loopback_a` | Arbitrary unique identification string provided by the transport. Typically includes a unique identifier like a serial number. It may also contain other information like the pin pairs used to provide the bidirectional communication. |
| `midiu_diag_loopback_a` | The entire string here is controlled by the transport. By convention it breaks down into the fields mentioned above, but that is not something you should count on. In general, parsing these strings is not recommended. When you want to use just this part of the Id, use the `MidiEndpointDeviceId` helper class|
| `GUID` | The interface Id. For Windows MIDI Services, every interface is a bidirectional interface, even if the connected device is MIDI 1.0 with a single unidirectional interface. For MIDI 1.0 devices, you can look at the group terminal blocks to identify active groups/directions. For MIDI 2.0 devices, you can look at the function blocks for the same information and more.|

By convention, Windows MIDI Services presents the Endpoint Device Id in all lowercase.

If you look at the device in Device Manager, and look at Details/Device Instance Path, you'll see all of the information here except for the interface Id. When you enumerate devices through `Windows.Devices.Enumeration` or through Windows MIDI Services, the interface Id is included and required.

> <h4>Tip:</h4> 
> We don't recommend parsing these strings. If there's information you need about the device which is not contained in the enumerated properties, please let us know and we'll look into whether or not we can create a custom property to hold that.
