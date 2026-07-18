---
layout: sdk_reference_page
title: MidiLoopbackCreationResponse
namespace: Windows.Devices.Midi2.Transports.Loopback
type: runtimeclass
description: Response from the attempt to create a loopback endpoint pair
---

This class represents the result of an attempt to create runtime transient loopback endpoints.

## Properties

| Property | Description |
|---|---|
| `Success` | True if the creation of both endpoints was a success |
| `ErrorCode` | A `MidiLoopbackErrorCode` value if `Success` is false |
| `ErrorMessage` | A human-readable error message if `Success` is false |
| `CreatedLoopbackEntry` | A `MidiLoopbackEntry` object with information about the created loopback pair, if successful |
