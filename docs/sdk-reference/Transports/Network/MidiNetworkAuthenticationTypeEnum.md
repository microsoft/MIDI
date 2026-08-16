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
| `NoAuthentication` | `0` | No authentication. Any client which passes the host's approval policy may connect |
| `PasswordAuthentication` | `1` | A shared secret is required. Not yet implemented |
| `UserAuthentication` | `2` | A user name and password are required. Not yet implemented |

## Remarks

Only `NoAuthentication` is currently accepted. A host configured for either of the others is rejected at configuration time with `AuthenticationNotImplemented` rather than starting and silently accepting unauthenticated connections. See [issue 733](https://github.com/microsoft/MIDI/issues/733).
