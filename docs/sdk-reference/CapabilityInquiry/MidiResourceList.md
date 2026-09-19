---
layout: sdk_reference_page
title: MidiResourceList
namespace: Windows.Devices.Midi2.CapabilityInquiry
type: runtimeclass
description: Everything a device offers through property exchange, from its ResourceList resource
---

Asking this before making a request is what stops a client sending an inquiry the device will only reject.

## Constructors

| Constructor | Description |
| ----------- | ----------- |
| `MidiResourceList()` | Constructs an empty list |

## Properties

| Property | Description |
| -------- | ----------- |
| `Entries` | The resources the device declared |

## Methods

| Method | Description |
| ------ | ----------- |
| `GetEntry(resource)` | The entry for a named resource, or null when the device does not offer it |
| `SupportsResource(resource)` | True when the device declared that resource |
| `GetJson()` | The ResourceList resource is a JSON array of entries |

## Static Methods

| Static Method | Description |
| ------------- | ----------- |
| `FromJson(json)` | Reads a list from the JSON array the resource is |
