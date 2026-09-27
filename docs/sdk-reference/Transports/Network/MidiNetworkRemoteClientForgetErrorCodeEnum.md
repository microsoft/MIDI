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
| `UnrecognizedCommand` | `0x00000001` | The service didn't recognize the command, which means the service is older than this command |
| `InvalidOrMissingEntryIdentifier` | `0x00000031` | The host id is missing or not valid |
| `MalformedEntryIdentifier` | `0x00000032` | The host id isn't in the right format |
| `InvalidOrMissingRemoteClientIdentity` | `0x00000073` | The remote client's name or product instance id is missing |
| `HostNotFound` | `0x00001065` | The host entry wasn't found |
| `InvalidArgument` | `0x11000055` | One or more arguments weren't valid |
| `ClientApiException` | `0x11002011` | An exception happened in the client API while handling the request |

## Remarks

There's no `RemoteClientNotFound` here, on purpose. Unlike a disconnect, forgetting a client the host has no decision for succeeds, because what you asked for is already true.
