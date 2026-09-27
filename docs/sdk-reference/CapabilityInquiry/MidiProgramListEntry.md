---
layout: sdk_reference_page
title: MidiProgramListEntry
namespace: Windows.Devices.Midi2.CapabilityInquiry
type: runtimeclass
implements: Windows.Foundation.IStringable
description: One selectable program on a device, from its ProgramList resource
---

The bank and program values are sent to the device exactly as they appear here. All three start at 0. The specification's rules require that, even though one of its own examples doesn't follow it.

## Constructors

| Constructor | Description |
| ----------- | ----------- |
| `MidiProgramListEntry()` | Creates an empty entry |
| `MidiProgramListEntry(title, bankMsb, bankLsb, programChange)` | Creates an entry with a title and the three values that select it |

## Properties

| Property | Description |
| -------- | ----------- |
| `Title` | The program's display name, from the entry's `title` |
| `BankMsb` | The bank select most significant byte. Starts at 0 |
| `BankLsb` | The bank select least significant byte. Starts at 0 |
| `ProgramChange` | The program change number. Starts at 0 |
| `Tags` | From the entry's `tags`. Sound sets often give a program and its bank variation the same title, so the tags may be the only way to tell them apart |
| `Categories` | From the entry's `category`. Appendix A of the specification (M2-107-UM) suggests category names, but a device can use its own, so don't expect every name to come from that list |
| `CollectionTitle` | Which collection this program came from, when the device offers more than one. It isn't part of the resource. It's filled in from the link that led to the list |

## Methods

| Method | Description |
| ------ | ----------- |
| `GetJson()` | The entry as it appears inside a ProgramList resource |
| `ToString` | (From `IStringable`) A readable summary of the entry |

## Static Methods

| Static Method | Description |
| ------------- | ----------- |
| `FromJson(json)` | Reads one entry from its JSON object |
