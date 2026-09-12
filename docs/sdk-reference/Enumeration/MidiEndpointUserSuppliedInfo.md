---
layout: sdk_reference_page
title: MidiEndpointUserSuppliedInfo
namespace: Windows.Devices.Midi2.Enumeration
type: runtimeclass
description: Custom information supplied by the user for an endpoint
---

This is all information supplied by the user through the MIDI Settings app and/or through the main configuration file.

## Properties

| Property | Description |
| --------------- | ----------- |
| `IsReadOnly` | True if this object should be treated as read-only |
| `Name` | User-supplied name for this endpoint, which overrides the transport-supplied name |
| `Description` | User-supplied description for this endpoint |
| `ImageFileName` | Path to an image file for use in applications |
| `RequiresNoteOffTranslation` | True if a Note On of zero velocity should be translated to a Note Off message |
| `RecommendedControlChangeAutomationIntervalMilliseconds` | For applications, this is the recommended maximum CC interval to use to avoid flooding the device |
| `SupportsMidiPolyphonicExpression` | True if this device is known to support MPE |
| `CustomMidiOutgoingLatencyTicks` | Custom outgoing latency in MIDI clock ticks, used when scheduling outgoing messages |
| `UseCustomMidiOutgoingLatencyTicksForScheduling` | True if `CustomMidiOutgoingLatencyTicks` should be used for message scheduling |

## Outgoing latency compensation

The outbound message scheduler releases a message early by the endpoint's compensation value, so
that it reaches the device at the timestamp the application asked for rather than some time after
it. Two values feed that, and this property decides between them.

| Source | Where it comes from |
| ------ | ------------------- |
| Calculated | Worked out by the transport. Bluetooth uses half the negotiated connection interval; Network MIDI 2.0 uses half the measured ping round trip. Nothing to configure. |
| Custom | `CustomMidiOutgoingLatencyTicks`, supplied by the user through the MIDI Console or a settings app. |

`UseCustomMidiOutgoingLatencyTicksForScheduling` is set automatically when a user supplies a custom
value, and cleared when that value is removed. When it is true the custom value is used; otherwise
the calculated value is used. The two are never combined.

The scheduler reads the compensation when a connection to the endpoint is opened, so a change takes
effect the next time an application connects rather than immediately.

Transports which cannot work out a meaningful value, including endpoints served by MIDI 1.0 drivers
that are not USB devices, supply no calculated latency. Those endpoints are uncompensated unless a
custom value is supplied.
