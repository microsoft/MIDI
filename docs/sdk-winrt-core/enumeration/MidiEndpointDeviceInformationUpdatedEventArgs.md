---
layout: api_page
title: MidiEndpointDeviceInformationUpdatedEventArgs
parent: Endpoint Enumeration
grand_parent: Midi2 core
has_children: false
---

# MidiEndpointDeviceInformationUpdatedEventArgs

Represents a notification that endpoint properties have been updated

## Location

| Namespace | Microsoft.Windows.Devices.Midi2 |
| Library | Microsoft.Windows.Devices.Midi2 |

## Functions

| Property | Description |
| --------------- | ----------- |
| `EndpointDeviceId` | Id of the endpoint which has been updated  |
| `IsNameUpdated` | True if the name properties have been updated  |
| `IsEndpointInformationUpdated` | True if the in-protocol endpoint information has been updated |
| `IsDeviceIdentityUpdated` | True if the in-protocol device identity information has been updated |
| `IsStreamConfigurationUpdated` | True if protocol negotiation changed configuration of the endpoint |
| `AreFunctionBlocksUpdated` | True if any function blocks have been updated |
| `IsUserMetadataUpdated` | True if any user-supplied metadata fields have been updated |
| `AreAdditionalCapabilitiesUpdated` | True if the additional capabilities have been updated |
| `DeviceInformationUpdate` | The source `Windows.Devices.Enumeration.DeviceInformationUpdate` object. |

If none of the `UpdatedXX` properties are true, then other properties have been updated.

## IDL

[MidiEndpointDeviceInformationUpdatedEventArgs IDL](https://github.com/microsoft/MIDI/blob/main/src/app-sdk/winrt/MidiEndpointDeviceInformationUpdatedEventArgs.idl)

