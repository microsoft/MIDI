---
layout: sdk_reference_page
title: MidiEndpointDeviceInformationFilters
namespace: Windows.Devices.Midi2.Enumeration
type: enum
description: Filter used when enumerating endpoints
---

When you list devices, it helps to be able to pick which kinds of endpoints to include. For example, a diagnostic or developer tool might want the diagnostic loopback endpoints. A digital audio workstation (DAW), on the other hand, only wants the normal endpoints, whether they natively use UMP or the MIDI 1.0 byte format.

## Properties

| Property | Value | Description |
| --------------- | ---------- | ----------- |
| `StandardNativeUniversalMidiPacketFormat` | `0x00000001` | Include endpoints that natively use UMP. These are usually thought of as MIDI 2.0 devices, even if they only send MIDI 1.0 messages in UMP. |
| `StandardNativeMidi1ByteFormat` | `0x00000002` | Include endpoints that natively use the MIDI 1.0 byte format. Windows MIDI Services converts their messages to and from UMP. |
| `VirtualDeviceResponder` | `0x00000100` | Include the device side of virtual devices, not the side other applications use. You usually won't need this. |
| `DiagnosticLoopback` | `0x00010000` | Include the diagnostic loopback endpoints. Use this only in development, test, or diagnostic tools. |
| `DiagnosticPing` | `0x00020000` | Include the diagnostic ping endpoint. You normally wouldn't, because this endpoint is only for the service's own use. |
| `AllStandardEndpoints` | `0x00000003` | `StandardNativeUniversalMidiPacketFormat` and `StandardNativeMidi1ByteFormat` together. This is the default, and it's the value most applications should use. |

