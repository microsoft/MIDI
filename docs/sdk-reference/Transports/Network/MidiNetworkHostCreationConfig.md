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

## Properties

| Property | Description |
| -------- | ----------- |
| `HostId` | The GUID which identifies this host entry, used for later updates, removal, and to match the entry in the configuration file. Set this yourself; it is how you find the host again. |
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
