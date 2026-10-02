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
| `NetworkAdapterId` | The network adapter the host is limited to, as the GUID Windows gives it. It's the same value as `Windows.Networking.Connectivity.NetworkAdapter.NetworkAdapterId` and .NET's `NetworkInterface.Id`. Leave it empty, the default, to run the host on every adapter. Setting it also fills in `NetworkAdapterName` |
| `NetworkAdapterName` | The adapter's name, like `Ethernet 3`. It's only shown to people, so they can tell which adapter a host is waiting for |
| `AllowNetworkAdapterFallback` | What the host does while its adapter is missing. When true, the default, it runs on every adapter until the adapter is back. When false, it doesn't run until the adapter is back |

## Remarks

`Name` and `ProductInstanceId` are checked against the byte limits in the MIDI 2.0 specification, not character counts, so a name with non-ASCII characters can hold fewer characters than you might expect. Going over either limit fails creation with `EndpointNameTooLong` or `ProductInstanceIdTooLong`, instead of quietly shortening the value.

A host limited to one network adapter only advertises itself there, and only answers devices that reach it through that adapter. The service also remembers the adapter's hardware address, so a USB network adapter that comes back with a new GUID after being plugged into another port is still found.

If the adapter is missing and `AllowNetworkAdapterFallback` is false, `CreateNetworkHostAsync` reports `NetworkAdapterNotAvailable`. The host is still created, and it starts by itself when the adapter is back, so save the configuration as usual.
