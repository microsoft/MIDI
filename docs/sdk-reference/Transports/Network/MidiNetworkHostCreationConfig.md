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
| `EnsureCompliantServiceInstanceName(serviceInstanceName)` | Returns a version of the supplied service instance name which is valid for use as an mDNS service instance name. Strips what would corrupt the DNS-SD record and truncates to the label byte limit. |
| `IsServiceInstanceNameAvailable(serviceInstanceName)` | False if a host on this PC already holds the name, or if anything on the local network is currently advertising it. |
| `MakeUniqueServiceInstanceName(baseServiceInstanceName)` | The supplied name if it is free, otherwise the same name with `-02`, `-03` and so on appended until one is. Already compliant on return. |

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
| `AllowPortFallback` | Only meaningful with a specific port. When that port cannot be bound the host starts on an automatically allocated one instead of refusing to start, and reports that it did through `MidiNetworkConfiguredHost.UsedPortFallback`. |
| `Advertise` | When true, the host is advertised over mDNS so remote devices can discover it. When false, it is reachable only by direct address. |
| `RemoteClientPolicy` | How this host handles unknown remote clients. `AllowAny` accepts unless explicitly denied; `RequireApproval` keeps clients pending until approved or denied. |
| `AuthenticationType` | The authentication this host requires. Only `NoAuthentication` is currently accepted; anything else is rejected at configuration time. See `MidiNetworkAuthenticationType`. |

## Remarks

`Name` and `ProductInstanceId` are validated against the byte limits in the MIDI 2.0 specification, not character counts, so non-ASCII names may be shorter than expected. Exceeding either limit fails creation with `EndpointNameTooLong` or `ProductInstanceIdTooLong` rather than silently truncating.
