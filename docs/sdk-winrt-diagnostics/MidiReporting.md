---
layout: api_page
title: MidiReporting
parent: Midi2.Diagnostics
has_children: false
---

# MidiReporting

## Remarks

The MidiReporting class contains functions to report on data from the MIDI Service

## Static Methods

| Static Property | Description |
| --------------- | ----------- |
| `GetInstalledTransportPlugins()` | Returns a list of `MidiServiceTransportPluginInfo` representing all service transport plugins (also called Abstractions) |
| `GetInstalledMessageProcessingPlugins()` | Returns a list of `MidiServiceMessageProcessingPluginInfo` objects representing all service message processing plugins (also called Transforms) |
| `GetActiveSessions()` | Returns a list of `MidiServiceSessionInfo` detailing all active Windows MIDI Services sessions on this PC. |

## IDL

[MidiReporting IDL](https://github.com/microsoft/MIDI/blob/main/src/app-sdk/winrt-diagnostics/MidiReporting.idl)