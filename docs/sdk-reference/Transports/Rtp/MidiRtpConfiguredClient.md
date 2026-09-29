---
layout: sdk_reference_page
title: MidiRtpConfiguredClient
namespace: Windows.Devices.Midi2.Transports.Rtp
type: runtimeclass
description: Information about an RTP-MIDI client entry set up in the service
---

Returned by `MidiRtpTransportManager.GetConfiguredClients()`.

## Properties

| Property | Description |
| -------- | ----------- |
| `ClientId` | The GUID that identifies this client entry |
| `Name` | What the remote device shows for this PC. When the client was created with an empty name, this is this PC's name |
| `EntryState` | Where this entry is in its life. See `MidiRtpClientEntryState` |
| `IsDirectConnection` | True if the client was set up with an address and port, instead of an advertised name |
| `RemoteServiceInstanceName` | The advertised name the client connects to. Empty for a direct connection |
| `ConfiguredDirectAddress` | The address or host name the client was set up with, for a direct connection |
| `ConfiguredDirectPort` | The port the client was set up with. Only meaningful when `IsDirectConnection` is true |
| `CustomEndpointName` | The name chosen for this connection's endpoint, or empty to use the name the remote device sends |
| `AutoReconnect` | True if the service connects again whenever a try or a connection ends |
| `IsEnabled` | True if the entry is turned on in the service |
| `SendRecoveryJournal` | True if this PC sends a recovery journal with each packet |
| `LastErrorCode` | The HRESULT from the last try, or `0`. See the table below |
| `Connection` | The [MidiRtpConnection]({{ site.baseurl }}/sdk-reference/Transports/Rtp/MidiRtpConnection/), or null when there isn't one |

## Common values of LastErrorCode

| Value | Meaning |
| ----- | ------- |
| `0x800704D0` | The remote device couldn't be found. Its advertised name isn't on the network right now, or its host name couldn't be looked up |
| `0x800705B4` | The remote device didn't answer |
| `0x80070005` | The remote device turned the connection down |
| `0x800704CA` | The remote device ended the connection |
| `0x800704D4` | The remote device stopped answering while it was connected |

Other values come from Windows networking.

## Remarks

Every client entry is reported, whether or not it's connected, so an entry that has never reached its remote device still shows up. Use `EntryState` to tell the cases apart.

This is a copy taken when you called `GetConfiguredClients()`, not an object that updates itself. Call it again to refresh.
