---
layout: api_page
title: MidiEndpointUserSuppliedInfo
parent: Endpoint Enumeration
grand_parent: Midi2 core
has_children: false
---

# MidiEndpointUserSuppliedInfo

This is all information supplied by the user through the MIDI Settings app and/or through the main configuration file.

## Fields

| Field | Description |
| --------------- | ----------- |
| `Name` | The endpoint name as provided by the transport |
| `Description` | The description, if any, as provided by the transport |
| `LargeImagePath` | Path to the small image for use in applications |
| `SmallImagePath` | Path to the larger image for use in applications |
| `RequiresNoteOffTranslation` | True if a Note On of zero velocity should be translated to a Note Off message |
| `RecommendedControlChangeAutomationIntervalMilliseconds` | For applications, this is the recommended maximum CC interval to use to avoid flooding the device  |
| `SupportsMidiPolyphonicExpression` | True if this device is known to support MPE |

## IDL

[MidiEndpointUserSuppliedInfo IDL](https://github.com/microsoft/MIDI/blob/main/src/app-sdk/winrt-core/MidiEndpointUserSuppliedInfo.idl)

