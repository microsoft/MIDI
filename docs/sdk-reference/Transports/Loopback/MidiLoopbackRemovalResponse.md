---
layout: sdk_reference_page
title: MidiLoopbackRemovalResponse
namespace: Windows.Devices.Midi2.Transports.Loopback
type: runtimeclass
description: Response from the attempt to remove a loopback endpoint pair
---

Returned by `MidiLoopbackManager.RemoveTransientLoopback()`.

## Properties

| Property | Description |
| -------- | ----------- |
| `Success` | True if the loopback pair was removed |
| `ErrorCode` | A `MidiLoopbackErrorCode`, if `Success` is false |
| `ErrorMessage` | An error message people can read, if `Success` is false |
