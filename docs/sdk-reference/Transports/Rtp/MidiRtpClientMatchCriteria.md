---
layout: sdk_reference_page
title: MidiRtpClientMatchCriteria
namespace: Windows.Devices.Midi2.Transports.Rtp
type: runtimeclass
description: Says which remote RTP-MIDI device a client connects to
---

Holds either the name a remote device advertises, or its address and port. Used by `MidiRtpClientConnectConfig.MatchCriteria`. Set one or the other, not both.

## Constructors

| Constructor | Description |
| -------- | ----------- |
| `MidiRtpClientMatchCriteria()` | Creates empty criteria, with `DirectPort` set to `5004` |

## Properties

| Property | Description |
| -------- | ----------- |
| `ServiceInstanceName` | The name the remote device advertises, from `MidiRtpAdvertisedHost.ServiceInstanceName`. The device is found again by this name when its address changes |
| `DirectHostNameOrIPAddress` | An IPv4 or IPv6 address, or a host name that's looked up each time the client connects, for a direct connection |
| `DirectPort` | The remote device's port, for a direct connection. RTP-MIDI also uses the port after it. `5004` unless you change it |

## Remarks

Your choice decides what the service does when the remote device can't be found. The service keeps looking for an advertised name, and connects as soon as the device is advertised. A direct address is looked up again every 15 seconds. See the [namespace overview]({{ site.baseurl }}/sdk-reference/Transports/Rtp/) for the full table.

Use the advertised name when you can. A device's address can change, for example when a router hands out addresses, but its advertised name usually stays the same.
