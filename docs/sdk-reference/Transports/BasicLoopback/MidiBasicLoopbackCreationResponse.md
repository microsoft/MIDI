---
layout: sdk_reference_page
title: MidiBasicLoopbackCreationResponse
namespace: Windows.Devices.Midi2.Transports.BasicLoopback
type: runtimeclass
description: Response from the attempt to create a basic MIDI 1.0-style loopback endpoint
---

This class represents the result of an attempt to create a runtime transient basic loopback endpoint.

## Properties

| Property | Description |
|---|---|
| `Success` | True if the creation of the endpoint was successful |
| `ErrorCode` | A `MidiBasicLoopbackErrorCode` value if `Success` is false |
| `ErrorMessage` | A human-readable error message if `Success` is false |
| `CreatedLoopbackEntry` | A `MidiBasicLoopbackEntry` object with information about the created loopback, if successful |
