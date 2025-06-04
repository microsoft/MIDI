---
layout: sdk_reference_page
title: MidiEndpointUserSuppliedInfo
namespace: Microsoft.Windows.Devices.Midi2
library: Microsoft.Windows.Devices.Midi2.dll
type: runtimeclass
description: Custom information supplied by the user for an endpoint
---

This is all information supplied by the user through the MIDI Settings app and/or through the main configuration file.

## Struct Fields

| Field | Description |
| --------------- | ----------- |
| `Name` | The endpoint name as provided by the transport |
| `Description` | The description, if any, as provided by the transport |
| `LargeImagePath` | Path to the small image for use in applications |
| `SmallImagePath` | Path to the larger image for use in applications |
| `RequiresNoteOffTranslation` | True if a Note On of zero velocity should be translated to a Note Off message |
| `RecommendedControlChangeAutomationIntervalMilliseconds` | For applications, this is the recommended maximum CC interval to use to avoid flooding the device  |
| `SupportsMidiPolyphonicExpression` | True if this device is known to support MPE |
