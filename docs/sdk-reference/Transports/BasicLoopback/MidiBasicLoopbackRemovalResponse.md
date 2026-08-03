---
layout: sdk_reference_page
title: MidiBasicLoopbackRemovalResponse
namespace: Windows.Devices.Midi2.Transports.BasicLoopback
type: runtimeclass
description: Response from the attempt to remove a basic MIDI 1.0-style loopback endpoint
---

Returned by `MidiBasicLoopbackManager.RemoveTransientLoopback()`.

## Properties

| Property | Description |
| -------- | ----------- |
| `Success` | True if the removal succeeded |
| `ErrorCode` | A `MidiBasicLoopbackErrorCode` value if `Success` is false |
| `ErrorMessage` | A human-readable error message if `Success` is false |
