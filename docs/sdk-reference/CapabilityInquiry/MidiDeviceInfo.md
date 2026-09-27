---
layout: sdk_reference_page
title: MidiDeviceInfo
namespace: Windows.Devices.Midi2.CapabilityInquiry
type: runtimeclass
implements: Windows.Foundation.IStringable
description: What a device says about itself, from its DeviceInfo resource
---

A device gives both identifiers and names, and you need both. The names are for people to read, and the identifiers are for your code to match on.

The identity uses the same type an endpoint uses to describe itself elsewhere, on purpose. A device says who it is in several different messages, and those must agree. Using one type for all of them helps keep them the same.

## Constructors

| Constructor | Description |
| ----------- | ----------- |
| `MidiDeviceInfo()` | Creates an empty DeviceInfo |
| `MidiDeviceInfo(identity)` | Creates one from the identity alone |
| `MidiDeviceInfo(identity, manufacturer, family, model)` | Creates one from the identity and the names that go with it |

## Properties

| Property | Description |
| -------- | ----------- |
| `Identity` | The same identity the endpoint gives elsewhere |
| `Manufacturer` | The manufacturer's name, if the device gives one |
| `Family` | The device family's name |
| `Model` | The model's name |
| `Version` | The version text, if the device gives one |

## Methods

| Method | Description |
| ------ | ----------- |
| `GetJson()` | The DeviceInfo resource as a JSON object |
| `ToString` | (From `IStringable`) A readable summary |

## Static Methods

| Static Method | Description |
| ------------- | ----------- |
| `FromJson(json)` | Reads DeviceInfo from its JSON object |
