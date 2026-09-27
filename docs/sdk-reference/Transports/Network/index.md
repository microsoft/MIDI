---
layout: sdk_namespace_page
title: WinRT API Support for Network MIDI 2.0 Endpoints
namespace: Windows.Devices.Midi2.Transports.Network
description: Namespace for creating and managing Network MIDI 2.0 (UDP) hosts and clients
---

Types for creating, removing, and monitoring Network MIDI 2.0 hosts and client connections at runtime, and for discovering Network MIDI 2.0 hosts advertised on the local network.

Everything here is reached through the static [MidiNetworkTransportManager]({{ site.baseurl }}/sdk-reference/Transports/Network/MidiNetworkTransportManager/) class.

## Definitions

Network MIDI 2.0 calls the two ends of a session the host and the client. The names say which end waits for a connection and which end starts it, not which end sends MIDI messages. Once a session is set up, data goes both ways.

- **Host**: Windows MIDI Services listens on a UDP port and accepts sessions from remote clients. Usually there's one per PC. It can be advertised over mDNS so other devices can find it.
- **Client**: Windows MIDI Services connects to a remote host. That's either one found over mDNS, or one you reach directly by its host name or IP address, and port.

A single PC can be both at the same time.

## Prerequisites

- The Network MIDI 2.0 transport is installed and enabled in the service
- `midisrv.exe` is allowed through Windows Firewall and any other firewall in use
- For discovery, mDNS is running on the PC and allowed on the network. Windows MIDI Services uses the mDNS support built into Windows, not Bonjour
- Direct connections by address and port don't need mDNS, and they aren't limited to the local subnet, as long as your network and firewalls allow them

## Typical flows

**Creating a host so other devices can connect to this PC**

1. Fill in a [MidiNetworkHostCreationConfig]({{ site.baseurl }}/sdk-reference/Transports/Network/MidiNetworkHostCreationConfig/)
2. `await MidiNetworkTransportManager.CreateNetworkHostAsync(config)`
3. Check `Success` on the returned [MidiNetworkHostCreationResponse]({{ site.baseurl }}/sdk-reference/Transports/Network/MidiNetworkHostCreationResponse/)

`CreateNetworkHostAsync` doesn't return until the host is running. So a successful result means the host is up and, if you asked for it, advertising.

**Connecting to a remote host**

1. Discover hosts with a [MidiNetworkAdvertisedHostWatcher]({{ site.baseurl }}/sdk-reference/Transports/Network/MidiNetworkAdvertisedHostWatcher/), or address one directly
2. Fill in a [MidiNetworkClientConnectConfig]({{ site.baseurl }}/sdk-reference/Transports/Network/MidiNetworkClientConnectConfig/) with a [MidiNetworkClientMatchCriteria]({{ site.baseurl }}/sdk-reference/Transports/Network/MidiNetworkClientMatchCriteria/)
3. `await MidiNetworkTransportManager.ConnectNetworkClientAsync(config)`

**Approving remote clients**

A host set to require approval answers an unknown remote device with "pending," instead of accepting it. Call `GetPendingRemoteClients()` to find them, and decide on each one with `ApproveOrDenyRemoteClientConnectRequestAsync`. The service doesn't send a notification, so check every few seconds.

**Changing a host or a connection that's already running**

1. Fill in a [MidiNetworkHostUpdateConfig]({{ site.baseurl }}/sdk-reference/Transports/Network/MidiNetworkHostUpdateConfig/) or a [MidiNetworkClientUpdateConfig]({{ site.baseurl }}/sdk-reference/Transports/Network/MidiNetworkClientUpdateConfig/), identifying the entry by its `HostId` or `ClientId`
2. `await MidiNetworkTransportManager.UpdateNetworkHostAsync(config)` or `UpdateNetworkClientAsync(config)`
3. Check `Success` on the returned [MidiNetworkHostUpdateResponse]({{ site.baseurl }}/sdk-reference/Transports/Network/MidiNetworkHostUpdateResponse/) or [MidiNetworkClientUpdateResponse]({{ site.baseurl }}/sdk-reference/Transports/Network/MidiNetworkClientUpdateResponse/)

Neither one stops the host or disconnects the client. A setting that can change on a running session, such as `FallbackMidi1PortCount`, takes effect right away. A setting that's decided when an endpoint is built, such as `CreateMidi1Ports`, is saved for the next connection. `Success` means the service accepted the settings, not that each one changed something you can see.

## Reconnection behavior

Once a client is set up, the service manages the connection for you. What it does when a remote host goes away depends on how the client was set up, because each way gives the service different information to work with.

| Situation | Discovered (mDNS) client | Direct address client |
| --------- | ------------------------ | --------------------- |
| Host not present at startup | Connects when the host advertises | One attempt, then marked `Unavailable` |
| Host goes away and returns | Reconnects when it advertises again | One further attempt, then `Unavailable` |
| Never answered | Retried whenever it advertises | Marked `Unavailable` |

A direct address is never retried on a timer. Nothing announces that a fixed IP address is back, so retrying on a timer would keep sending invitations over the network forever, for every address in the configuration that can't be reached. To retry one, call `ConnectNetworkClientAsync` again with the same `ClientId`. For an entry that already exists, this means "it's reachable now, try again."

Use [MidiNetworkConfiguredClient]({{ site.baseurl }}/sdk-reference/Transports/Network/MidiNetworkConfiguredClient/).`EntryState` to show this in your app. See [MidiNetworkClientEntryState]({{ site.baseurl }}/sdk-reference/Transports/Network/MidiNetworkClientEntryStateEnum/).

## Persistence

Hosts and clients created with this API are temporary, and go away when the service restarts. To keep one, also pass the same configuration object to `MidiServiceTransportPluginConfigManager.SaveUpdate`. MIDI Settings and Network MIDI 2.0 Setup do this for you.

To keep a host's allow and deny decisions after a restart, save a [MidiNetworkHostKnownClientsConfig]({{ site.baseurl }}/sdk-reference/Transports/Network/MidiNetworkHostKnownClientsConfig/) the same way. The service reads those lists when it starts, but it never saves them itself.

## Current limitations

- Authentication isn't built yet. A host set up to require it is rejected when it's configured, instead of quietly accepting connections that aren't authenticated. See [issue 733](https://github.com/microsoft/MIDI/issues/733)
- mDNS discovery only works on the local subnet. Direct connections don't have that limit
- mDNS discovery can take a while to find everything. `midimdnsinfo.exe` in the MIDI tools helps you check what's visible on the network
