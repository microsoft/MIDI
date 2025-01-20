---
layout: api_page
title: MidiReporting
parent: Midi2.Reporting
has_children: false
---

# MidiReporting

The MidiReporting class contains functions to report on data from the MIDI Service

## Location

| Namespace | Microsoft.Windows.Devices.Midi2.Reporting |
| Library | Microsoft.Windows.Devices.Midi2 |

## Static Methods

| Static Property | Description |
| --------------- | ----------- |
| `GetInstalledTransportPlugins()` | Returns a list of `MidiServiceTransportPluginInfo` representing all service transport plugins (also called Abstractions) |
| `GetInstalledMessageProcessingPlugins()` | Returns a list of `MidiServiceMessageProcessingPluginInfo` objects representing all service message processing plugins (also called Transforms). NOTE: In the first release of Windows MIDI Services, this returns an empty vector. |
| `GetActiveSessions()` | Returns a list of `MidiServiceSessionInfo` detailing all active Windows MIDI Services sessions on this PC. |

## IDL

[MidiReporting IDL](https://github.com/microsoft/MIDI/blob/main/src/app-sdk/winrt/MidiReporting.idl)
