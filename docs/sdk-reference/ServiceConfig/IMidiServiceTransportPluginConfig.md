---
layout: sdk_reference_page
title: IMidiServiceTransportPluginConfig
namespace: Microsoft.Windows.Devices.Midi2.ServiceConfig
library: Microsoft.Windows.Devices.Midi2.dll
type: interface
description: Information for a transport plugin in the MIDI Service
---

Interface implemented by strongly-typed SDK classes used in configuring transport features. For example, this is used when setting up a virtual device, configuring Network MIDI 2.0 hosts and clients, and creating loopback endpoints.

> <h3>Important Note</h3>
> The json config sent to and received from the service is an implementation detail, not a contract, and is subject to change. Do not attempt to manually manipulate or create json to send to the MIDI Service, or manually parse the json return results, unless you are creating a transport yourself.

## Properties

| Property | Description |
| --- | --- |
| `TransportID` | The ID of the transport this configuration targets. This is used in the service for routing this config. |

## Functions

| Function | Description |
| --- | --- |
| `GetConfigJson` | Returns the json to send to the service |
