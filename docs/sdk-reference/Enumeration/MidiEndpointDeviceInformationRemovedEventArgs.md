---
layout: sdk_reference_page
title: MidiEndpointDeviceInformationRemovedEventArgs
namespace: Windows.Devices.Midi2.Enumeration
type: runtimeclass
description: Arguments supplied by the watcher when an endpoint is removed from the system
---

The watcher passes this to your `Removed` handler when an endpoint is removed.

## Properties

| Property | Description |
| --------------- | ----------- |
| `RemovedDevice` | The `MidiEndpointDeviceInformation` for the endpoint that was removed |

## Remarks

The removed device's properties are a copy taken before it was removed. The endpoint is already gone when this is raised, so use it to update your own list, not to ask the device anything.
