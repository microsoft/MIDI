---
layout: sdk_reference_page
title: MidiEndpointDevicePropertyHelper
namespace: Windows.Devices.Midi2.Enumeration
type: runtimeclass
description: Assists with understanding the GUID-based Windows MIDI Services device properties.
status: preview
---

Static methods that give readable names to the Windows MIDI Services device properties in a property bag. The MIDI Console uses this class the most.

You can't use these names in device queries, and they can change at any time. They're just easier to read than the raw GUID and index values.

## Static Methods

| Static Method | Description |
| --------------- | ----------- |
| `GetMidiPropertyNameFromPropertyKey(fmtid, pid)` | Returns the readable name for a MIDI property, given its GUID and property id |
| `GetMidiPropertyNameFromPropertyKey(key)` | Returns the readable name for a MIDI property, given its property key as text |
| `IsMidiPropertyKey(fmtid, pid)` | Returns true if the property is a Windows MIDI Services property |
| `IsMidiPropertyKey(key)` | Returns true if the property is a Windows MIDI Services property |
| `GetAllMidiProperties()` | Returns a map of every Windows MIDI Services property key |
