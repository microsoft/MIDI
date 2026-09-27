---
layout: sdk_reference_page
title: MidiBasicLoopbackCreationConfig
namespace: Windows.Devices.Midi2.Transports.BasicLoopback
type: runtimeclass
implements: Windows.Devices.Midi2.ServiceConfig.IMidiServiceTransportPluginConfig
description: Config sent to the service to create a basic MIDI 1.0-style loopback endpoint
---

The configuration your app sends to the service to create a temporary basic loopback endpoint.

## Constructors

| Constructor | Description |
| -------- | ----------- |
| `MidiBasicLoopbackCreationConfig()` | Creates an empty configuration |
| `MidiBasicLoopbackCreationConfig(endpointDefinition)` | Creates a configuration with this endpoint definition |

## Properties

| Property | Description |
| -------- | ----------- |
| `AssociationId` | Read-only. A GUID that identifies this loopback. It's made when the configuration is created. Use it to remove the loopback later |
| `EndpointDefinition` | The `MidiBasicLoopbackEndpointDefinition` for this loopback |
| `IsMuted` | When true, the loopback endpoint is created, but no messages get through |
| `FeedbackProtection` | What the loopback does if MIDI feeds back into it, as a [`MidiBasicLoopbackFeedbackProtection`]({{ site.baseurl }}/sdk-reference/Transports/BasicLoopback/MidiBasicLoopbackFeedbackProtectionEnum/). `Mute` by default. Ignored when the transport can't watch for feedback |

## Remarks

The association id is made for you, not supplied by you, because it's an internal id that means nothing to people. Setting the definition also fills in its `UniqueId` if you left that empty. So if you only want to name the endpoint, you don't have to make up any ids.
