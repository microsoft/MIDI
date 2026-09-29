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
| `UsedPortFallback` | True when that happened. The host works, but not on the port that was asked for, so it's worth showing people |
| `SendRecoveryJournal` | True if the host sends a recovery journal with each packet |
| `RemoteClientPolicy` | What the host does when a remote device it hasn't been told about asks to connect. See `MidiRtpRemoteClientPolicy` |
| `KnownRemoteClients` | The remote devices this host has been told to allow or deny for good, as `MidiRtpKnownRemoteClient` entries |
| `LastErrorCode` | The HRESULT from the last try to start or advertise the host, or `0` |
| `Connections` | The remote devices connected to this host right now, as [MidiRtpConnection]({{ site.baseurl }}/sdk-reference/Transports/Rtp/MidiRtpConnection/) entries |

## Remarks

`KnownRemoteClients` has every decision the service holds for this host that isn't limited to a single request, including ones made with `ApproveOrDenyRemoteClientConnectRequestAsync` since the service started. The service doesn't save them. To keep them after a restart, put this list in a [MidiRtpHostKnownClientsConfig]({{ site.baseurl }}/sdk-reference/Transports/Rtp/MidiRtpHostKnownClientsConfig/), change it if you need to, and save it.

A remote device waiting for approval isn't connected, so it isn't in `Connections`. Use `GetPendingRemoteClients()` to find those.

This is a copy taken when you called `GetConfiguredHosts()`, not an object that updates itself. Call it again to refresh.
