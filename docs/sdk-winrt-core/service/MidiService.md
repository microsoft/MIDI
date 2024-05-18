---
layout: api_page
title: MidiService
parent: Service
grand_parent: Microsoft.Devices.Midi2
has_children: false
---

# MidiService

## Remarks

The MidiService class contains a number of static functions which enable working with the service outside of a specific session. 

## Static Methods


### Runtime Configuration

| `UpdateTransportPluginConfiguration(IMidiServiceTransportPluginConfiguration)` | Sends an update to the service to be used by a transport plugin ("Abstraction") |
| `UpdateProcessingPluginConfiguration(IMidiServiceMessageProcessingPluginConfiguration)` | Sends an update to the service to be used by a message processing plugin ("Transform")  |

For plugins which support updates at runtime, developers of those plugins should create configuration WinRT types which implement the required configuration interfaces, and create the JSON that is used in the service. In this way, third-party service transport and message processing plugins can be created and configured without changes to the API.

> Note: In version 1 of the API, only transports can be configured at runtime. We're working on enabling configuration of message processing plugins. The API is a no-op.

## See also

[MidiService IDL](https://github.com/microsoft/MIDI/blob/main/src/app-sdk/winrt-core/MidiService.idl)