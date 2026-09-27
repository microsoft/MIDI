---
layout: sdk_reference_page
title: Midi1PortNameTableEntry
namespace: Windows.Devices.Midi2.Enumeration
type: runtimeclass
description: Represents the names generated when constructing a MIDI 1.0 port from a UMP device
---

These entries are made when the parent UMP device is found, and saved in a property on the parent UMP endpoint. Later, the table is used to set the friendly name of the software device (SWD) for each MIDI 1.0 port.

`MidiEndpointDeviceInformation.GetNameTable()` returns a list of these.

## Properties

| Property | Description |
| -------- | ----------- |
| `Group` | The `MidiGroup` this MIDI 1.0 port uses |
| `Flow` | A `Midi1PortFlow` value that says which way this port sends messages |
| `CustomName` | A name the user gave the port, if there is one |
| `LegacyCompatibleName` | A name made the same way older versions of Windows named MIDI ports |
| `NewStyleName` | A name made the new Windows MIDI Services way |
