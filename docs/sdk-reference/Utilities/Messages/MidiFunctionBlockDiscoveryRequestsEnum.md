---
layout: sdk_reference_page
title: MidiFunctionBlockDiscoveryRequests
namespace: Windows.Devices.Midi2.Utilities.Messages
type: enum
description: MIDI 2.0 function block discovery request flags
---

Says which function block messages you want back when you ask for function blocks. This is a flags enumeration, so you can combine values.

## Properties

| Property | Value | Description |
| -------- | ------- | ------ |
| `None` | `0x00000000` | Ask for nothing |
| `RequestFunctionBlockInfo` | `0x00000001` | Ask for the main function block information |
| `RequestFunctionBlockName` | `0x00000002` | Ask for the function block name messages |
