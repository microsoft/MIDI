---
layout: sdk_reference_page
title: MidiNetworkRemoteClientForgetErrorCode
namespace: Windows.Devices.Midi2.Transports.Network
type: enum
description: Error codes for dropping a remembered decision for a remote client
---

Used by `MidiNetworkRemoteClientForgetResponse.ErrorCode`.

## Values

| Value | Numeric Value | Description |
| ----- | ------------- | ----------- |
| `NoErrorInformationAvailable` | `0x00000000` | No additional error information is available |
| `UnrecognizedCommand` | `0x00000001` | The service did not recognize the command, which means it predates it |
| `InvalidOrMissingEntryIdentifier` | `0x00000031` | Host identifier is missing or invalid |
| `MalformedEntryIdentifier` | `0x00000032` | Host identifier format is malformed |
| `InvalidOrMissingRemoteClientIdentity` | `0x00000073` | Required remote client identity fields are missing |
| `HostNotFound` | `0x00001065` | The target host entry was not found |
| `InvalidArgument` | `0x11000055` | One or more arguments were invalid |
| `ClientApiException` | `0x11002011` | A client-side API exception occurred while processing the request |

## Remarks

There is deliberately no `RemoteClientNotFound` here. Unlike a disconnect, forgetting an identity the host holds no decision for succeeds, because what the caller asked for is already true.
