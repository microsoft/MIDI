---
layout: sdk_reference_page
title: MidiLoopbackUpdateResponse
namespace: Windows.Devices.Midi2.Transports.Loopback
type: runtimeclass
description: Response from a mute or unmute operation on a loopback endpoint pair
---

Returned by `MidiLoopbackManager.MuteLoopback()` and `MidiLoopbackManager.UnmuteLoopback()`.

## Properties

| Property | Description |
| -------- | ----------- |
| `Success` | True if the operation succeeded |
| `ErrorCode` | A `MidiLoopbackErrorCode` value if `Success` is false |
| `ErrorMessage` | A human-readable error message if `Success` is false |
