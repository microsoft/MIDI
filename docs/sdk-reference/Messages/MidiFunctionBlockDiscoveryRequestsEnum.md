---
layout: sdk_reference_page
title: MidiFunctionBlockDiscoveryRequests
namespace: Microsoft.Windows.Devices.Midi2.Messages
library: Microsoft.Windows.Devices.Midi2.dll
type: enum
description: MIDI 2.0 function block discovery request flags
---

Used to indicate which function block messages you want to receive when you request function blocks.

## Properties

| Property | Value | Description |
| -------- | ------- | ------ |
| `None` | `0x00000000` | No information requested |
| `RequestFunctionBlockInformation` | `0x00000001` | Request the core function block information |
| `RequestFunctionBlockName` | `0x00000002` | Request a set of function block name messages |
