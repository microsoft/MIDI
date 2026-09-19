---
layout: sdk_reference_page
title: MidiEndpointDeviceInformationUpdatedEventArgs
namespace: Windows.Devices.Midi2.Enumeration
type: runtimeclass
description: Argument supplied by the watcher when the properties of an endpoint have been updated.
---

Represents a notification that endpoint properties have been updated

## Properties

| Property | Description |
| --------------- | ----------- |
| `UpdatedDevice` | The `MidiEndpointDeviceInformation` for the endpoint which was updated, carrying the current property values |
| `DeviceInformationUpdate` | The source `Windows.Devices.Enumeration.DeviceInformationUpdate` object. |
| `IsNameUpdated` | True if the name properties have been updated  |
| `IsEndpointInformationUpdated` | True if the in-protocol endpoint information has been updated |
| `IsDeviceIdentityUpdated` | True if the in-protocol device identity information has been updated |
| `IsStreamConfigurationUpdated` | True if protocol negotiation changed configuration of the endpoint |
| `AreFunctionBlocksUpdated` | True if any function blocks have been updated |
| `IsUserMetadataUpdated` | True if any user-supplied metadata fields have been updated |
| `AreAdditionalCapabilitiesUpdated` | True if the additional capabilities have been updated |
| `AreUniqueIdsUpdated` | True if any unique identifier properties have been updated |
| `AreGroupTerminalBlocksUpdated` | True if any group terminal blocks have been updated |
| `IsMutedStateUpdated` | True if the muted state of the endpoint has been updated |
| `IsEndpointDiscoveryStateUpdated` | True if `MidiEndpointDeviceInformation.IsEndpointDiscoveryComplete` has changed |
| `IsMidi1PortMappingUpdated` | True if the MIDI 1.0 port name table or naming approach for this endpoint has changed. The set of MIDI 1.0 ports for the endpoint typically changes at the same time |
| `IsDevicePresenceUpdated` | True if the device interface was enabled or disabled, or the device arrived or was removed |
| `AreLatencyPropertiesUpdated` | True if the calculated or user-supplied outgoing latency values have changed |
| `AreTransportSuppliedPropertiesUpdated` | True if anything returned by `GetTransportSuppliedInfo()`, or any transport-specific property such as the network remote host, has changed |
| `AreSystemDevicePropertiesUpdated` | True if a Windows PnP property such as the parent, the manufacturer or the interface class changed |

## Reacting to updates

Every property the watcher requests belongs to at least one group above, so at least one of these is always true. Do not write code which treats "no flag set" as a meaningful state.

Groups deliberately overlap, because a single property can be relevant to more than one of them. A custom endpoint name, for example, sets both `IsNameUpdated` and `IsUserMetadataUpdated`.

A device coming online produces a sequence of updates rather than a single one, because the service writes properties as the information arrives from the device. Treat each update as "re-read what you care about", not as "this is the final state". See [Endpoint arrival and update ordering]({{ site.baseurl }}/kb/endpoint-arrival-and-update-ordering/).
