---
layout: sdk_reference_page
title: MidiVirtualDeviceClientEndpointInUseChangedEventArgs
namespace: Windows.Devices.Midi2.Transports.Virtual
type: runtimeclass
description: Event args for the MidiVirtualDevice ClientEndpointInUseChanged event
---

Supplied with the `MidiVirtualDevice.ClientEndpointInUseChanged` event, raised when an application connects to, or disconnects from, the device's client-visible endpoint.

## Properties

| Property | Description |
| -------- | ----------- |
| `IsClientEndpointInUse` | True when one or more applications are now connected to the client-visible endpoint, false when none are. |

## Remarks

This is a connected or not-connected signal, not a count of applications. See the remarks on `MidiVirtualDevice` for why.
