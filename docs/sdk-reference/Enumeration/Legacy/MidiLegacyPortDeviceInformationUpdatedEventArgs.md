---
layout: sdk_reference_page
title: MidiLegacyPortDeviceInformationUpdatedEventArgs
namespace: Windows.Devices.Midi2.Enumeration.Legacy
type: runtimeclass
description: Event args for when a MIDI 1.0 port is updated
---

Raised by `MidiLegacyPortDeviceWatcher` when a MIDI 1.0 port's properties have been updated.

## Properties

| Property | Description |
| -------- | ----------- |
| `UpdatedDevice` | The updated `MidiLegacyPortDeviceInformation` |
| `IsNameUpdated` | True if the name was updated |  
| `IsNumberUpdated` | True if the WinMM port number was updated |
