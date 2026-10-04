---
layout: sdk_reference_page
title: MidiRtpHostRemoteClientSettingsConfig
namespace: Windows.Devices.Midi2.Transports.Rtp
type: runtimeclass
implements: Windows.Devices.Midi2.ServiceConfig.IMidiServiceTransportPluginConfig
description: The remote devices of an RTP-MIDI host that have a sending speed of their own
---

Pass this to `MidiServiceTransportPluginConfigManager.SendUpdate` to change a running host, and to `SaveUpdate` to keep the change after the service restarts.

## Constructors

| Constructor | Description |
| -------- | ----------- |
| `MidiRtpHostRemoteClientSettingsConfig()` | Creates an empty configuration |
| `MidiRtpHostRemoteClientSettingsConfig(hostId)` | Creates a configuration for this host |

## Properties

| Property | Description |
| -------- | ----------- |
| `HostId` | The GUID of the host entry these settings belong to |
| `RemoteClientSettings` | Every [MidiRtpRemoteClientSettings]({{ site.baseurl }}/sdk-reference/Transports/Rtp/MidiRtpRemoteClientSettings/) the host has |

## Remarks

`RemoteClientSettings` must hold every remote device with a speed of its own, not just the ones that changed. The host's whole list is replaced with what it holds. So start from `MidiRtpConfiguredHost.RemoteClientSettings`, which is the list the running service holds, change it, and send the whole thing. Leaving a device out is how you put it back on the host's speed. An empty list puts every device back on the host's speed.

`SendUpdate` changes the running host. Devices that are already connected change speed right away, and they aren't disconnected. A device that connects later gets its own speed from the start. Changing the host itself, by creating it again with the same `HostId`, keeps the list.

`SaveUpdate` keeps the list after the service restarts. The settings are saved beside the host, like its remembered decisions, and they're only used while the host is saved. If the host isn't saved, `SaveUpdate` returns `ErrorEntryNotSaved` and writes nothing. Send the change first, and save it only if the service took it.

A host keeps settings for up to 256 remote devices. Removing a host removes its devices' settings too, and saving a `MidiRtpHostRemovalConfig` takes them out of the configuration file along with the host.

Let the customer choose these speeds. Read them to show what's set, but don't change them without the customer's permission. Network MIDI Setup is the main place a customer manages them.
