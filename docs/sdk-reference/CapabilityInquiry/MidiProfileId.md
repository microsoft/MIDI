---
layout: sdk_reference_page
title: MidiProfileId
namespace: Windows.Devices.Midi2.CapabilityInquiry
type: runtimeclass
implements: Windows.Foundation.IStringable
description: The five byte identifier of a MIDI-CI profile
---

A profile is identified by five bytes. The first says which kind it is: `0x7E` for a profile the MIDI Association and AMEI have adopted, and a manufacturer's own System Exclusive identifier for anything else.

The remaining four bytes mean different things in each case, which is why they are readable both as raw bytes and through the named accessors. For a standard defined profile they are the bank, the number, the version and the level. For a manufacturer specific profile the first two are the rest of that manufacturer's System Exclusive identifier and the last two are whatever the manufacturer wants. A manufacturer with a one byte identifier puts it in the first byte and leaves the next two as zero, exactly as it would in any other System Exclusive message.

Version and level are part of the identity. A device offering two versions of one profile is offering two profiles, and `IsSameProfileAs` compares all five bytes accordingly.

## Constructors

| Constructor | Description |
| ----------- | ----------- |
| `MidiProfileId()` | Constructs an empty identifier |
| `MidiProfileId(idByte1, idByte2, idByte3, idByte4, idByte5)` | Constructs the identifier from its five bytes |

## Properties

| Property | Description |
| -------- | ----------- |
| `IdByte1` | The first byte, which says what kind of profile this is |
| `IdByte2` | The second byte |
| `IdByte3` | The third byte |
| `IdByte4` | The fourth byte |
| `IdByte5` | The fifth byte |
| `IsStandardDefined` | True when the first byte is `0x7E`, meaning a profile adopted by the MIDI Association and AMEI |
| `ProfileBank` | The bank of a standard defined profile. Zero for a manufacturer specific one, where that byte means something else |
| `ProfileNumber` | The number of a standard defined profile. Zero for a manufacturer specific one |
| `ProfileVersion` | The version of a standard defined profile. Zero for a manufacturer specific one |
| `ProfileLevel` | The level of a standard defined profile. Zero for a manufacturer specific one |

## Methods

| Method | Description |
| ------ | ----------- |
| `IsSameProfileAs(other)` | True when all five bytes agree. Returns false for a null argument |
| `ToString` | (From `IStringable`) The five bytes in the order they travel |

## Static Properties

| Static Property | Description |
| --------------- | ----------- |
| `StandardDefinedIdByte1` | The first byte of a profile adopted by the MIDI Association and AMEI. `0x7E` |

## Static Methods

| Static Method | Description |
| ------------- | ----------- |
| `CreateStandardDefined(profileBank, profileNumber, profileVersion, profileLevel)` | Constructs the identifier of a standard defined profile |
| `CreateManufacturerSpecific(sysExIdByte1, sysExIdByte2, sysExIdByte3, infoByte1, infoByte2)` | Constructs the identifier of a manufacturer's own profile |
