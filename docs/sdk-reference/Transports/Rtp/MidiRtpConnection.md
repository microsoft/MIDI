---
layout: sdk_reference_page
title: MidiRtpConnection
namespace: Windows.Devices.Midi2.Transports.Rtp
type: runtimeclass
description: One connection between this PC and a remote RTP-MIDI device
---

Found in `MidiRtpConfiguredHost.Connections` and `MidiRtpConfiguredClient.Connection`. A connection works the same way whichever end started it, so hosts and clients both report their connections with this type.

## Properties

| Property | Description |
| -------- | ----------- |
| `ConnectionId` | Identifies this connection within its host or client entry. Pass it to `DisconnectRemoteClientAsync` to end a connection to a host |
| `RemoteName` | The name the remote device sent. The endpoint is named after it, unless the client has a `CustomEndpointName` |
| `RemoteAddress` | The remote device's address right now. For display only. It can change between connections, so don't use it to recognize the device |
| `RemotePort` | The remote device's port right now. For display only |
| `LocalPort` | This PC's port for the connection. RTP-MIDI also uses the port after it |
| `RemoteHostName` | The remote device's DNS host name, such as `Studio-Mac.local`, from its RTP-MIDI advertisement. Empty when the device doesn't advertise, or when two advertised devices list the same address |
| `IsConnected` | False while the two devices are still setting up the connection |
| `ThisPcInvited` | True when this PC started the connection. That's always so for a client, and never for a host |
| `EndpointDeviceId` | The id of the MIDI endpoint made for this connection. Empty until the connection is up |
| `CurrentLatencyTicks` | The average round trip time of the recent clock exchanges between the two devices, in MIDI timestamp ticks. Zero until the first exchange is done |
| `BestLatencyTicks` | The shortest of those round trip times, in MIDI timestamp ticks |
| `TotalCountNetworkPacketsSent` | The number of packets sent to the remote device |
| `TotalCountNetworkPacketsReceived` | The number of packets received from the remote device |
| `TotalCountPacketsLost` | The number of packets from the remote device that never arrived |
| `TotalCountLossesRepairedFromJournal` | How many of those losses were repaired from the recovery journal |
| `TotalCountNoteOffsRecovered` | How many Note Off messages were put back after a loss, so no note was left hanging |
| `TotalCountMessagesSent` | The number of MIDI messages sent to the remote device |
| `TotalCountMessagesReceived` | The number of MIDI messages received from the remote device |

## Remarks

RTP-MIDI never sends a lost packet again. Instead, each packet carries a recovery journal, a short summary of what came before it. When a packet goes missing, the next one that arrives is used to repair the damage, for example by ending a note whose Note Off was lost. A note that can't be repaired is ended anyway, so it doesn't hang.

To show latency in milliseconds, use `MidiClock.ConvertTimestampTicksToMilliseconds`.

`RemoteHostName` is the same for everything a device advertises, so it's how you find out whether a device you're connected to also has Network MIDI 2.0. Compare it with `MidiNetworkAdvertisedHost.HostName`, ignoring case. See the [namespace overview]({{ site.baseurl }}/sdk-reference/Transports/Rtp/).

This is a copy taken when you called `GetConfiguredHosts()` or `GetConfiguredClients()`. Call it again to refresh.
