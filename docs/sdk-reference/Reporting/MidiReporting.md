---
layout: sdk_reference_page
title: MidiReporting
namespace: Microsoft.Windows.Devices.Midi2.Reporting
library: Microsoft.Windows.Devices.Midi2.dll
type: runtimeclass
description: Provides information about the service configuration
---

The MidiReporting class contains functions to report on data from the MIDI Service

## Static Methods

| Static Property | Description |
| --------------- | ----------- |
| `GetInstalledTransportPlugins()` | Returns a list of `MidiServiceTransportPluginInfo` representing all service transport plugins (also called Abstractions) |
| `GetInstalledMessageProcessingPlugins()` | Returns a list of `MidiServiceMessageProcessingPluginInfo` objects representing all service message processing plugins (also called Transforms). NOTE: In the first release of Windows MIDI Services, this returns an empty vector. |
| `GetActiveSessions()` | Returns a list of `MidiServiceSessionInfo` detailing all active Windows MIDI Services sessions on this PC. |
