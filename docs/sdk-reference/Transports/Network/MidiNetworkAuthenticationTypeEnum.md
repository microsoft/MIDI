---
layout: sdk_reference_page
title: MidiNetworkAuthenticationType
namespace: Windows.Devices.Midi2.Transports.Network
type: enum
description: Authentication required by a Network MIDI 2.0 host
---

Set on `MidiNetworkHostCreationConfig.AuthenticationType`.

## Values

| Value | Numeric Value | Description |
| ----- | ------------- | ----------- |
| `NoAuthentication` | `0` | No authentication. Any client that gets past the host's approval policy can connect |
| `PasswordAuthentication` | `1` | A shared secret is required. Not built yet |
| `UserAuthentication` | `2` | A user name and password are required. Not built yet |

## Remarks

Only `NoAuthentication` is accepted right now. A host set up for either of the others is rejected when it's configured, with `AuthenticationNotImplemented`. That's better than starting and quietly accepting connections that aren't authenticated. See [issue 733](https://github.com/microsoft/MIDI/issues/733).
