---
layout: sdk_reference_page
title: MidiResourceLink
namespace: Windows.Devices.Midi2.CapabilityInquiry
type: runtimeclass
implements: Windows.Foundation.IStringable
description: A pointer from one resource to another, as carried in a links array
---

This is how a device says which ProgramList a channel uses. For a device with several collections, following these links is the only way to reach them all.

## Constructors

| Constructor | Description |
| ----------- | ----------- |
| `MidiResourceLink()` | Creates an empty link |
| `MidiResourceLink(resource, resourceId)` | Creates a link to one copy of a resource |

## Properties

| Property | Description |
| -------- | ----------- |
| `Resource` | The resource being pointed at, for example `ProgramList` |
| `ResourceId` | Which copy of that resource, when the device offers more than one. Empty means the device has only one, and requests for it don't include an id |
| `Title` | The collection's display name, if the device gives one |

## Methods

| Method | Description |
| ------ | ----------- |
| `GetJson()` | The link as it appears inside a `links` array |
| `ToString` | (From `IStringable`) A readable summary of the link |

## Static Methods

| Static Method | Description |
| ------------- | ----------- |
| `FromJson(json)` | Reads a link from its JSON object |
