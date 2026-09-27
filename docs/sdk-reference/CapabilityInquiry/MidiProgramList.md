---
layout: sdk_reference_page
title: MidiProgramList
namespace: Windows.Devices.Midi2.CapabilityInquiry
type: runtimeclass
description: The programs a device offers, from its ProgramList resource
---

A workstation keyboard can have thousands of programs, so a device may send its list one page at a time. The offset and the total don't come from the list itself. The offset is what was asked for, and the total is what the device reported in the header of its reply.

A device that sends its whole list at once doesn't report a total. In that case, `TotalCount` is the number of entries and `Offset` is zero.

Paging only moves forward through the list, never back.

`MidiCapabilityInquirySession.GetProgramListAsync` asks for pages until the device says there are no more. So it returns the whole list, and `HasMoreEntries` is false.

## Constructors

| Constructor | Description |
| ----------- | ----------- |
| `MidiProgramList()` | Creates an empty list |

## Properties

| Property | Description |
| -------- | ----------- |
| `Entries` | The programs in this list, or in this page of it |
| `Offset` | Where this page starts |
| `TotalCount` | How many entries the device says it has in all |
| `HasMoreEntries` | True when the device has more entries after this page. It tells you to ask again, so you don't have to guess from the count |
| `NextOffset` | The offset to ask for next. Zero when there's nothing more |

## Methods

| Method | Description |
| ------ | ----------- |
| `GetJson()` | The list as JSON. The ProgramList resource is a JSON array of entries, not an object |

## Static Methods

| Static Method | Description |
| ------------- | ----------- |
| `FromJson(json)` | Reads a list from the resource's JSON array |
