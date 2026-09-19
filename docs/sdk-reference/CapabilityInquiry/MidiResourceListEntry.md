---
layout: sdk_reference_page
title: MidiResourceListEntry
namespace: Windows.Devices.Midi2.CapabilityInquiry
type: runtimeclass
implements: Windows.Foundation.IStringable
description: One resource a device offers, together with what may be done with it
---

A client reads these before asking for anything: they say whether a resource can be read, written, subscribed to, or paged through.

`CanSet` is a string rather than a flag because the set is open. The specification defines three values and a device may declare its own, so an enumeration here would be unable to carry what a real device sends.

## Constructors

| Constructor | Description |
| ----------- | ----------- |
| `MidiResourceListEntry()` | Constructs an empty entry |
| `MidiResourceListEntry(resource)` | Constructs an entry for a named resource |

## Properties

| Property | Description |
| -------- | ----------- |
| `Resource` | The resource name, for example `DeviceInfo` or `ProgramList` |
| `CanGet` | Whether the resource can be read. True unless the device says otherwise |
| `CanSet` | Whether and how the resource can be written. Compare against the static properties below rather than against a literal |
| `CanSubscribe` | Whether a client may subscribe to changes |
| `CanPaginate` | Whether the resource can be asked for a page at a time |
| `RequireResourceId` | True when a request for this resource must name which instance of it is wanted |
| `MediaTypes` | Media types the device declares for this resource |
| `Encodings` | Encodings the device will accept or produce, for example `Mcoded7` |
| `Schema` | The declared JSON Schema, carried through untouched. It is arbitrary schema rather than anything this API interprets, so it is offered as it arrived |

## Methods

| Method | Description |
| ------ | ----------- |
| `GetJson()` | The entry as it appears inside a ResourceList resource |
| `ToString` | (From `IStringable`) A readable summary |

## Static Properties

| Static Property | Description |
| --------------- | ----------- |
| `CanSetNone` | The value meaning the resource cannot be written |
| `CanSetFull` | The value meaning the whole resource may be replaced |
| `CanSetPartial` | The value meaning part of the resource may be replaced |

## Static Methods

| Static Method | Description |
| ------------- | ----------- |
| `FromJson(json)` | Reads one entry from its JSON object |
