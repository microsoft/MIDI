---
layout: api_page
title: MidiServiceConfig
parent: Midi2.ServiceConfig
has_children: false
---

# MidiServiceConfig

The MidiServiceConfig class contains methods which are typically used only by the client-side configuration components of transports and message transforms. The JSON in these messages must be understood by the service and known to the service transport/transform to be processed properly.

## Location

| Namespace | Microsoft.Windows.Devices.Midi2.ServiceConfig |
| Library | Microsoft.Windows.Devices.Midi2 |

## Static Functions

| Property | Description |
| --------------- | ----------- |
| `UpdateTransportPluginConfiguration(IMidiServiceTransportPluginConfiguration)` | Sends an update to the service to be used by a transport plugin ("Abstraction") |
| `UpdateProcessingPluginConfiguration(IMidiServiceMessageProcessingPluginConfiguration)` | Sends an update to the service to be used by a message processing plugin ("Transform")  |

For plugins which support updates at runtime, developers of those plugins should create configuration WinRT types which implement the required configuration interfaces, and create the JSON that is used in the service. In this way, third-party service transport and message processing plugins can be created and configured without changes to the API.

> Note: In version 1 of the API, only transports can be configured at runtime. We're working on enabling configuration of message processing plugins. The API is a no-op.

## IDL

[MidiServiceConfig IDL](https://ggithub.com/microsoft/MIDI/blob/main/src/app-sdk/winrt/MidiServiceConfig.idl)
