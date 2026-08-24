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
| `AllowAny` | `0` | Unknown remote clients are admitted unless explicitly denied |
| `RequireApproval` | `1` | Unknown remote clients remain pending until approved or denied |
