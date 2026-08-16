---
layout: sdk_namespace_page
title: App SDK Support for Network MIDI 2.0 Endpoints
namespace: Windows.Devices.Midi2.Transports.Network
description: Namespace for creating and managing Network MIDI 2.0 (UDP) hosts and clients
---

Types for creating, removing, and monitoring Network MIDI 2.0 hosts and client connections at runtime, and for discovering Network MIDI 2.0 hosts advertised on the local network.

Everything here is reached through the static [MidiNetworkTransportManager](MidiNetworkTransportManager) class.

## Definitions

Network MIDI 2.0 uses the terms Host and Client for the two ends of a session. They describe which end accepts and which end initiates, not which end sends MIDI messages — data flows both ways once a session is established.

- **Host**: Windows MIDI Services listens on a UDP port and accepts sessions from remote clients. Typically one per PC. Optionally advertised over mDNS.
- **Client**: Windows MIDI Services connects out to a remote host, either one discovered over mDNS or one addressed directly by host name / IP address and port.

A single PC can be both at the same time.

## Prerequisites

- The Network MIDI 2.0 transport is installed and enabled in the service
- `midisrv.exe` is allowed through Windows Firewall and any other firewall in use
- For discovery, mDNS is running on the PC and permitted on the network. Windows MIDI Services uses the built-in Windows mDNS support, not Bonjour
- Direct connections by address and port do not require mDNS and are not limited to the local subnet, provided routing and firewalls allow it

## Typical flows

**Creating a host so other devices can connect to this PC**

1. Fill in a [MidiNetworkHostCreationConfig](MidiNetworkHostCreationConfig)
2. `await MidiNetworkTransportManager.CreateNetworkHostAsync(config)`
3. Check `Success` on the returned [MidiNetworkHostCreationResponse](MidiNetworkHostCreationResponse)

`CreateNetworkHostAsync` does not return until the host is actually running, so a successful result means the host is live and, if requested, advertising.

**Connecting to a remote host**

1. Discover hosts with a [MidiNetworkAdvertisedHostWatcher](MidiNetworkAdvertisedHostWatcher), or address one directly
2. Fill in a [MidiNetworkClientConnectConfig](MidiNetworkClientConnectConfig) with a [MidiNetworkClientMatchCriteria](MidiNetworkClientMatchCriteria)
3. `await MidiNetworkTransportManager.ConnectNetworkClientAsync(config)`

**Approving remote clients**

A host configured to require approval answers an unknown remote with "pending" rather than accepting it. Poll `GetPendingRemoteClients()` and resolve each one with `ApproveOrDenyRemoteClientConnectRequestAsync`. There is no notification from the service, so polling every few seconds is the expected pattern.

## Reconnection behavior

Once a client has been configured, the service manages the connection for you. What it does when a remote host goes away depends on how the client was configured, because the two cases give the service different information to work with.

| Situation | Discovered (mDNS) client | Direct address client |
| --------- | ------------------------ | --------------------- |
| Host not present at startup | Connects when the host advertises | One attempt, then marked `Unavailable` |
| Host goes away and returns | Reconnects when it advertises again | One further attempt, then `Unavailable` |
| Never answered | Retried whenever it advertises | Marked `Unavailable` |

A direct address is never retried on a timer. Nothing announces that a fixed IP address has come back, so polling it would put invitations on the wire indefinitely for every unreachable address in a user's configuration. To retry one, call `ConnectNetworkClientAsync` again with the same `ClientId` — for an entry which already exists, this acts as an "it is reachable now, try again" signal.

Use [MidiNetworkConfiguredClient](MidiNetworkConfiguredClient).`EntryState` to surface this in a UI. See [MidiNetworkClientEntryState](MidiNetworkClientEntryStateEnum).

## Persistence

Hosts and clients created through this API are transient and disappear when the service restarts. To persist them, the creating application also writes the equivalent entry to the configuration file. The Windows MIDI Services Settings app does this for you.

The service reads `allowedClients` and `deniedClients` lists for each host from the configuration file at startup, so remote client decisions saved by an app are honored on the next run. The service itself never writes to the configuration file.

## Current limitations

- Authentication is not implemented. A host configured to require it is rejected at configuration time rather than silently accepting unauthenticated connections. See [issue 733](https://github.com/microsoft/MIDI/issues/733)
- mDNS discovery is limited to the local subnet. Direct connections are not
- mDNS discovery can be slow to settle. `midimdnsinfo.exe` in the SDK tools is useful for checking what is visible on the network
