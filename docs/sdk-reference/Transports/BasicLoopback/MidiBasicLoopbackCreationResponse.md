---
layout: sdk_reference_page
title: MidiBasicLoopbackCreationResponse
namespace: Windows.Devices.Midi2.Transports.BasicLoopback
type: runtimeclass
description: Response from the attempt to create a basic MIDI 1.0-style loopback endpoint
---

The result of trying to create a temporary basic loopback endpoint.

## Properties

| Property | Description |
|---|---|
| `Success` | True if the endpoint was created |
| `ErrorCode` | A `MidiBasicLoopbackErrorCode`, if `Success` is false |
| `ErrorMessage` | An error message people can read, if `Success` is false |
| `CreatedLoopbackEntry` | A `MidiBasicLoopbackEntry` with information about the new loopback, if it worked |
