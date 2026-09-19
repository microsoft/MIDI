---
layout: sdk_reference_page
title: MidiProgramListEntry
namespace: Windows.Devices.Midi2.CapabilityInquiry
type: runtimeclass
implements: Windows.Foundation.IStringable
description: One selectable program on a device, from its ProgramList resource
---

The bank and program values go on the wire exactly as they appear here. All three are zero based, which the specification's normative text requires even though its own worked example is not.

## Constructors

| Constructor | Description |
| ----------- | ----------- |
| `MidiProgramListEntry()` | Constructs an empty entry |
| `MidiProgramListEntry(title, bankMsb, bankLsb, programChange)` | Constructs an entry with a title and the three values which select it |

## Properties

| Property | Description |
| -------- | ----------- |
| `Title` | Display name for the program, from the entry's `title` |
| `BankMsb` | Bank select most significant byte. Zero based |
| `BankLsb` | Bank select least significant byte. Zero based |
| `ProgramChange` | Program change number. Zero based |
| `Tags` | From the entry's `tags`. A sound set commonly gives a program and its bank variation the same title, so the tags are often the only thing telling them apart |
| `Categories` | Appendix A category names, from the entry's `category` |
| `CollectionTitle` | Which collection this program came from, when the device offers more than one. Not part of the resource itself; filled in from the link that led to the list |

## Methods

| Method | Description |
| ------ | ----------- |
| `GetJson()` | Round trips the entry as it appears inside a ProgramList resource |
| `ToString` | (From `IStringable`) A readable summary of the entry |

## Static Methods

| Static Method | Description |
| ------------- | ----------- |
| `FromJson(json)` | Reads one entry from its JSON object |
