---
layout: sdk_reference_page
title: MidiResourceLink
namespace: Windows.Devices.Midi2.CapabilityInquiry
type: runtimeclass
implements: Windows.Foundation.IStringable
description: A pointer from one resource to another, as carried in a links array
---

This is how a device says which ProgramList a given channel selects from. A device with several collections is only reachable by following these.

## Constructors

| Constructor | Description |
| ----------- | ----------- |
| `MidiResourceLink()` | Constructs an empty link |
| `MidiResourceLink(resource, resourceId)` | Constructs a link to one instance of a resource |

## Properties

| Property | Description |
| -------- | ----------- |
| `Resource` | The resource being pointed at, for example `ProgramList` |
| `ResourceId` | Which instance of that resource, when the device offers more than one. Empty means the device has only the one and the request carries no identifier |
| `Title` | Display name for the collection, when the device gives one |

## Methods

| Method | Description |
| ------ | ----------- |
| `GetJson()` | The link as it appears inside a `links` array |
| `ToString` | (From `IStringable`) A readable summary of the link |

## Static Methods

| Static Method | Description |
| ------------- | ----------- |
| `FromJson(json)` | Reads a link from its JSON object |
