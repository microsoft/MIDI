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
| `MidiNetworkHostCreationConfig()` | Creates an empty configuration |

## Static Methods

| Static Method | Description |
| -------- | ----------- |
| `CreateDefault()` | Creates a configuration filled in with good defaults, including a name based on the computer name |
| `EnsureCompliantServiceInstanceName(serviceInstanceName)` | Returns a version of this name that's valid as an mDNS service instance name. It removes anything that would break the DNS-SD record, and shortens the name to the limit in bytes |
| `IsServiceInstanceNameAvailable(serviceInstanceName)` | Returns false if a host on this PC already uses the name, or if anything on the local network is advertising it right now |
| `MakeUniqueServiceInstanceName(baseServiceInstanceName)` | Returns the name as is if it's free. Otherwise, adds `-02`, `-03`, and so on until it finds one that is. The result is always a valid name |

## Properties

| Property | Description |
| -------- | ----------- |
| `HostId` | Read-only. The GUID that identifies this host entry, made when the configuration is created. Use it for later updates and removal, and to find the entry in the configuration |
| `Name` | The UMP Endpoint Name for this host, as remote devices will see it. The MIDI 2.0 specification limits it to 98 bytes |
| `ServiceInstanceName` | The mDNS service instance name. It must be unique on the network, and it's also used to name the parent device. Creation fails with `ServiceInstanceNameInUse` if another host already has it |
| `ProductInstanceId` | The Product Instance Id advertised for this host. The specification limits it to 42 bytes |
| `CreateOnlyUmpEndpoints` | When true, only UMP endpoints are created. When false, MIDI 1.0 ports are created with them |
| `FallbackMidi1PortCount` | How many source and destination ports to create for a remote client that declares no function blocks. 1 to 16, and 1 by default. Ignored when the client does describe itself, and when `CreateOnlyUmpEndpoints` is true |
| `UseAutomaticPortAllocation` | When true, the service picks a UDP port. When false, `ManuallyAssignedPort` is used |
| `ManuallyAssignedPort` | The UDP port to use, as a string. Ignored when `UseAutomaticPortAllocation` is true |
| `AllowPortFallback` | Only matters when you choose a port. If that port can't be used, the host starts on a port the service picks, instead of not starting at all. `MidiNetworkConfiguredHost.UsedPortFallback` tells you when that happened |
| `Advertise` | When true, the host is advertised over mDNS so remote devices can find it. When false, it can only be reached by direct address |
| `RemoteClientPolicy` | How this host handles unknown remote clients. `AllowAny` accepts them unless they've been denied. `RequireApproval` keeps them waiting until they're approved or denied |
| `AuthenticationType` | The authentication this host requires. Only `NoAuthentication` is accepted right now. Anything else is rejected when the host is configured. See `MidiNetworkAuthenticationType` |

## Remarks

`Name` and `ProductInstanceId` are checked against the byte limits in the MIDI 2.0 specification, not character counts, so a name with non-ASCII characters can hold fewer characters than you might expect. Going over either limit fails creation with `EndpointNameTooLong` or `ProductInstanceIdTooLong`, instead of quietly shortening the value.
