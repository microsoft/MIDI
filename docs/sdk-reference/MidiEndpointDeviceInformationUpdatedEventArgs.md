---
layout: sdk_reference_page
title: MidiEndpointDeviceInformationUpdatedEventArgs
namespace: Microsoft.Windows.Devices.Midi2
library: Microsoft.Windows.Devices.Midi2.dll
type: runtimeclass
description: Argument supplied by the watcher when the properties of an endpoint have been updated.
---

Represents a notification that endpoint properties have been updated

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
