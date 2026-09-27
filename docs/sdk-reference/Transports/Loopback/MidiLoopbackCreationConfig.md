---
layout: sdk_reference_page
title: MidiLoopbackCreationConfig
namespace: Windows.Devices.Midi2.Transports.Loopback
type: runtimeclass
implements: Windows.Devices.Midi2.ServiceConfig.IMidiServiceTransportPluginConfig
description: Config sent to the service to create a loopback endpoint pair
---

The configuration your app sends to the service to create a temporary loopback endpoint pair.

## Constructors

| Constructor | Description |
| -------- | ----------- |
| `MidiLoopbackCreationConfig()` | Creates an empty configuration |
| `MidiLoopbackCreationConfig(endpointDefinitionA, endpointDefinitionB)` | Creates a configuration with these endpoint definitions |

## Properties

| Property | Description |
| -------- | ----------- |
| `AssociationId` | Read-only. A GUID that identifies this loopback pair. It's made when the configuration is created. Use it to remove the loopback later |
| `EndpointDefinitionA` | The `MidiLoopbackEndpointDefinition` for the A side of the pair |
| `EndpointDefinitionB` | The `MidiLoopbackEndpointDefinition` for the B side of the pair |
| `IsMuted` | When true, the loopback endpoints are created, but no messages get through |
| `FeedbackProtection` | What the loopback pair does if MIDI feeds back into it, as a [`MidiLoopbackFeedbackProtection`]({{ site.baseurl }}/sdk-reference/Transports/Loopback/MidiLoopbackFeedbackProtectionEnum/). `Mute` by default. Ignored when the transport can't watch for feedback |

## Remarks

The association id is made for you, not supplied by you, because it's an internal id that means nothing to people. Setting a definition also fills in its `UniqueId` if you left that empty. So if you only want to name the endpoints, you don't have to make up any ids.
