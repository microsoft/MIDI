---
layout: sdk_reference_page
title: MidiNetworkTransportManager
namespace: Windows.Devices.Midi2.Transports.Network
type: runtimeclass
description: The primary class used to create, remove, and monitor Network MIDI 2.0 hosts and client connections
---

Start here for anything to do with Network MIDI 2.0. All members are static.

## Static Properties

| Static Property | Description |
| -------- | ----------- |
| `IsTransportAvailable` | True if the Network MIDI 2.0 transport is available in the service |
| `TransportId` | The GUID of this transport |
| `MidiNetworkUdpDnsServiceType` | The DNS-SD service type Network MIDI 2.0 uses, for your own discovery code |
| `MidiNetworkUdpDnsDomain` | The DNS-SD domain used for discovery |
| `MidiNetworkUdpDnsSdQueryName` | The full DNS-SD query name, `_midi2._udp.local`, as passed to `DnsServiceBrowse` |

## Static Methods

| Static Method | Description |
| -------- | ----------- |
| `CreateNetworkHostAsync(creationConfig)` | Creates a host that remote clients can connect to. Returns a `MidiNetworkHostCreationResponse`. Doesn't finish until the host has started |
| `RemoveNetworkHostAsync(removalConfig)` | Removes a host, and disconnects anything connected to it. Returns a `MidiNetworkHostRemovalResponse` |
| `StartNetworkHostAsync(hostId)` | Starts a host that's set up but stopped. Returns a `MidiNetworkHostUpdateResponse` |
| `StopNetworkHostAsync(hostId)` | Stops a running host without removing its configuration. Returns a `MidiNetworkHostUpdateResponse` |
| `UpdateNetworkHostAsync(updateConfig)` | Changes settings on an existing host without stopping it. Returns a `MidiNetworkHostUpdateResponse` |
| `ConnectNetworkClientAsync(connectConfig)` | Connects to a remote host, found by discovery or by direct address. Returns a `MidiNetworkClientConnectResponse`. For a `ClientId` that already exists, this retries an entry that was marked unavailable |
| `DisconnectNetworkClientAsync(disconnectConfig)` | Disconnects a client connection. Returns a `MidiNetworkClientDisconnectResponse`. A client disconnected this way isn't reconnected automatically |
| `UpdateNetworkClientAsync(updateConfig)` | Changes settings on an existing client connection without disconnecting it. Returns a `MidiNetworkClientUpdateResponse` |
| `ApproveOrDenyRemoteClientConnectRequestAsync(approvalConfig)` | Approves or denies a remote client that's waiting on a host that requires approval. Returns a `MidiNetworkRemoteClientApprovalResponse` |
| `DisconnectRemoteClientAsync(disconnectConfig)` | Ends one remote client's session with one of this PC's hosts. Returns a `MidiNetworkRemoteClientDisconnectResponse`. This doesn't save an allow or deny decision for later |
| `ForgetRemoteClientAsync(forgetConfig)` | Drops whatever decision a host has for a remote client, so its next invitation is judged by the host's policy alone. Returns a `MidiNetworkRemoteClientForgetResponse`. This applies to the running service. Also save the lists with `MidiNetworkHostKnownClientsConfig`, or the decision comes back the next time the service starts. A session that's already running keeps running |
| `GetConfiguredHosts()` | Returns a `MidiNetworkConfiguredHost` for every host set up in the service |
| `GetConfiguredClients()` | Returns a `MidiNetworkConfiguredClient` for every client that's set up, connected or not |
| `GetPendingRemoteClients()` | Returns a `MidiNetworkPendingRemoteClient` for each remote client waiting for someone to decide. Call it every few seconds to keep an approval screen up to date |
| `GetAdvertisedHosts()` | Returns the `MidiNetworkAdvertisedHost` entries visible on the network right now, one time. To keep getting updates, use `MidiNetworkAdvertisedHostWatcher` |
| `GetTransportSettings()` | Returns a [MidiNetworkTransportSettings]({{ site.baseurl }}/sdk-reference/Transports/Network/MidiNetworkTransportSettings/) with the settings the transport is using right now. These may not match the configuration exactly, because a value that's out of range or the wrong type is corrected when it's read |
| `GenerateAvailableHostPort()` | Returns a free UDP port for a host to keep, or zero if none was found. It's picked below the range Windows hands out on its own, so Windows won't give it to another program while the service isn't running |
| `IsHostPortAvailable(port)` | Returns true if a host can use the port. Use it to check a port someone typed in, before you try to create the host with it |
| `GetSavedHosts()` | Returns a [MidiNetworkSavedHost]({{ site.baseurl }}/sdk-reference/Transports/Network/MidiNetworkSavedHost/) for every host saved in the configuration file, with its saved allow and deny decisions. Works even when the service isn't running |
| `GetSavedClients()` | Returns a [MidiNetworkSavedClient]({{ site.baseurl }}/sdk-reference/Transports/Network/MidiNetworkSavedClient/) for every client saved in the configuration file. Works even when the service isn't running |

## Remarks

All the `Async` methods are truly asynchronous, and none of them throw exceptions. They report failure with the `Success` and `ErrorCode` properties on the response they return.

`GetConfiguredClients()` lists every client entry the running service holds, not just live connections. So an entry that has never connected, or that can't be reached right now, still shows up in the results, with its `EntryState` set to match.

## Saved and configured

Configured means what the running service holds now. Saved means what's in the configuration file, which is what the service creates the next time it starts. An entry can be one without the other. For example, a client connected without being saved is configured but not saved. Match the two lists on `HostId` or `ClientId`.

To change what's saved, pass a configuration object to `MidiServiceTransportPluginConfigManager.SaveUpdate`:

| To | Save |
| --- | --- |
| Save a host | `MidiNetworkHostCreationConfig` |
| Change a saved host's MIDI 1.0 port settings | `MidiNetworkHostUpdateConfig` |
| Change a saved host's allow and deny decisions | `MidiNetworkHostKnownClientsConfig`, starting from `MidiNetworkSavedHost.KnownRemoteClients` |
| Remove a saved host | `MidiNetworkHostRemovalConfig` |
| Save a client | `MidiNetworkClientConnectConfig` |
| Change a saved client's MIDI 1.0 port settings | `MidiNetworkClientUpdateConfig` |
| Remove a saved client | `MidiNetworkClientDisconnectConfig` |
