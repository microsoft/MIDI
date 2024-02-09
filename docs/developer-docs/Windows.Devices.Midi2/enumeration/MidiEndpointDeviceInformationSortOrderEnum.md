---
layout: api_page
title: MidiEndpointDeviceInformationSortOrder
parent: Endpoint Enumeration
grand_parent: Windows.Devices.Midi2 API
has_children: false
---

# MidiEndpointDeviceInformationSortOrder Enumeration

Specifies the sort order to use when enumerating a static list of devices.

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
| `TransportMnemonicThenName` | `21` | Sort by the transport mnemonic (example: "DIAG") and then by the device instance id |
| `TransportMnemonicThenEndpointDeviceId` | `22` | Sort by the transport mnemonic and then by the endpoint id |
| `TransportMnemonicThenDeviceInstanceId` | `23` | Sort by the transport mnemonic and then by the device instance id |

## IDL

[MidiEndpointDeviceInformationSortOrderEnum IDL](https://github.com/microsoft/MIDI/blob/main/src/api/Client/Midi2Client/MidiEndpointDeviceInformationSortOrderEnum.idl)

