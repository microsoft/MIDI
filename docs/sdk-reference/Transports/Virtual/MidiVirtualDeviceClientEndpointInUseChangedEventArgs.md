---
layout: sdk_reference_page
title: MidiVirtualDeviceClientEndpointInUseChangedEventArgs
namespace: Windows.Devices.Midi2.Transports.Virtual
type: runtimeclass
description: Event args for the MidiVirtualDevice ClientEndpointInUseChanged event
---

Comes with the `MidiVirtualDevice.ClientEndpointInUseChanged` event. That event is raised when an app connects to, or disconnects from, the endpoint other apps see for the device.

## Properties

| Property | Description |
| -------- | ----------- |
| `IsClientEndpointInUse` | True when one or more apps are now connected to that endpoint, and false when none are |

## Remarks

It tells you whether any app is connected, not how many. The remarks on `MidiVirtualDevice` explain why.
