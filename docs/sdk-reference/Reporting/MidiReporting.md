---
layout: sdk_reference_page
title: MidiReporting
namespace: Windows.Devices.Midi2.Reporting
type: runtimeclass
description: Provides information about the service configuration
---

The MidiReporting class contains functions to report on data from the MIDI Service

## Static Methods

| Static Method | Description |
| --------------- | ----------- |
| `GetInstalledTransportPlugins()` | Returns a list of `MidiServiceTransportPluginInfo` representing all service transport plugins (also called Abstractions) |
| `GetActiveSessions()` | Returns a list of `MidiServiceSessionInfo` detailing all active Windows MIDI Services sessions on this PC. |
