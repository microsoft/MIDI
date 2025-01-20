---
layout: api_page
title: MidiMessageType
parent: Messages
grand_parent: Midi2 core
has_children: false
---

# MidiMessageType Enumeration

The values correspond directly to the "mt" field in the MIDI UMP packet and may be cast as such if trimmed to 4 bits and shifted into place.

## Location

| Namespace | Microsoft.Windows.Devices.Midi2 |
| Library | Microsoft.Windows.Devices.Midi2 |

## Properties

| Property | Value | Description |
| -------- | ------- | ------ |
| `UtilityMessage32` | `0x0` | 32-bit utility message |
| `SystemCommon32` | `0x1` | 32-bit system common message |
| `Midi1ChannelVoice32` | `0x2` | 32-bit MIDI 1.0 channel voice message |
| `DataMessage64` | `0x3` | 64-bit data message (including MIDI 1.0 System Exclusive) |
| `Midi2ChannelVoice64` | `0x4` | 64-bit MIDI 2.0 channel voice message |
| `DataMessage128` | `0x5` | 128-bit Data Message |
| `FutureReserved632` | `0x6` | Reserved for future use by the MIDI standards bodies |
| `FutureReserved732` | `0x7` | Reserved for future use by the MIDI standards bodies |
| `FutureReserved864` | `0x8` | Reserved for future use by the MIDI standards bodies |
| `FutureReserved964` | `0x9` | Reserved for future use by the MIDI standards bodies |
| `FutureReservedA64` | `0xA` | Reserved for future use by the MIDI standards bodies |
| `FutureReservedB96` | `0xB` | Reserved for future use by the MIDI standards bodies |
| `FutureReservedC96` | `0xC` | Reserved for future use by the MIDI standards bodies |
| `FlexData128` | `0xD` | 128-bit Flex Data message including song file data messages |
| `FutureReservedE128` | `0xE` | Reserved for future use by the MIDI standards bodies |
| `Stream128` | `0xF` | 128-bit stream message, including endpoint discovery and function block messages |

## IDL

[MidiMessageType IDL](https://github.com/microsoft/MIDI/blob/main/src/app-sdk/winrt/MidiMessageTypeEnum.idl)
