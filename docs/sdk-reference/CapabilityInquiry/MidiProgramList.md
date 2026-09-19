---
layout: sdk_reference_page
title: MidiProgramList
namespace: Windows.Devices.Midi2.CapabilityInquiry
type: runtimeclass
description: The programs a device offers, from its ProgramList resource
---

A workstation's program list runs to thousands of entries, so the resource may be paged. The offset and the total both come from the exchange rather than from the resource itself: the offset is what was asked for, and the total is what the device reported in its reply header.

A device which sends its whole list at once reports no total. In that case `TotalCount` is the entry count and `Offset` is zero.

Neither value can run backwards. Paging only ever moves forward through the list.

`MidiCapabilityInquirySession.GetProgramListAsync` asks for pages until the device says there are no more, so what it returns is the whole list and `HasMoreEntries` is false.

## Constructors

| Constructor | Description |
| ----------- | ----------- |
| `MidiProgramList()` | Constructs an empty list |

## Properties

| Property | Description |
| -------- | ----------- |
| `Entries` | The programs in this list, or in this page of it |
| `Offset` | Where this page began |
| `TotalCount` | How many entries the device says it has in total |
| `HasMoreEntries` | True when the device holds entries beyond this page, so a caller paging through a workstation sized list knows to ask again rather than guessing from the count |
| `NextOffset` | The offset to ask for next. Zero when there is nothing more |

## Methods

| Method | Description |
| ------ | ----------- |
| `GetJson()` | The ProgramList resource is a JSON array of entries, not an object |

## Static Methods

| Static Method | Description |
| ------------- | ----------- |
| `FromJson(json)` | Reads a list from the JSON array the resource is |
