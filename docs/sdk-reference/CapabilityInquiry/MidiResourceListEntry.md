---
layout: sdk_reference_page
title: MidiResourceListEntry
namespace: Windows.Devices.Midi2.CapabilityInquiry
type: runtimeclass
implements: Windows.Foundation.IStringable
description: One resource a device offers, and what you can do with it
---

Read these before you ask for anything. They say whether a resource can be read, written, subscribed to, or asked for a page at a time.

`CanSet` is text instead of a true-or-false value because the list of possible values isn't fixed. The specification defines three values, and a device can add its own, so an enum couldn't hold everything a real device might send.

A device can leave a property out of its list. When it does, the value comes from the specification for that resource. Most resources share the same defaults, but a few don't. For example, a `ProgramList` can be asked for a page at a time and needs a resource ID, even when the device's list says only `{"resource": "ProgramList"}`. `FromJson` and the constructor that takes a name start from those defaults, and `GetJson` leaves out anything that matches them.

## Constructors

| Constructor | Description |
| ----------- | ----------- |
| `MidiResourceListEntry()` | Creates an empty entry |
| `MidiResourceListEntry(resource)` | Creates an entry for a named resource, starting from that resource's defaults |

## Properties

| Property | Description |
| -------- | ----------- |
| `Resource` | The resource name, for example `DeviceInfo` or `ProgramList` |
| `CanGet` | Whether the resource can be read. True unless the device says otherwise |
| `CanSet` | Whether the resource can be written, and how. Compare it with the static properties below instead of typing the text yourself |
| `CanSubscribe` | Whether you can subscribe to changes |
| `CanPaginate` | Whether the resource can be asked for a page at a time |
| `RequireResourceId` | True when a request for this resource must say which copy of it you want |
| `MediaTypes` | The media types the device lists for this resource |
| `Encodings` | The encodings the device accepts or sends, for example `Mcoded7` |
| `Schema` | The JSON Schema the device gave, exactly as it arrived. This API doesn't read it |

## Methods

| Method | Description |
| ------ | ----------- |
| `GetJson()` | The entry as it appears inside a ResourceList resource |
| `ToString` | (From `IStringable`) A readable summary |

## Static Properties

| Static Property | Description |
| --------------- | ----------- |
| `CanSetNone` | The value that means the resource can't be written |
| `CanSetFull` | The value that means the whole resource can be replaced |
| `CanSetPartial` | The value that means part of the resource can be replaced |

## Static Methods

| Static Method | Description |
| ------------- | ----------- |
| `FromJson(json)` | Reads one entry from its JSON object |
