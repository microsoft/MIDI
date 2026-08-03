---
layout: sdk_reference_page
title: MidiEndpointConnectionSettings
namespace: Windows.Devices.Midi2
type: runtimeclass
description: Settings used when creating a connection to a MIDI endpoint
---

`MidiEndpointConnectionSettings` is passed to `MidiSession.CreateEndpointConnection` to configure how the connection behaves. Create one using the constructors if you want non-default settings; otherwise pass no settings object to use defaults.

## Constructors

| Constructor | Description |
| ----------- | ----------- |
| `MidiEndpointConnectionSettings()` | Create settings with default values (`WaitForEndpointReceiptOnSend = false`, `AutoReconnect = true`) |
| `MidiEndpointConnectionSettings(waitForEndpointReceiptOnSend)` | Create settings with the specified `WaitForEndpointReceiptOnSend` value and default `AutoReconnect` |
| `MidiEndpointConnectionSettings(waitForEndpointReceiptOnSend, autoReconnect)` | Create settings with the specified values |

## Properties

| Property | Description |
| -------- | ----------- |
| `WaitForEndpointReceiptOnSend` | When true, send calls wait for the service to acknowledge receipt before returning. This can reduce throughput but improves reliability in some scenarios. |
| `AutoReconnect` | When true, the connection will automatically attempt to reconnect if the endpoint device is disconnected and then reconnected while the connection is alive. |
