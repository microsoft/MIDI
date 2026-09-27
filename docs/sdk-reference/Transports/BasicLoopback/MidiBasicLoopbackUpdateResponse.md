---
layout: sdk_reference_page
title: MidiBasicLoopbackUpdateResponse
namespace: Windows.Devices.Midi2.Transports.BasicLoopback
type: runtimeclass
description: Response from a mute or unmute operation on a basic MIDI 1.0-style loopback endpoint
---

Returned by `MidiBasicLoopbackManager.MuteLoopback()` and `MidiBasicLoopbackManager.UnmuteLoopback()`.

## Properties

| Property | Description |
| -------- | ----------- |
| `Success` | True if it worked |
| `ErrorCode` | A `MidiBasicLoopbackErrorCode`, if `Success` is false |
| `ErrorMessage` | An error message people can read, if `Success` is false |
