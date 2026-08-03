---
layout: sdk_reference_page
title: MidiEndpointDeviceInformationSortOrder
namespace: Windows.Devices.Midi2.Enumeration
type: enum
description: Specifies the sort order to use when enumerating a static list of devices.
---

## Properties

| Property | Value | Description |
| --------------- | ---------- | ----------- |
| `None` | `0x00000000` | No sort. Return in default order |
| `Name` | `0x00000001` | Sort by the name of the endpoint |
| `EndpointDeviceId` | `0x00000002` | Sort by the id of the endpoint (the SWD id) |
| `DeviceInstanceId` | `0x00000003` | Sort by the device instance id |
| `ContainerThenName` | `0x0000000B` | Sort by the container and then by name. This is helpful when you want endpoints grouped by parent. |
| `ContainerThenEndpointDeviceId` | `0x0000000C` | Sort by the container and then by the endpoint id |
| `ContainerThenDeviceInstanceId` | `0x0000000D` | Sort by the container and then by the device instance id |
| `TransportCodeThenName` | `0x00000015` | Sort by the transport abbreviation (example: "DIAG", formerly called the mnemonic) and then by the device instance id |
| `TransportCodeThenEndpointDeviceId` | `0x00000016` | Sort by the transport abbreviation (formerly called the mnemonic) and then by the endpoint id |
| `TransportCodeThenDeviceInstanceId` | `0x00000017` | Sort by the transport abbreviation (formerly called the mnemonic) and then by the device instance id |
