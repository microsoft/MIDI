---
layout: sdk_reference_page
title: MidiNetworkRemoteClientPolicy
namespace: Windows.Devices.Midi2.Transports.Network
type: enum
description: How a host handles unknown remote client connection requests
---

Used by `MidiNetworkHostCreationConfig.RemoteClientPolicy` and reported by `MidiNetworkConfiguredHost.RemoteClientPolicy`.

## Values

| Value | Numeric Value | Description |
| ----- | ------------- | ----------- |
| `AllowAny` | `0` | Unknown remote clients are let in, unless they've been denied |
| `RequireApproval` | `1` | Unknown remote clients wait until they're approved or denied |
