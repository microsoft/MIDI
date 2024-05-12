---
layout: api_page
title: MidiFunctionBlockDiscoveryFilterFlags
parent: Messages
grand_parent: Microsoft.Devices.Midi2
has_children: false
---

# MidiFunctionBlockDiscoveryFilterFlags Enumeration

Used to indicate which function block messages you want to receive when you request function blocks.

## Properties

| Property | Value | Description |
| -------- | ------- | ------ |
| `None` | `0x00000000` | No information requested |
| `RequestFunctionBlockInformation` | `0x00000001` | Request the core function block information |
| `RequestFunctionBlockName` | `0x00000002` | Request a set of function block name messages |

## IDL

[MidiFunctionBlockDiscoveryFilterFlagsEnum IDL](https://github.com/microsoft/MIDI/blob/main/src/api/Client/Midi2Client/MidiFunctionBlockDiscoveryFilterFlagsEnum.idl)
