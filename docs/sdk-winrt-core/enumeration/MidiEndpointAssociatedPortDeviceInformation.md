---
layout: api_page
title: MidiEndpointAssociatedPortDeviceInformation
parent: Endpoint Enumeration
grand_parent: Midi2 core
has_children: false
---

# MidiEndpointAssociatedPortDeviceInformation

This type contains information about a MIDI 1.0 API port that is associated with a Windows MIDI Services UMP endpoint. You will find this used by functions on the `MidiEndpointDeviceInformation` type.

## Location

| Namespace | Microsoft.Windows.Devices.Midi2 |
| Library | Microsoft.Windows.Devices.Midi2 |

## Properties

| Field | Description |
| --------------- | ----------- |
| `ContainerId` | The system container id for the MIDI port |
| `ParentDeviceInstanceId` | Windows Device Instance Id for the port |
| `ParentEndpointDeviceId` | The Endpoint Device Id for the UMP endpoint which this port is associated with |
| `Group` | The single group that this port represents on the Parent UMP Endpoint |
| `PortFlow` | The flow (input/source or output/destination) as defined by the `Midi1PortFlow` enum, for this port |
| `PortName` | The name this port has been created with |
| `PortDeviceId` | The full device id for this MIDI 1.0 port. This can be used directly by the WinRT MIDI 1.0 API |
| `PortNumber` | The port number as used in the WinMM MIDI 1.0 API |
| `DeviceInformation` | The full system `DeviceInformation` instance for this MIDI 1.0 port |

## IDL

[MidiEndpointAssociatedPortDeviceInformation IDL](https://github.com/microsoft/MIDI/blob/main/src/app-sdk/winrt/MidiEndpointAssociatedPortDeviceInformation.idl)

