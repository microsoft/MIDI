---
layout: sdk_reference_page
title: MidiMessageType
namespace: Windows.Devices.Midi2
type: enum
description: Represents the type of a UMP message
---

The values correspond directly to the "mt" field in the MIDI UMP packet and may be cast as such if trimmed to 4 bits and shifted into place.

## Properties

| Property | Value | Description |
| -------- | ------- | ------ |
| `UtilityMessage32` | `0x00000000` | 32-bit utility message |
| `SystemCommon32` | `0x00000001` | 32-bit system common message |
| `Midi1ChannelVoice32` | `0x00000002` | 32-bit MIDI 1.0 channel voice message |
| `DataMessage64` | `0x00000003` | 64-bit data message (including MIDI 1.0 System Exclusive) |
| `Midi2ChannelVoice64` | `0x00000004` | 64-bit MIDI 2.0 channel voice message |
| `DataMessage128` | `0x00000005` | 128-bit Data Message |
| `FutureReserved632` | `0x00000006` | Reserved for future use by the MIDI standards bodies |
| `FutureReserved732` | `0x00000007` | Reserved for future use by the MIDI standards bodies |
| `FutureReserved864` | `0x00000008` | Reserved for future use by the MIDI standards bodies |
| `FutureReserved964` | `0x00000009` | Reserved for future use by the MIDI standards bodies |
| `FutureReservedA64` | `0x0000000A` | Reserved for future use by the MIDI standards bodies |
| `FutureReservedB96` | `0x0000000B` | Reserved for future use by the MIDI standards bodies |
| `FutureReservedC96` | `0x0000000C` | Reserved for future use by the MIDI standards bodies |
| `FlexData128` | `0x0000000D` | 128-bit Flex Data message including song file data messages |
| `FutureReservedE128` | `0x0000000E` | Reserved for future use by the MIDI standards bodies |
| `Stream128` | `0x0000000F` | 128-bit stream message, including endpoint discovery and function block messages |
