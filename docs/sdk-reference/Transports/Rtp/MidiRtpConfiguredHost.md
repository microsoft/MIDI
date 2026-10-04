---
layout: sdk_reference_page
title: MidiRtpConfiguredHost
namespace: Windows.Devices.Midi2.Transports.Rtp
type: runtimeclass
description: Information about an RTP-MIDI host set up in the service
---

Returned by `MidiRtpTransportManager.GetConfiguredHosts()`.

## Properties

| Property | Description |
| -------- | ----------- |
| `HostId` | The GUID that identifies this host entry |
| `Name` | What remote devices show for this PC. When the host was created with an empty name, this is this PC's name |
| `ServiceInstanceName` | The name the host is set up to advertise. When it was created without one, this is `Name` |
| `ActualServiceInstanceName` | The name really in use on the network. When another device already advertises a name, Windows gives this host a different one instead of refusing it, so this isn't always the name that was set up. Empty when the host isn't advertised |
| `ServiceInstanceNameWasChanged` | True when that happened. The host works, but other devices see a different name than the one set up, so it's worth showing people |
| `IsEnabled` | False while the host is stopped with `StopRtpHostAsync` |
| `HasStarted` | True when the host is running and can take connections |
| `Advertise` | True if the host is advertised on the local network |
| `ConfiguredPort` | The port the host was set up to use, or `auto`. Compare it with `ActualPort` to see whether the host got the port that was asked for |
| `ActualPort` | The UDP port the host is using. RTP-MIDI also uses the port after it. Zero until the host has started |
| `AllowPortFallback` | True when the host was allowed to start on another port if the one it was set up with was in use |
| `UsedPortFallback` | True when the host couldn't have the port it wanted, so it started on another one. For a host set to `auto`, that means port 5004 was already in use, often by other RTP-MIDI software on the same PC. The host works, but not on the port that was asked for, so it's worth showing people |
| `SendRecoveryJournal` | True if the host sends a recovery journal with each packet |
| `SendSpeedLimit` | How fast the host is set up to send to each connected device. See [MidiRtpSendSpeedLimit]({{ site.baseurl }}/sdk-reference/Transports/Rtp/MidiRtpSendSpeedLimitEnum/) |
| `CurrentSendSpeedLimit` | How fast the running host is sending. It's only different from `SendSpeedLimit` for a moment after a change |
| `NetworkAdapterId` | The network adapter the host is limited to, or an empty GUID for every adapter |
| `NetworkAdapterName` | That adapter's name, from when it was chosen |
| `AllowNetworkAdapterFallback` | True when the host runs on every adapter while its own adapter is missing |
| `IsNetworkAdapterMissing` | True when the host's adapter is missing. If `HasStarted` is also true, the host is running on every adapter until the adapter is back. If not, the host is waiting, and starts by itself when the adapter is back |
| `UsedNetworkAdapterFallback` | True when the host is running on every adapter because its own adapter is missing |
| `RemoteClientPolicy` | What the host does when a remote device it hasn't been told about asks to connect. See `MidiRtpRemoteClientPolicy` |
| `KnownRemoteClients` | The remote devices this host has been told to allow or deny for good, as `MidiRtpKnownRemoteClient` entries |
| `RemoteClientSettings` | The remote devices this host sends to at a speed of their own instead of the host's, as [MidiRtpRemoteClientSettings]({{ site.baseurl }}/sdk-reference/Transports/Rtp/MidiRtpRemoteClientSettings/) entries. This is the list the running service holds, whether or not the devices are connected |
| `LastErrorCode` | The HRESULT from the last try to start or advertise the host, or `0` |
| `Connections` | The remote devices connected to this host right now, as [MidiRtpConnection]({{ site.baseurl }}/sdk-reference/Transports/Rtp/MidiRtpConnection/) entries |

## Remarks

`KnownRemoteClients` has every decision the service holds for this host that isn't limited to a single request, including ones made with `ApproveOrDenyRemoteClientConnectRequestAsync` since the service started. The service doesn't save them. To keep them after a restart, put this list in a [MidiRtpHostKnownClientsConfig]({{ site.baseurl }}/sdk-reference/Transports/Rtp/MidiRtpHostKnownClientsConfig/), change it if you need to, and save it.

To change a remote device's own speed, start from `RemoteClientSettings`, change it, and send a [MidiRtpHostRemoteClientSettingsConfig]({{ site.baseurl }}/sdk-reference/Transports/Rtp/MidiRtpHostRemoteClientSettingsConfig/) holding the whole list.

A remote device waiting for approval isn't connected, so it isn't in `Connections`. Use `GetPendingRemoteClients()` to find those.

This is a copy taken when you called `GetConfiguredHosts()`, not an object that updates itself. Call it again to refresh.
