---
layout: sdk_reference_page
title: MidiCustomizationLatencySource
namespace: Windows.Devices.Midi2.ServiceConfig
type: enum
idl: MidiServiceEndpointCustomizationProvenance.idl
description: Whether a stored outgoing latency value was measured or typed in.
---

Stored on `MidiServiceEndpointCustomizationProvenance`, along with the date of any measurement.

It lets anything that offers to delete or replace a customization say what kind of value it's about to throw away. A typed value can be typed again from memory. A measured one took a loopback cable, a measurement run, and some patience. It's the hardest thing in a stored customization to get back.

## Properties

| Property | Value | Description |
| --- | --- | --- |
| `Unspecified` | `0x00000000` | No latency value is stored, or it was stored before this was recorded |
| `Entered` | `0x00000001` | Someone typed in the value |
| `Measured` | `0x00000002` | The value came from a measurement. `LatencyMeasured` says when |
