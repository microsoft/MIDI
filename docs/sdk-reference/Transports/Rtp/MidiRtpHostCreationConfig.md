---
layout: sdk_reference_page
title: MidiRtpHostCreationConfig
namespace: Windows.Devices.Midi2.Transports.Rtp
type: runtimeclass
implements: Windows.Devices.Midi2.ServiceConfig.IMidiServiceTransportPluginConfig
description: Config sent to the service to create an RTP-MIDI host
---

Describes a host for remote devices to connect to. Pass it to `MidiRtpTransportManager.CreateRtpHostAsync`. To keep the host after the service restarts, also pass it to `MidiServiceTransportPluginConfigManager.SaveUpdate`.

## Constructors

| Constructor | Description |
| -------- | ----------- |
| `MidiRtpHostCreationConfig()` | Creates a configuration with a new `HostId` and the defaults below |

## Properties

| Property | Description |
| -------- | ----------- |
| `HostId` | The GUID that identifies this host entry. A new one is made when the configuration is created. Use it to stop, start, and remove the host later, and to find it in `GetConfiguredHosts()`. Set it to the id of a host that already exists to change that host's settings |
| `Name` | What remote devices show for this PC. Leave it empty to use this PC's name. At most 63 bytes in UTF-8 |
| `ServiceInstanceName` | The name advertised on the network, when it should be different from `Name`. Leave it empty to advertise `Name`. At most 63 bytes in UTF-8, with no periods |
| `UseAutomaticPortAllocation` | When true, the host tries port 5004 first, and takes a free port if 5004 is in use. `MidiRtpConfiguredHost.UsedPortFallback` tells you when it didn't get 5004. When false, `ManuallyAssignedPort` is used |
| `ManuallyAssignedPort` | The UDP port to use, from 1024 to 65534. RTP-MIDI also uses the port after it. Ignored when `UseAutomaticPortAllocation` is true |
| `AllowPortFallback` | Only matters when you choose a port. If that port is in use, the host starts on another one, instead of not starting at all. `MidiRtpConfiguredHost.UsedPortFallback` tells you when that happened |
| `Advertise` | When true, the host is advertised on the local network, so remote devices list it. When false, it can only be reached by its address and port |
| `RemoteClientPolicy` | What the host does when a remote device it hasn't been told about asks to connect. See `MidiRtpRemoteClientPolicy` |
| `SendRecoveryJournal` | When true, each packet carries a recovery journal, so the remote device can repair a lost Note Off. Only turn it off for a device that can't read one |
| `NetworkAdapterId` | The network adapter the host is limited to, as the GUID Windows gives it. It's the same value as `Windows.Networking.Connectivity.NetworkAdapter.NetworkAdapterId` and .NET's `NetworkInterface.Id`. Leave it empty to run the host on every adapter. Setting it also fills in `NetworkAdapterName` |
| `NetworkAdapterName` | The adapter's name, like `Ethernet 3`. It's only shown to people, so they can tell which adapter a host is waiting for |
| `AllowNetworkAdapterFallback` | What the host does while its adapter is missing. When true, it runs on every adapter until the adapter is back. When false, it doesn't run until the adapter is back |
| `SendSpeedLimit` | How fast the host sends to each connected device. Choose a slower speed for a device that loses data when a lot of it arrives at once, like a long SysEx dump. A lone message is never delayed. See [MidiRtpSendSpeedLimit]({{ site.baseurl }}/sdk-reference/Transports/Rtp/MidiRtpSendSpeedLimitEnum/) |

## Defaults

| Property | Default |
| -------- | ------- |
| `Name` | Empty, so this PC's name is used |
| `ServiceInstanceName` | Empty, so `Name` is advertised |
| `UseAutomaticPortAllocation` | `true` |
| `ManuallyAssignedPort` | `5004` |
| `AllowPortFallback` | `true` |
| `Advertise` | `true` |
| `RemoteClientPolicy` | `AllowAny` |
| `SendRecoveryJournal` | `true` |
| `NetworkAdapterId` | Empty, so the host runs on every adapter |
| `AllowNetworkAdapterFallback` | `true` |
| `SendSpeedLimit` | `Unlimited` |

## Remarks

The 63 byte limit is in bytes, not characters, so a name with non-ASCII characters holds fewer characters than you might expect. It's the most a name advertised on the network can hold. A longer name fails with `NameTooLong`, instead of being shortened.

When the host is advertised and `ServiceInstanceName` is empty, `Name` is the name advertised, so it can't have a period in it either.

If another device on the network already advertises the same name, Windows gives the host a different one, instead of refusing it. `MidiRtpConfiguredHost.ActualServiceInstanceName` has the name really in use.

There's no separate way to change a host. To change one, fill in every property the way you want it, set `HostId` to the host's id, and pass the configuration to `CreateRtpHostAsync`. The host restarts with the new settings, which ends its connections, and its remembered allow and deny decisions are kept. A host that was stopped is started again. A new `SendSpeedLimit` is the exception: when it's the only change, it applies to the running host and its connections straight away, without a restart.

A host limited to one network adapter only advertises itself there, and only answers devices that reach it through that adapter. The service also remembers the adapter's hardware address, so a USB network adapter that comes back with a new GUID after being plugged into another port is still found. If the adapter is missing and `AllowNetworkAdapterFallback` is false, `CreateRtpHostAsync` reports `NetworkAdapterNotAvailable`. The host is still created, and it starts by itself when the adapter is back, so save the configuration as usual.
