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
