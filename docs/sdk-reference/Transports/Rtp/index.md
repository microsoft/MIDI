---
layout: sdk_namespace_page
title: WinRT API Support for RTP-MIDI Endpoints
namespace: Windows.Devices.Midi2.Transports.Rtp
description: Namespace for creating and managing RTP-MIDI network hosts and client connections
---

Types for offering this PC to RTP-MIDI devices on the network, connecting to RTP-MIDI devices, deciding which remote devices may connect, and finding RTP-MIDI devices that advertise themselves on the local network.

RTP-MIDI is the network MIDI 1.0 protocol from IETF RFC 6295, along with Apple's protocol for setting up connections. It's built into macOS and iOS, and many MIDI interfaces and apps use it too. It only carries MIDI 1.0 messages. For the newer protocol which supports both MIDI 2.0 and MIDI 1.0 over a network, use [Network MIDI 2.0]({{ site.baseurl }}/sdk-reference/Transports/Network/).

Everything here is reached through the static [MidiRtpTransportManager]({{ site.baseurl }}/sdk-reference/Transports/Rtp/MidiRtpTransportManager/) class.

## Definitions

RTP-MIDI itself doesn't have hosts and clients. Once two devices are connected, they're equals, and MIDI goes both ways. This API uses the same two words as Network MIDI 2.0 to say what this PC does:

- **Host**: this PC listens on a UDP port, and remote devices connect to it. It can be advertised on the local network, so a Mac or an iPad lists it without being told the address. A host takes up to 16 connections at a time.
- **Client**: this PC connects to one remote device, found by the name it advertises, or reached by its address and port.

A PC can have several hosts and clients at the same time. Either way, each connection is reported as a [MidiRtpConnection]({{ site.baseurl }}/sdk-reference/Transports/Rtp/MidiRtpConnection/), because a connection works the same way no matter which end started it.

## Endpoints

Each connection gets its own MIDI endpoint, with MIDI 1.0 ports for apps that use the older Windows MIDI APIs. The endpoint is created when the connection is up, and removed when it ends. An app that lists endpoints only when it starts won't see a connection made after that.

The endpoint is named after the name the remote device sends. For a client, set `CustomEndpointName` to choose a different name before the endpoint is created.

When the same remote device connects to a host again, its endpoint gets the same id as last time, so a name or other setting the customer gave it stays with it. The remote is recognized by the name it sends. A client's endpoint keeps the same id for as long as the client entry exists.

> Unlike Network MIDI 2.0, RTP uses names for programmatically identifying endpoints. 

The transport code is `RTPMIDI`. Use [MidiEndpointTransportSuppliedInfo]({{ site.baseurl }}/sdk-reference/Enumeration/MidiEndpointTransportSuppliedInfo/).`TransportCode` to tell RTP-MIDI endpoints apart from others.

## Prerequisites

- The RTP-MIDI transport is installed and enabled in the service. Check `MidiRtpTransportManager.IsTransportAvailable`. In the preview, it's installed with the Network MIDI 2.0 transport
- For a host, `midisrv.exe` is allowed through Windows Firewall and any other firewall in use
- For discovery, mDNS is running on the PC and allowed on the network. Windows MIDI Services uses the mDNS support built into Windows, not the Apple Bonjour service. Apple Bonjour is not required.
- Direct connections by address and port don't need mDNS

Nothing listens for connections until an app creates a host. A new install has no hosts and no clients.

## Typical flows

**Offering this PC to other devices**

1. Create a [MidiRtpHostCreationConfig]({{ site.baseurl }}/sdk-reference/Transports/Rtp/MidiRtpHostCreationConfig/). Its defaults are a good start: this PC's name, port 5004 or the next free port, advertised, and anyone may connect
2. `await MidiRtpTransportManager.CreateRtpHostAsync(config)`
3. Check `Success` on the returned [MidiRtpHostCreationResponse]({{ site.baseurl }}/sdk-reference/Transports/Rtp/MidiRtpHostCreationResponse/)

`CreateRtpHostAsync` doesn't return until the host has started, so a successful result means other devices can connect now. On a Mac, an advertised host shows up in the Directory list of the MIDI Network Setup window in Audio MIDI Setup.

**Connecting to a remote device**

1. Find it with `MidiRtpTransportManager.GetAdvertisedHosts()`, or use its address and port
2. Fill in a [MidiRtpClientConnectConfig]({{ site.baseurl }}/sdk-reference/Transports/Rtp/MidiRtpClientConnectConfig/) with a [MidiRtpClientMatchCriteria]({{ site.baseurl }}/sdk-reference/Transports/Rtp/MidiRtpClientMatchCriteria/)
3. `await MidiRtpTransportManager.ConnectRtpClientAsync(config)`

`ConnectRtpClientAsync` returns as soon as the service has the entry, and connecting happens in the background. To follow it, read `EntryState` on the entry from `GetConfiguredClients()`, or wait for its endpoint to appear.

**Approving remote devices**

A host with `RemoteClientPolicy` set to `RequireApproval` doesn't let in a remote device it hasn't been told about. RTP-MIDI has no way to tell a device to wait, so the host doesn't answer at all. The remote keeps asking for about twelve seconds, and then gives up.

Call `GetPendingRemoteClients()` every few seconds to find the remotes that are waiting, and decide about each one with `ApproveOrDenyRemoteClientConnectRequestAsync`. An approved remote gets in the next time it asks. A remote that already gave up stays in the list for two minutes after it last asked, so a decision made in that time still counts when it tries again. The service doesn't send a notification, so checking every few seconds is the only way to know.

A remote is known by the name it sends, ignoring uppercase and lowercase differences. RTP-MIDI carries nothing else that stays the same from one connection to the next. There's no authentication, and a device can send any name it likes. So approval keeps out devices nobody expected, but it can't stop a device that sends a name you've already allowed.

**Stopping a host for a while**

`StopRtpHostAsync` ends the host's connections, and nothing can reach it until `StartRtpHostAsync` starts it again. The host stays set up in between.

## Reconnection behavior

Once a client is set up, the service manages its connection for you. What it does when the remote device isn't there depends on how the client finds it.

| Situation | Advertised name | Direct address |
| --------- | --------------- | -------------- |
| Remote not found | Connects as soon as it's advertised | Looked up again 15 seconds later |
| Remote doesn't answer, or turns the connection down | Tried again 15 seconds later | Tried again 15 seconds later |
| Remote ends the connection, or stops answering | Tried again 15 seconds later | Tried again 15 seconds later |

A remote that doesn't answer is asked for about twelve seconds before the service gives up on that try, so the 15 second wait starts after that.

Unlike Network MIDI 2.0, a client with a direct address keeps being retried. RTP-MIDI devices are often at a fixed address, and a device that's switched off and on again should come back without anyone doing anything.

To connect only once, set `AutoReconnect` to false. Then, when a try or a connection ends, the entry is marked `Unavailable` instead of being tried again. A remote that can't be found yet doesn't count as a try, so the service keeps looking for it either way.

An entry marked `Unavailable` stays that way until `ReconnectRtpClientAsync` is called. Use [MidiRtpConfiguredClient]({{ site.baseurl }}/sdk-reference/Transports/Rtp/MidiRtpConfiguredClient/).`EntryState` and `LastErrorCode` to show this in your app. See [MidiRtpClientEntryState]({{ site.baseurl }}/sdk-reference/Transports/Rtp/MidiRtpClientEntryStateEnum/).

## Persistence

Hosts, clients, and decisions made with this API change the running service only, and they're gone when the service restarts. To keep one, also pass the same configuration object to [MidiServiceTransportPluginConfigManager]({{ site.baseurl }}/sdk-reference/ServiceConfig/MidiServiceTransportPluginConfigManager/).`SaveUpdate`.

- To keep a host or a client, save its `MidiRtpHostCreationConfig` or `MidiRtpClientConnectConfig`
- To take one out of the configuration, save a `MidiRtpHostRemovalConfig` or `MidiRtpClientDisconnectConfig`
- To keep a host's allow and deny decisions, save a [MidiRtpHostKnownClientsConfig]({{ site.baseurl }}/sdk-reference/Transports/Rtp/MidiRtpHostKnownClientsConfig/). The service reads those lists when it starts, but never saves them itself
- To see what's saved, call `GetSavedHosts` and `GetSavedClients`. They read the configuration file, so they work even when the service isn't running

## The same device on Network MIDI 2.0

Some computers and devices offer both RTP-MIDI and Network MIDI 2.0. To find out whether a device you're connected to over RTP-MIDI also has Network MIDI 2.0, compare `MidiRtpConnection.RemoteHostName` or `MidiRtpAdvertisedHost.HostName` with [MidiNetworkAdvertisedHost]({{ site.baseurl }}/sdk-reference/Transports/Network/MidiNetworkAdvertisedHost/).`HostName`, ignoring case. A match means the same computer or device, not always the same MIDI port, because one device can offer several.

When a device has both, Network MIDI 2.0 is the better choice. The service doesn't stop anyone connecting to one device both ways, and that's the customer's decision to make. If your app finds a match, tell the customer, rather than refusing.

> If you have a choice, we recommend using Network MIDI 2.0 instead of RTP-MIDI.

## Current limitations

- RTP-MIDI carries MIDI 1.0 messages only. MIDI 2.0 messages sent to an RTP-MIDI endpoint are translated to MIDI 1.0 where MIDI 1.0 has an equivalent
- mDNS discovery only works on the local subnet. Direct connections don't have that limit
- A connection's endpoint exists only while the connection is up
