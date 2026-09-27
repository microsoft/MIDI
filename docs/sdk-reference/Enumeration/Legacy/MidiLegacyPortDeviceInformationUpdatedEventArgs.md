---
layout: sdk_reference_page
title: MidiLegacyPortDeviceInformationUpdatedEventArgs
namespace: Windows.Devices.Midi2.Enumeration.Legacy
type: runtimeclass
description: Event args for when a MIDI 1.0 port is updated
---

`MidiLegacyPortDeviceWatcher` passes this to your `Updated` handler when a MIDI 1.0 port's properties change.

## Properties

| Property | Description |
| -------- | ----------- |
| `UpdatedDevice` | The `MidiLegacyPortDeviceInformation` for the port, with its current values |
| `DeviceInformationUpdate` | The `Windows.Devices.Enumeration.DeviceInformationUpdate` this came from, for an application that needs the raw changed properties |
| `IsNameUpdated` | True if the name changed |  
| `IsNumberUpdated` | True if the WinMM port number changed |
