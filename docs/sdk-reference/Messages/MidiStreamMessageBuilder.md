---
layout: sdk_reference_page
title: MidiStreamMessageBuilder
namespace: Microsoft.Windows.Devices.Midi2.Messages
library: Microsoft.Windows.Devices.Midi2.dll
type: runtimeclass
description: Helper class to construct and parse MIDI 2.0 stream messages
---

Most of the message builders in this class will not be needed by typical applications outside of those used for virtual devices.

Virtual device apps will be interested in the `BuildStreamConfigurationNotificationMessage` to respond to a stream configuration request event.

## Static Functions

The messages are described in the MIDI Association's MIDI 2.0 UMP specification.

| Function | Description |
| --------------- | ----------- |
| `BuildEndpointDiscoveryMessage` | Build a MIDI 2.0 endpoint discovery message |
| `BuildEndpointInfoNotificationMessage` | Build a MIDI 2.0 endpoint info notification message |
| `BuildDeviceIdentityNotificationMessage` | Build a MIDI 2.0 device identity notification |
| `BuildEndpointNameNotificationMessages` | Build the set of name notification messages corresponding to the supplied string |
| `BuildProductInstanceIdNotificationMessages` | Build the set of id notification messages corresponding to the supplied string |
| `BuildStreamConfigurationRequestMessage` | Build the MIDI 2.0 protocol negotiation stream configuration request message |
| `BuildStreamConfigurationNotificationMessage` | Build the MIDI 2.0 protocol negotiation stream configuration notification message |
| `BuildFunctionBlockDiscoveryMessage` | Build the MIDI 2.0 function block discovery message |
| `BuildFunctionBlockInfoNotificationMessage` | Build the MIDI 2.0 function block info notification message |
| `BuildFunctionBlockNameNotificationMessages` | Build the set of name notification messages corresponding to the supplied string  |

These functions are for parsing raw multi-message string responses

| Function | Description |
| --------------- | ----------- |
| `ParseEndpointNameNotificationMessages` | Processes all messages and returns the string |
| `ParseFunctionBlockNameNotificationMessages` | Processes all messages and returns the string |
| `ParseProductInstanceIdNotificationMessages` | Processes all messages and returns the string |
