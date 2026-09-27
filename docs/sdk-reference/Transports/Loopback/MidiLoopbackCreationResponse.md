---
layout: sdk_reference_page
title: MidiLoopbackCreationResponse
namespace: Windows.Devices.Midi2.Transports.Loopback
type: runtimeclass
description: Response from the attempt to create a loopback endpoint pair
---

The result of trying to create a temporary loopback endpoint pair.

## Properties

| Property | Description |
|---|---|
| `Success` | True if both endpoints were created |
| `ErrorCode` | A `MidiLoopbackErrorCode`, if `Success` is false |
| `ErrorMessage` | An error message people can read, if `Success` is false |
| `CreatedLoopbackEntry` | A `MidiLoopbackEntry` with information about the new loopback pair, if it worked |
