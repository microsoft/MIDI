---
layout: sdk_reference_page
title: MidiFunctionBlockRepresentsMidi10Connection
namespace: Windows.Devices.Midi2.Enumeration
type: enum
description: Indicates if a function block represents a MIDI 1.0 connection on the device
---

Indicates the MIDI 1.0 capability restrictions for a function block. Note that Windows MIDI Services does not currently throttle the speed of outbound messages, even if the block indicates it has restricted bandwidth.

## Properties

| Property | Value | Description |
| -------- | ------- | ------ |
| `Not10` | `0x00000000` | This function block is not a MIDI 1.0 function. |
| `YesBandwidthUnrestricted` | `0x00000001` | This block represents a MIDI 1.0 function, but has the ability to receive messages faster than the original MIDI 1.0 protocol speed. |
| `YesBandwidthRestricted` | `0x00000002` | This block represents a MIDI 1.0 function, and applications should take care to send messages to it at the normal MIDI 1.0 protocol speed. |
| `Reserved` | `0x00000003` | Reserved for future use. |
