---
layout: sdk_reference_page
title: MidiNetworkClientConnectConfig
namespace: Windows.Devices.Midi2.Transports.Network
type: runtimeclass
implements: Windows.Devices.Midi2.ServiceConfig.IMidiServiceTransportPluginConfig
description: Config sent to the service to connect to a remote Network MIDI 2.0 host
---

Pass to `MidiNetworkTransportManager.ConnectNetworkClientAsync`.

## Constructors

| Constructor | Description |
| -------- | ----------- |
| `MidiNetworkClientConnectConfig()` | Creates an empty configuration |

## Properties

| Property | Description |
| -------- | ----------- |
| `ClientId` | The GUID that identifies this client entry. It's used to disconnect later, and to find the entry in the configuration |
| `Comment` | An optional comment saved with the entry in the configuration. The service doesn't use it |
| `CreateOnlyUmpEndpoints` | When true, only UMP endpoints are created. When false, MIDI 1.0 ports are created with them |
| `FallbackMidi1PortCount` | How many source and destination ports to create when the remote host declares no function blocks. 1 to 16, and 1 by default. Ignored when the host does describe itself, and when `CreateOnlyUmpEndpoints` is true |
| `UmpEndpointName` | The UMP Endpoint Name to use for this PC's end of the connection |
| `CustomEndpointName` | The name the person chose for the MIDI endpoint this connection creates. It's applied before the endpoint is turned on, so the endpoint and its MIDI 1.0 ports never appear under the remote device's own name first. Leave it empty to use the name the remote device announces |
| `MatchCriteria` | A `MidiNetworkClientMatchCriteria` that says which remote host to connect to |

## Remarks

Calling `ConnectNetworkClientAsync` with a `ClientId` that already exists doesn't create a copy. It starts the existing entry trying again, which is how you retry a direct connection marked `Unavailable`.