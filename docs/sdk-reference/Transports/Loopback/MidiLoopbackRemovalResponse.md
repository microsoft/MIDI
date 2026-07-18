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
| `Success` | True if the removal succeeded |
| `ErrorCode` | A `MidiLoopbackErrorCode` value if `Success` is false |
| `ErrorMessage` | A human-readable error message if `Success` is false |
