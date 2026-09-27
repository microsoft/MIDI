---
layout: sdk_reference_page
title: MidiNetworkRemoteClientApprovalErrorCode
namespace: Windows.Devices.Midi2.Transports.Network
type: enum
description: Error codes returned when approving or denying a remote Network MIDI 2.0 client
---

Returned in `MidiNetworkRemoteClientApprovalResponse.ErrorCode`.

## Values

| Value | Numeric Value | Description |
| ----- | ------------- | ----------- |
| `NoErrorInformationAvailable` | `0x00000000` | No additional error information is available |
| `UnrecognizedCommand` | `0x00000001` | The service didn't recognize the command |
| `HostNotFound` | `0x00001065` | No host with this id is set up |
| `PendingRemoteClientNotFound` | `0x00000074` | No waiting client matches this name and id. It may have given up |
| `InvalidOrMissingRemoteClientIdentity` | `0x00000073` | Both the UMP endpoint name and the product instance id are needed to identify the client |
| `InvalidOrMissingEntryIdentifier` | `0x00000031` | `HostId` was missing |
| `MalformedEntryIdentifier` | `0x00000032` | `HostId` wasn't a valid GUID |
| `InvalidArgument` | `0x11000055` | Your code passed an argument that isn't valid |
| `ClientApiException` | `0x11002011` | An exception happened in the client API |
