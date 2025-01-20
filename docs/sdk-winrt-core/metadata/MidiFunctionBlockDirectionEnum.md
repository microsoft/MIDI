---
layout: api_page
title: MidiFunctionBlockDirection
parent: Metadata
grand_parent: Midi2 core
has_children: false
---

# MidiFunctionBlockDirection Enumeration

Indicates the message flow for a function block. Note that this is, per the specification, from the function block's point of view. So, for example, a function block specifying `BlockOutput` would be a sender of messages, and therefore used as an input in the API.

## Location

| Namespace | Microsoft.Windows.Devices.Midi2 |
| Library | Microsoft.Windows.Devices.Midi2 |

## Properties

| Property | Value | Description |
| -------- | ------- | ------ |
| `Undefined` | `0x0` | Unknown or undefined |
| `BlockInput` | `0x1` | This block represents an input function, from the block's point of view. |
| `BlockOutput` | `0x2` | This block represents an output function, from the block's point of view. |
| `Bidirectional` | `0x3` | This block represents a bidirectional function. |

## IDL

[MidiFunctionBlockDirectionEnum IDL](https://github.com/microsoft/MIDI/blob/main/src/app-sdk/winrt/MidiFunctionBlockDirectionEnum.idl)
