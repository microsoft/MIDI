---
layout: api_page
title: MidiLoopbackEndpointRemovalConfig
parent: Midi2.Endpoints.Loopback
has_children: false
---

# MidiLoopbackEndpointRemovalConfig

The configuration passed to the service when an application wants to remove a loopback endpoint pair.

## Location

| Namespace | Microsoft.Windows.Devices.Midi2.Endpoints.Loopback |
| Library | Microsoft.Windows.Devices.Midi2 |

## Properties

| Property | Description |
| -------- | ----------- |
| `AssociationId` | The GUID used to uniquely identify the endpoint pair to remove |

## Methods

| Name | Description |
| -------- | ----------- |
| `MidiLoopbackEndpointRemovalConfig(associationId)` | Construct a removal config with the specified association id |

## IDL

[MidiLoopbackEndpointRemovalConfig IDL](https://github.com/microsoft/MIDI/blob/main/src/app-sdk/winrt/MidiLoopbackEndpointRemovalConfig.idl)