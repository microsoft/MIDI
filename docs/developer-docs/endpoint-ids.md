---
layout: page
title: Endpoint Device Ids
parent: Windows Midi Services
has_children: false
---

# Endpoint Device Ids

The Endpoint Device Id (also referred to as a Device Id) is the way we identify individual devices and interfaces in Windows. 

Example for one of the built-in loopback endpoints: 

`\\?\SWD#MIDISRV#MIDIU_DIAG_LOOPBACK_A#{e7cce071-3c03-423f-88d3-f1045d02552b}`

| Part | Description |
| ----- | -- |
| `SWD` | Software device. This is any device that is not a physical device connected to the PC, and which is created using the Software Device APIs. All MIDI endpoints are software devices and may or may not have a physical connected device as a parent. |
| `MIDISRV` | The name of the enumerator. For Windows MIDI Services, this is the MidiSrv Windows Service |
| `MIDIU` | Indicates a MIDI UMP interface. |
| `DIAG` | Mnemonic for the transport which created this device interface. |
| `LOOPBACK_A` | Arbitrary unique identification string provided by the transport. Typically includes a unique identifier like a serial number. It may also contain other information like the pin pairs used to provide the bidirectional communication. |
| `MIDIU_DIAG_LOOPBACK_A` | The entire string here is controlled by the transport. By convention it breaks down into the fields mentioned above, but that is not something you should count on. In general, parsing these strings is not recommended. |
| `GUID` | The interface Id. For Windows MIDI Services, every interface is a bidirectional interface, even if the connected device is MIDI 1.0 with a single unidirectional interface. For MIDI 1.0 devices, you can look at the group terminal blocks to identify active groups/directions. For MIDI 2.0 devices, you can look at the function blocks for the same information and more.|


If you look at the device in Device Manager, and look at Details/Device Instance Path, you'll see all of the information here except for the interface Id. When you enumerate devices through `Windows::Devices::Enumeration` or through Windows MIDI Services, the interface Id is included and required.

> Tip: Although it was required in the past, we don't recommend parsing these strings. If there's information you need about the device which is not contained in the enumerated properties, please let us know and we'll look into whether or not we can create a custom property to hold that.
