---
layout: sdk_reference_page
title: MidiEndpointDeviceInformationRemovedEventArgs
namespace: Windows.Devices.Midi2.Enumeration
type: runtimeclass
description: Arguments supplied by the watcher when an endpoint is removed from the system
---

Represents a notification that an endpoint has been removed from the system.

## Properties

| Property | Description |
| --------------- | ----------- |
| `RemovedDevice` | The `MidiEndpointDeviceInformation` for the endpoint which was removed |

## Remarks

The removed device's properties are a snapshot taken before removal. The endpoint is already gone by the time this is raised, so use this to update your own state rather than to query the device.
