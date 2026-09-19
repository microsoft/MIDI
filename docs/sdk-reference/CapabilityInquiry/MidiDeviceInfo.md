---
layout: sdk_reference_page
title: MidiDeviceInfo
namespace: Windows.Devices.Midi2.CapabilityInquiry
type: runtimeclass
implements: Windows.Foundation.IStringable
description: What a device says about itself, from its DeviceInfo resource
---

The identifiers and the names are not alternatives. A device declares both: the names are what a person reads and the identifiers are what code matches on.

The identity is deliberately the same type an endpoint declares elsewhere. A device states who it is through several different carriers and they are required to agree, so sharing the type is what keeps them from drifting apart.

## Constructors

| Constructor | Description |
| ----------- | ----------- |
| `MidiDeviceInfo()` | Constructs an empty DeviceInfo |
| `MidiDeviceInfo(identity)` | Constructs one from the identity alone |
| `MidiDeviceInfo(identity, manufacturer, family, model)` | Constructs one from the identity and the names that go with it |

## Properties

| Property | Description |
| -------- | ----------- |
| `Identity` | The same identity the endpoint declares elsewhere |
| `Manufacturer` | Name of the manufacturer, when the device gives one |
| `Family` | Name of the device family |
| `Model` | Name of the model |
| `Version` | Version string, when the device gives one |

## Methods

| Method | Description |
| ------ | ----------- |
| `GetJson()` | The DeviceInfo resource, which is a JSON object |
| `ToString` | (From `IStringable`) A readable summary |

## Static Methods

| Static Method | Description |
| ------------- | ----------- |
| `FromJson(json)` | Reads DeviceInfo from its JSON object |
