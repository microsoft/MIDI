---
layout: sdk_reference_page
title: MidiEndpointConnectionSettings
namespace: Windows.Devices.Midi2
type: runtimeclass
description: Settings used when creating a connection to a MIDI endpoint
---

Pass a `MidiEndpointConnectionSettings` to `MidiSession.CreateEndpointConnection` to choose how the connection behaves. You only need one if you want something other than the defaults. Otherwise, call the version of `CreateEndpointConnection` that takes no settings.

## Constructors

| Constructor | Description |
| ----------- | ----------- |
| `MidiEndpointConnectionSettings()` | Creates settings with the default values: `WaitForEndpointReceiptOnSend` is false and `AutoReconnect` is true |
| `MidiEndpointConnectionSettings(waitForEndpointReceiptOnSend)` | Creates settings with the `WaitForEndpointReceiptOnSend` value you choose, and the default `AutoReconnect` |
| `MidiEndpointConnectionSettings(waitForEndpointReceiptOnSend, autoReconnect)` | Creates settings with both values you choose |

## Properties

| Property | Description |
| -------- | ----------- |
| `WaitForEndpointReceiptOnSend` | When true, each send call waits for the service to confirm it got the message before returning. Sending gets slower, but in some cases it's more reliable |
| `AutoReconnect` | When true, if the endpoint device disconnects and then comes back while the connection is still open, the connection reconnects to it automatically |
