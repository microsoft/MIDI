---
layout: sdk_reference_page
title: MidiServiceTransportPluginConfigManager
namespace: Windows.Devices.Midi2.ServiceConfig
type: runtimeclass
description: Class used to update information in the service. Typically not used directly by apps.
---

The `MidiServiceTransportPluginConfigManager` class contains methods which are typically used only by the client-side configuration components of transports. The JSON in these messages must be understood by the service and known to the service transport to be processed properly.

Unless you are authoring a new transport, it is not recommended that you use this class. Instead, known configuration options are provided by strongly-typed classes.

**NOTE: The json config sent to and received from the service is an implementation detail, not a contract, and is subject to change. Do not attempt to manually manipulate or create json to send to the MIDI Service, or manually parse the json return results unless you are creating a transport yourself.**

## Static Methods

| Static Method | Description |
| --------------- | ----------- |
| `SendUpdate(configUpdate)` | Sends an `IMidiServiceTransportPluginConfig` update to the service to be used by a transport plugin. Returns a `MidiServiceConfigResponse`. |
| `SendUpdate(transportId, fullConfigObject)` | Sends a raw JSON configuration object to the service for the specified transport. Returns a `MidiServiceConfigResponse`. |
| `SendCommand(command)` | Sends a `MidiServiceTransportCommand` to the service. Returns a `MidiServiceConfigResponse`. |
