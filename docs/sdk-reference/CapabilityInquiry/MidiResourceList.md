---
layout: sdk_reference_page
title: MidiResourceList
namespace: Windows.Devices.Midi2.CapabilityInquiry
type: runtimeclass
description: Everything a device offers through property exchange, from its ResourceList resource
---

Read this before you make a request, so you don't send a request the device will only refuse.

## Constructors

| Constructor | Description |
| ----------- | ----------- |
| `MidiResourceList()` | Creates an empty list |

## Properties

| Property | Description |
| -------- | ----------- |
| `Entries` | The resources the device listed |

## Methods

| Method | Description |
| ------ | ----------- |
| `GetEntry(resource)` | The entry for a named resource, or null if the device doesn't offer it |
| `SupportsResource(resource)` | True if the device lists that resource |
| `GetJson()` | The list as JSON. The ResourceList resource is a JSON array of entries |

## Static Methods

| Static Method | Description |
| ------------- | ----------- |
| `FromJson(json)` | Reads a list from the resource's JSON array |
