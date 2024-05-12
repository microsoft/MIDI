---
layout: api_page
title: MidiEndpointDeviceInformationUpdateEventArgs
parent: Endpoint Enumeration
grand_parent: Microsoft.Devices.Midi2
has_children: false
---

# MidiEndpointDeviceInformationUpdateEventArgs

Represents a notification that endpoint properties have been updated

## Functions

| Property | Description |
| --------------- | ----------- |
| `Id` | Id of the endpoint which has been updated  |
| `UpdatedName` | True if the name properties have been updated  |
| `UpdatedEndpointInformation` | True if the in-protocol endpoint information has been updated |
| `UpdatedDeviceIdentity` | True if the in-protocol device identity information has been updated |
| `UpdatedStreamConfiguration` | True if protocol negotiation changed configuration of the endpoint |
| `UpdatedFunctionBlocks` | True if any function blocks have been updated |
| `UpdatedUserMetadata` | True if any user-supplied metadata fields have been updated |
| `UpdatedAdditionalCapabilities` | True if the additional capabilities have been updated |
| `DeviceInformationUpdate` | The source `Windows.Devices.Enumeration.DeviceInformationUpdate` object. |

If none of the `UpdatedXX` properties are true, then other properties have been updated.

## IDL

[MidiEndpointDeviceInformationUpdateEventArgs IDL](https://github.com/microsoft/MIDI/blob/main/src/api/Client/Midi2Client/MidiEndpointDeviceInformationUpdateEventArgs.idl)

