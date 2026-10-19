---
layout: sdk_reference_page
title: MidiNetworkHostRemoteClientSettingsConfig
namespace: Windows.Devices.Midi2.Transports.Network
type: runtimeclass
implements: Windows.Devices.Midi2.ServiceConfig.IMidiServiceTransportPluginConfig
description: The remote clients of a Network MIDI 2.0 host that have a sending speed of their own
---

Pass this to `MidiServiceTransportPluginConfigManager.SendUpdate` to change a running host, and to `SaveUpdate` to keep the change after the service restarts.

## Constructors

| Constructor | Description |
| -------- | ----------- |
| `MidiNetworkHostRemoteClientSettingsConfig()` | Creates an empty configuration |
| `MidiNetworkHostRemoteClientSettingsConfig(hostId)` | Creates a configuration for this host |

## Properties

| Property | Description |
| -------- | ----------- |
| `HostId` | The GUID of the host entry these settings belong to |
| `RemoteClientSettings` | Every [MidiNetworkRemoteClientSettings]({{ site.baseurl }}/sdk-reference/Transports/Network/MidiNetworkRemoteClientSettings/) the host has |

## Remarks

`RemoteClientSettings` must hold every remote client with settings of its own, not just the ones that changed. The host's whole list is replaced with what it holds. So start from `MidiNetworkConfiguredHost.RemoteClientSettings`, which is the list the running service holds, change it, and send the whole thing. Leaving a client out is how you put it back on the host's speed and ports. An empty list puts every client back on the host's settings.

An entry can hold a speed, MIDI 1.0 ports, or both. When you change one, keep the other: copy the client's `FallbackMidi1PortGroups` into its new entry when you change its speed, and its speed when you change its ports. To put a client back on the host's speed but keep its ports, keep the entry and set `UseHostSendSpeed` to true.

`SendUpdate` changes the running host. Clients that are already connected change speed and ports right away, and they aren't disconnected. A client that connects later gets its own settings from the start.

`SaveUpdate` keeps the list after the service restarts. The settings are saved with the host. If the host isn't saved, `SaveUpdate` returns `ErrorEntryNotSaved` and writes nothing. Send the change first, and save it only if the service took it.

A host keeps settings for up to 256 remote clients. Removing a host removes its clients' settings too.

Let the customer choose these speeds. Read them to show what's set, but don't change them without the customer's permission. Network MIDI Setup is the main place a customer manages them.
