---
layout: sdk_reference_page
title: MidiEndpointDeviceInformationSortOrder
namespace: Microsoft.Windows.Devices.Midi2
library: Microsoft.Windows.Devices.Midi2.dll
type: enum
description: Specifies the sort order to use when enumerating a static list of devices.
---

## Properties

| Property | Value | Description |
| --------------- | ---------- | ----------- |
| `None` | `0` | No sort. Return in default order |
| `Name` | `1` | Sort by the name of the endpoint |
| `EndpointDeviceId` | `2` | Sort by the id of the endpoint (the SWD id) |
| `DeviceInstanceId` | `3` | Sort by the device instance id |
| `ContainerThenName` | `11` | Sort by the container and then by name. This is helpful when you want endpoints grouped by parent. |
| `ContainerThenEndpointDeviceId` | `12` | Sort by the container and then by the endpoint id |
| `ContainerThenDeviceInstanceId` | `13` | Sort by the container and then by the device instance id |
| `TransportCodeThenName` | `21` | Sort by the transport abbreviation (example: "DIAG", formerly called the mnemonic) and then by the device instance id |
| `TransportCodeThenEndpointDeviceId` | `22` | Sort by the transport abbreviation (formerly called the mnemonic) and then by the endpoint id |
| `TransportCodeThenDeviceInstanceId` | `23` | Sort by the transport abbreviation (formerly called the mnemonic) and then by the device instance id |
