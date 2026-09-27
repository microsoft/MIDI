---
layout: sdk_reference_page
title: MidiProfileId
namespace: Windows.Devices.Midi2.CapabilityInquiry
type: runtimeclass
implements: Windows.Foundation.IStringable
description: The five-byte identifier of a MIDI-CI profile
---

A profile is identified by five bytes. The first byte says what kind of profile it is. It's `0x7E` for a profile the MIDI Association and AMEI have adopted, and a manufacturer's own System Exclusive id for any other profile.

The other four bytes mean different things for each kind, which is why you can read them both as plain bytes and through the named properties. For a standard profile, they're the bank, the number, the version, and the level. For a manufacturer's profile, the first two are the rest of that manufacturer's System Exclusive id, and the last two are whatever the manufacturer wants. A manufacturer with a one-byte id puts it in the first byte and sets the next two to zero, just as it would in any other System Exclusive message.

The version and level are part of the identifier. A device that offers two versions of one profile is offering two profiles, which is why `IsSameProfileAs` compares all five bytes.

## Constructors

| Constructor | Description |
| ----------- | ----------- |
| `MidiProfileId()` | Creates an empty identifier |
| `MidiProfileId(idByte1, idByte2, idByte3, idByte4, idByte5)` | Creates the identifier from its five bytes |

## Properties

| Property | Description |
| -------- | ----------- |
| `IdByte1` | The first byte, which says what kind of profile this is |
| `IdByte2` | The second byte |
| `IdByte3` | The third byte |
| `IdByte4` | The fourth byte |
| `IdByte5` | The fifth byte |
| `IsStandardDefined` | True when the first byte is `0x7E`, which means a profile adopted by the MIDI Association and AMEI |
| `ProfileBank` | The bank of a standard profile. Zero for a manufacturer's profile, where that byte means something else |
| `ProfileNumber` | The number of a standard profile. Zero for a manufacturer's profile |
| `ProfileVersion` | The version of a standard profile. Zero for a manufacturer's profile |
| `ProfileLevel` | The level of a standard profile. Zero for a manufacturer's profile |

## Methods

| Method | Description |
| ------ | ----------- |
| `IsSameProfileAs(other)` | True when all five bytes match. Returns false if `other` is null |
| `ToString` | (From `IStringable`) The five bytes, in the order they're sent |

## Static Properties

| Static Property | Description |
| --------------- | ----------- |
| `StandardDefinedIdByte1` | The first byte of a profile adopted by the MIDI Association and AMEI: `0x7E` |

## Static Methods

| Static Method | Description |
| ------------- | ----------- |
| `CreateStandardDefined(profileBank, profileNumber, profileVersion, profileLevel)` | Creates the identifier of a standard profile |
| `CreateManufacturerSpecific(manufacturerSysExIdByte1, manufacturerSysExIdByte2, manufacturerSysExIdByte3, manufacturerInfoByte1, manufacturerInfoByte2)` | Creates the identifier of a manufacturer's own profile |
