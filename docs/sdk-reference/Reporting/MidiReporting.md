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
| `FindAllSessionsWithMatchingOpenUmpEndpoint(endpointDeviceId, includeRelatedMidi1Ports)` | Returns the `MidiServiceSessionInfo` for every session which currently has the specified UMP endpoint open. When `includeRelatedMidi1Ports` is true, sessions holding only the MIDI 1.0 ports associated with that endpoint are included as well. |
| `FindAllSessionsWithMatchingOpenUmpEndpointOrMidi1Ports(endpointsAndPorts)` | Returns the `MidiServiceSessionInfo` for every session which has any of the supplied endpoint or MIDI 1.0 port ids open. |

## Remarks

The two `FindAllSessions...` methods answer "who is using this device?", which is what you need before warning a user that changing or removing an endpoint will affect a running application.
