---
layout: sdk_reference_page
title: MidiEndpointUserSuppliedInfo
namespace: Windows.Devices.Midi2.Enumeration
type: runtimeclass
description: Custom information supplied by the user for an endpoint
---

Everything here was set by the user, through MIDI Settings or another tool that saves endpoint customizations. Changing an object you got from `MidiEndpointDeviceInformation` doesn't change the saved settings. To change those, use [`MidiServiceEndpointCustomizationConfig`]({{ site.baseurl }}/sdk-reference/ServiceConfig/MidiServiceEndpointCustomizationConfig/).

## Constructors

| Constructor | Description |
| ----------- | ----------- |
| `MidiEndpointUserSuppliedInfo()` | Creates an empty object |
| `MidiEndpointUserSuppliedInfo(name, imageFileName, requiresNoteOffTranslation, recommendedControlChangeAutomationIntervalMilliseconds, supportsMidiPolyphonicExpression, customMidiOutgoingLatencyTicks, useCustomMidiOutgoingLatencyTicksForScheduling)` | Creates an object with these values. There's no description parameter, so set `Description` afterward if you need it |

## Properties

| Property | Description |
| --------------- | ----------- |
| `IsReadOnly` | True if you should treat this object as read-only |
| `Name` | The name the user gave this endpoint. It's used instead of the name from the transport |
| `Description` | The description the user gave this endpoint |
| `ImageFileName` | The path to an image file that applications can show for this endpoint |
| `RequiresNoteOffTranslation` | True if a Note On with a velocity of zero should be changed into a Note Off message |
| `RecommendedControlChangeAutomationIntervalMilliseconds` | The time, in milliseconds, that applications should leave between control change messages, so they don't flood the device |
| `SupportsMidiPolyphonicExpression` | True if this device supports MIDI Polyphonic Expression (MPE) |
| `CustomMidiOutgoingLatencyTicks` | A custom outgoing latency, in MIDI clock ticks, used when scheduling outgoing messages. It can be negative for an endpoint that runs early |
| `UseCustomMidiOutgoingLatencyTicksForScheduling` | True if `CustomMidiOutgoingLatencyTicks` should be used when scheduling messages |
| `CalculatedMidiOutgoingLatencyTicks` | The latency compensation the transport worked out for this endpoint. The transport supplies it, not the user. It's here so your application can show what's in effect without reading device properties itself |

## Outgoing latency compensation

The scheduler sends each message early by the endpoint's compensation value, so the message reaches the device at the time the application asked for, not a little after. There are two possible values, and `UseCustomMidiOutgoingLatencyTicksForScheduling` picks between them.

| Source | Where it comes from |
| ------ | ------------------- |
| Calculated | Worked out by the transport. Bluetooth uses half the connection interval it agreed on with the device. Network MIDI 2.0 uses half the measured ping round trip time. There's nothing to set up. |
| Custom | `CustomMidiOutgoingLatencyTicks`, supplied by the user through the MIDI Console or a settings app. |

`UseCustomMidiOutgoingLatencyTicksForScheduling` is set when a user supplies a custom value, and cleared when that value is removed. When it's true, the custom value is used. Otherwise, the calculated value is used. The two are never added together.

A customer can also make that choice directly, through `UseCustomOutgoingLatency` on `MidiServiceEndpointCustomizationConfig`. That lets them turn compensation off without throwing away a value that took a loopback cable and a measurement to get.

The scheduler reads the compensation when a connection to the endpoint opens. So a change takes effect the next time an application connects, not right away.

Transports that can't work out a useful value don't supply a calculated latency. That includes endpoints that come from MIDI 1.0 drivers for devices that aren't USB. Those endpoints get no compensation unless someone supplies a custom value.
