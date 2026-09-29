---
layout: sdk_reference_page
title: MidiRtpTransportManager
namespace: Windows.Devices.Midi2.Transports.Rtp
type: runtimeclass
description: The primary class used to create, remove, and monitor RTP-MIDI hosts and client connections
---

Start here for anything to do with RTP-MIDI. All members are static.

## Static Properties

| Static Property | Description |
| -------- | ----------- |
| `IsTransportAvailable` | True if the RTP-MIDI transport is installed in the service |
| `TransportId` | The GUID of this transport |
| `DefaultHostPort` | `5004`, the port other RTP-MIDI software looks at first. RTP-MIDI also uses the port after it |
| `MidiRtpDnsServiceType` | The DNS-SD service type RTP-MIDI devices advertise, `_apple-midi._udp`, for your own discovery code |
| `MidiRtpDnsDomain` | The DNS-SD domain used for discovery, `local` |
| `MidiRtpDnsSdQueryName` | The full DNS-SD query name, `_apple-midi._udp.local`, as passed to `DnsServiceBrowse` |

## Static Methods

| Static Method | Description |
| -------- | ----------- |
| `CreateRtpHostAsync(creationConfig)` | Creates a host that remote devices can connect to. Returns a `MidiRtpHostCreationResponse`. Doesn't finish until the host has started, or about ten seconds have gone by |
| `RemoveRtpHostAsync(removalConfig)` | Ends every connection to the host, and removes the host from the running service. Returns a `MidiRtpHostRemovalResponse` |
| `StopRtpHostAsync(hostId)` | Stops a host without removing it. Its connections end, and nothing can reach it until it's started again. Returns a `MidiRtpHostUpdateResponse` |
| `StartRtpHostAsync(hostId)` | Starts a host that was stopped. Returns a `MidiRtpHostUpdateResponse` |
| `ConnectRtpClientAsync(connectConfig)` | Adds a client entry, and starts connecting to the remote device in the background. Returns a `MidiRtpClientConnectResponse` as soon as the service has the entry |
| `ReconnectRtpClientAsync(clientId)` | Tries to connect a client right away, instead of after the usual wait. It's also how you bring back an entry marked `Unavailable`. Returns a `MidiRtpClientConnectResponse` |
| `DisconnectRtpClientAsync(disconnectConfig)` | Ends the client's connection, and removes the entry from the running service. Returns a `MidiRtpClientDisconnectResponse` |
| `ApproveOrDenyRemoteClientConnectRequestAsync(approvalConfig)` | Approves or denies a remote device that asked to connect to a host. Denying also ends any connection that remote device already has to the host. Returns a `MidiRtpRemoteClientApprovalResponse` |
| `DisconnectRemoteClientAsync(disconnectConfig)` | Ends one connection to one of this PC's hosts. Nothing is remembered, so the remote device may connect again. Returns a `MidiRtpRemoteClientDisconnectResponse` |
| `ForgetRemoteClientAsync(forgetConfig)` | Drops a host's allow or deny decision for a remote device, so the next time it asks, it's judged by the host's policy alone. A connection that's already up keeps running. Returns a `MidiRtpRemoteClientForgetResponse` |
| `GetConfiguredHosts()` | Returns a `MidiRtpConfiguredHost` for every host in the service, with its connections |
| `GetConfiguredClients()` | Returns a `MidiRtpConfiguredClient` for every client entry, connected or not |
| `GetPendingRemoteClients()` | Returns a `MidiRtpPendingRemoteClient` for each remote device waiting for someone to decide. Call it every few seconds to keep an approval screen up to date |
| `GetAdvertisedHosts()` | Returns a `MidiRtpAdvertisedHost` for each RTP-MIDI device advertised on the local network right now, including this PC's own hosts |

## Remarks

All the `Async` methods are truly asynchronous, and none of them throw exceptions. They report failure with the `Success` and `ErrorCode` properties on the response they return.

Everything these methods change is in the running service only. To keep a host, a client, or a decision after the service restarts, also save it with `MidiServiceTransportPluginConfigManager.SaveUpdate`. See the [namespace overview]({{ site.baseurl }}/sdk-reference/Transports/Rtp/).

The service watches the network for advertised RTP-MIDI devices all the time, so `GetAdvertisedHosts()` answers right away. Each `Get` method returns a copy taken when you called it, not a list that updates itself. Call it again to refresh. When the transport isn't installed, or the service isn't running, they return empty lists.
