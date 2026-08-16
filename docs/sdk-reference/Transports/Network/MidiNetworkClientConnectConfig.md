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
| `MidiNetworkClientConnectConfig()` | Create an empty config |

## Properties

| Property | Description |
| -------- | ----------- |
| `ClientId` | The GUID which identifies this client entry, used for later disconnect and to match the entry in the configuration file |
| `Comment` | Optional comment written to the configuration file. Not used by the service |
| `CreateOnlyUmpEndpoints` | When true, only UMP endpoints are created. When false, MIDI 1.0 ports are created alongside them |
| `UmpEndpointName` | The UMP Endpoint Name to use for the local end of this connection |
| `MatchCriteria` | A `MidiNetworkClientMatchCriteria` identifying the remote host |

## Remarks

Calling `ConnectNetworkClientAsync` with a `ClientId` which already exists does not create a duplicate. It re-arms the existing entry, which is how a direct connection marked `Unavailable` is retried.