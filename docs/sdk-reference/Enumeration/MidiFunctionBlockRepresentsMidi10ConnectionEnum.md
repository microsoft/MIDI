---
layout: sdk_reference_page
title: MidiFunctionBlockRepresentsMidi10Connection
namespace: Windows.Devices.Midi2.Enumeration
type: enum
description: Indicates if a function block represents a MIDI 1.0 connection on the device
---

Says whether a function block stands for a MIDI 1.0 connection, and whether that connection is limited to MIDI 1.0 speed. Windows MIDI Services doesn't currently slow down outgoing messages, even when a block says its speed is limited.

## Properties

| Property | Value | Description |
| -------- | ------- | ------ |
| `Not10` | `0x00000000` | This function block isn't a MIDI 1.0 connection. |
| `YesBandwidthUnrestricted` | `0x00000001` | This block is a MIDI 1.0 connection, but it can receive messages faster than MIDI 1.0 speed. |
| `YesBandwidthRestricted` | `0x00000002` | This block is a MIDI 1.0 connection. Send messages to it no faster than normal MIDI 1.0 speed. |
| `Reserved` | `0x00000003` | Set aside for future use. |
