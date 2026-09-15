---
layout: sdk_reference_page
title: MidiCustomizationLatencySource
namespace: Windows.Devices.Midi2.ServiceConfig
type: enum
description: Whether a stored outgoing latency value was measured or typed in.
---

Recorded on `MidiServiceEndpointCustomizationProvenance`, alongside the date any measurement was taken.

This exists so that anything offering to discard or overwrite a customization can say which kind of value it is about to throw away. A typed value can be retyped from memory. A measured one needed a loopback cable, a measurement run and the patience to do it, and is the single most expensive thing a stored customization can contain.

## Properties

| Property | Value | Description |
| --- | --- | --- |
| `Unspecified` | `0x00000000` | No latency value is stored, or it predates this being recorded |
| `Entered` | `0x00000001` | A customer supplied the value directly |
| `Measured` | `0x00000002` | The value came from a measurement. See `LatencyMeasured` for when |
