---
layout: sdk_reference_page
title: MidiRtpAdvertisedHost
namespace: Windows.Devices.Midi2.Transports.Rtp
type: runtimeclass
description: An RTP-MIDI device advertising itself on the local network
---

Returned by `MidiRtpTransportManager.GetAdvertisedHosts()`. Each one is an RTP-MIDI device the service can see advertised on the local network right now.

## Properties

| Property | Description |
| -------- | ----------- |
| `ServiceInstanceName` | The name the device advertises. Put it in `MidiRtpClientMatchCriteria.ServiceInstanceName` to connect to the device |
| `HostName` | The device's DNS host name, such as `Studio-Mac.local` |
| `Port` | The port the device listens on. RTP-MIDI also uses the port after it |
| `IPAddresses` | Every address the device advertised, IPv4 first. There may be more than one |
| `IPv4Addresses` | Just the IPv4 addresses |
| `IPv6Addresses` | Just the IPv6 addresses |
| `IsThisPc` | True for a host on this PC. A list of devices to connect to should leave these out |

## Remarks

Connect by `ServiceInstanceName` when you can. Addresses can change, and the service finds the device again by its name.

One device can advertise several entries, each with its own name and port. For example, a MIDI interface with several network ports lists each one. They all have the same `HostName`.

`HostName` is also how you find out whether a device has Network MIDI 2.0 as well. Compare it with `MidiNetworkAdvertisedHost.HostName`, ignoring case.
