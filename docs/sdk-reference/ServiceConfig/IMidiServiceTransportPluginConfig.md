---
layout: sdk_reference_page
title: IMidiServiceTransportPluginConfig
namespace: Windows.Devices.Midi2.ServiceConfig
type: interface
description: Information for a transport plugin in the MIDI Service
---

The interface that every transport configuration class has. Those classes set up transport features, such as creating a virtual device, setting up Network MIDI 2.0 hosts and clients, or creating loopback endpoints. Pass one to `MidiServiceTransportPluginConfigManager.SendUpdate` or `SaveUpdate`.

> **Important:** The JSON that goes to and from the service is an implementation detail, not a contract, and it can change. Don't build or edit this JSON by hand, and don't parse what comes back, unless you're writing a transport yourself.

## Properties

| Property | Description |
| --- | --- |
| `TransportId` | The GUID of the transport this configuration is for. The service uses it to pass the configuration to the right transport |
| `ConfigJson` | The `Windows.Data.Json.JsonObject` to send to the service |
