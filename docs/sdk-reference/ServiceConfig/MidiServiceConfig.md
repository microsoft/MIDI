---
layout: sdk_reference_page
title: MidiServiceConfig
namespace: Microsoft.Windows.Devices.Midi2.ServiceConfig
library: Microsoft.Windows.Devices.Midi2.dll
type: runtimeclass
description: Class used to update information in the service. Typically not used directly by apps.
---

The `MidiServiceConfig` class contains methods which are typically used only by the client-side configuration components of transports (and in the future, message transforms). The JSON in these messages must be understood by the service and known to the service transport/transform to be processed properly.

Unless you are authoring a new transport, it is not recommended that you use this class. Instead, known configuration options are provided by strongly-typed classes.

**NOTE: The json config sent to and received from the service is an implementation detail, not a contract, and is subject to change. Do not attempt to manually manipulate or create json to send to the MIDI Service, or manually parse the json return results unless you are creating a transport yourself.**

## Static Functions

| Property | Description |
| --------------- | ----------- |
| `UpdateTransportPluginConfiguration (config)` | Sends an update to the service to be used by a transport plugin |

For plugins which support updates at runtime, developers of those plugins should create configuration WinRT types which implement the required configuration interfaces, and create the JSON that is used in the service. In this way, third-party service transport and message processing plugins can be created and configured without changes to the API.

> Note: In version 1 of the API, only transports can be configured at runtime. We're working on enabling configuration of message processing plugins.
