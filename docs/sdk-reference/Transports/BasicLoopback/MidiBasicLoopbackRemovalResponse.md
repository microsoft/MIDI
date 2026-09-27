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
| `Success` | True if the loopback was removed |
| `ErrorCode` | A `MidiBasicLoopbackErrorCode`, if `Success` is false |
| `ErrorMessage` | An error message people can read, if `Success` is false |
