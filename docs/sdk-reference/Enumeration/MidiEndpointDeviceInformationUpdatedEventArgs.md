---
layout: sdk_reference_page
title: MidiEndpointDeviceInformationUpdatedEventArgs
namespace: Windows.Devices.Midi2.Enumeration
type: runtimeclass
description: Argument supplied by the watcher when the properties of an endpoint have been updated.
---

The watcher passes this to your `Updated` handler when an endpoint's properties change. The flags say which groups of properties changed.

## Properties

| Property | Description |
| --------------- | ----------- |
| `UpdatedDevice` | The `MidiEndpointDeviceInformation` for the endpoint that changed, with the current property values |
| `DeviceInformationUpdate` | The `Windows.Devices.Enumeration.DeviceInformationUpdate` object this update came from |
| `IsNameUpdated` | True if any of the name properties changed |
| `IsEndpointInformationUpdated` | True if the endpoint information from discovery changed |
| `IsDeviceIdentityUpdated` | True if the device identity from discovery changed |
| `IsStreamConfigurationUpdated` | True if protocol negotiation changed the endpoint's stream configuration |
| `AreFunctionBlocksUpdated` | True if any function blocks changed |
| `IsUserMetadataUpdated` | True if any of the information the user supplied changed |
| `AreAdditionalCapabilitiesUpdated` | True if the additional capabilities changed |
| `AreUniqueIdsUpdated` | True if any of the unique id properties changed |
| `AreGroupTerminalBlocksUpdated` | True if any group terminal blocks changed |
| `IsMutedStateUpdated` | True if the endpoint was muted or unmuted |
| `IsEndpointDiscoveryStateUpdated` | True if `MidiEndpointDeviceInformation.IsEndpointDiscoveryComplete` changed |
| `IsMidi1PortMappingUpdated` | True if the MIDI 1.0 port name table or naming approach for this endpoint changed. The endpoint's MIDI 1.0 ports usually change at the same time |
| `IsDevicePresenceUpdated` | True if the device interface was turned on or off, or the device arrived or was removed |
| `AreLatencyPropertiesUpdated` | True if the calculated or user-supplied outgoing latency values changed |
| `AreTransportSuppliedPropertiesUpdated` | True if anything returned by `GetTransportSuppliedInfo()` changed, or any property that belongs to the transport, such as the network remote host |
| `AreSystemDevicePropertiesUpdated` | True if a Windows Plug and Play property, such as the parent, the manufacturer, or the interface class, changed |

## Reacting to updates

Every property the watcher asks for belongs to at least one group above, so at least one flag is always true. Don't write code that treats "no flag set" as something that can happen.

The groups overlap on purpose, because one property can matter to more than one group. For example, a custom endpoint name sets both `IsNameUpdated` and `IsUserMetadataUpdated`.

A device coming online causes a series of updates, not just one, because the service writes properties as the information arrives from the device. Treat each update as "read again what you care about," not as "this is the final state." See [Endpoint arrival and update ordering]({{ site.baseurl }}/kb/endpoint-arrival-and-update-ordering/).
