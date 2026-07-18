---
layout: sdk_reference_page
title: Midi1PortNameTableEntry
namespace: Windows.Devices.Midi2.Enumeration
type: runtimeclass
description: Represents the names generated when constructing a MIDI 1.0 port from a UMP device
---

This entry is generated when the parent UMP device is enumerated, and is then stored in a property on the parent UMP endpoint. The table is later used to set the FriendlyName on the Software Device (SWD) which represents the MIDI 1.0 port. 

A list of these is returned by `MidiEndpointDeviceInformation.GetNameTable()`.

## Properties

| Property | Description |
| -------- | ----------- |
| `Group` | The `MidiGroup` associated with this MIDI 1.0 port entry |
| `Flow` | A `Midi1PortFlow` value indicating the direction for this port |
| `CustomName` | A user-supplied custom name, if any |
| `LegacyCompatibleName` | A name compatible with legacy MIDI port naming conventions |
| `NewStyleName` | A name using the new Windows MIDI Services naming style |
