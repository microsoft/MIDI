---
layout: sdk_reference_page
title: MidiNetworkHostCreationConfig
namespace: Windows.Devices.Midi2.Transports.Network
type: runtimeclass
implements: Windows.Devices.Midi2.ServiceConfig.IMidiServiceTransportPluginConfig
description: Config sent to the service to create a Network MIDI 2.0 host
---

Describes a host for remote clients to connect to. Pass to `MidiNetworkTransportManager.CreateNetworkHostAsync`.

## Constructors

| Constructor | Description |
| -------- | ----------- |
| `MidiNetworkHostCreationConfig()` | Create an empty config |

## Static Functions

| Function | Description |
| -------- | ----------- |
| `CreateDefault()` | Create a configuration pre-populated with sensible defaults, including a name derived from the computer name. |
| `EnsureCompliantServiceInstanceName(serviceInstanceName)` | Returns a version of the supplied service instance name which is valid for use as an mDNS service instance name. |

## Properties

| Property | Description |
| -------- | ----------- |
| `HostId` | Read-only. The GUID which identifies this host entry, generated when the configuration is created. Use it for later updates, removal, and to match the entry in the configuration file. |
| `Name` | The UMP Endpoint Name for this host, as remote devices will see it. Capped at 98 bytes by the MIDI 2.0 specification. |
| `ServiceInstanceName` | The mDNS service instance name. Must be unique on the network, and is also used to name the parent device. Creation fails with `ServiceInstanceNameInUse` if another host already has it. |
| `ProductInstanceId` | The Product Instance Id advertised for this host. Capped at 42 bytes by the specification. |
| `CreateOnlyUmpEndpoints` | When true, only UMP endpoints are created. When false, MIDI 1.0 ports are created alongside them. |
| `UseAutomaticPortAllocation` | When true, the service picks a UDP port. When false, `ManuallyAssignedPort` is used. |
| `ManuallyAssignedPort` | The UDP port to bind, as a string. Ignored when `UseAutomaticPortAllocation` is true. |
| `Advertise` | When true, the host is advertised over mDNS so remote devices can discover it. When false, it is reachable only by direct address. |
| `AuthenticationType` | The authentication this host requires. Only `NoAuthentication` is currently accepted; anything else is rejected at configuration time. See `MidiNetworkAuthenticationType`. |

## Remarks

`Name` and `ProductInstanceId` are validated against the byte limits in the MIDI 2.0 specification, not character counts, so non-ASCII names may be shorter than expected. Exceeding either limit fails creation with `EndpointNameTooLong` or `ProductInstanceIdTooLong` rather than silently truncating.
