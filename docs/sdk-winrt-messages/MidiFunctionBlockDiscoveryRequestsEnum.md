---
layout: api_page
title: MidiFunctionBlockDiscoveryRequests
parent: Midi2.Messages
has_children: false
---

# MidiFunctionBlockDiscoveryFilterFlags Enumeration

Used to indicate which function block messages you want to receive when you request function blocks.

## Location

| Namespace | Microsoft.Windows.Devices.Midi2.Messages |
| Library | Microsoft.Windows.Devices.Midi2 |

## Properties

| Property | Value | Description |
| -------- | ------- | ------ |
| `None` | `0x00000000` | No information requested |
| `RequestFunctionBlockInformation` | `0x00000001` | Request the core function block information |
| `RequestFunctionBlockName` | `0x00000002` | Request a set of function block name messages |

## IDL

[MidiFunctionBlockDiscoveryRequests IDL](https://github.com/microsoft/MIDI/blob/main/src/app-sdk/winrt/MidiFunctionBlockDiscoveryRequestsEnum.idl)
