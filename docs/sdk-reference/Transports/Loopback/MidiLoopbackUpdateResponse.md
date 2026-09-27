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
| `Success` | True if it worked |
| `ErrorCode` | A `MidiLoopbackErrorCode`, if `Success` is false |
| `ErrorMessage` | An error message people can read, if `Success` is false |
