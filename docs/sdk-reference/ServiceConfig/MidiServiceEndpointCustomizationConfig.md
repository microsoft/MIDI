---
layout: sdk_reference_page
title: MidiServiceEndpointCustomizationConfig
namespace: Windows.Devices.Midi2.ServiceConfig
type: runtimeclass
implements: Windows.Devices.Midi2.ServiceConfig.IMidiServiceTransportPluginConfig
description: Configuration object for customizing a MIDI endpoint in the service
---

Use this class to send endpoint customizations to the service. What you set here replaces the values the transport supplied for the matching endpoint.

## Constructors

| Constructor | Description |
| ----------- | ----------- |
| `MidiServiceEndpointCustomizationConfig()` | Creates an empty customization with no transport id. You can't set `TransportId` later, so use one of the other constructors if you plan to send it to the service |
| `MidiServiceEndpointCustomizationConfig(transportId)` | Creates an empty customization for this transport |
| `MidiServiceEndpointCustomizationConfig(transportId, name, description)` | Creates a customization with a name and description |
| `MidiServiceEndpointCustomizationConfig(transportId, name, description, imageFileName)` | Creates a customization with a name, description, and image |
| `MidiServiceEndpointCustomizationConfig(transportId, name, description, imageFileName, requiresNoteOffTranslation, supportsMidiPolyphonicExpression, recommendedControlChangeIntervalMilliseconds)` | Creates a customization with all of these values set |

## Properties

| Property | Description |
| -------- | ----------- |
| `Name` | The name to show for the endpoint |
| `Description` | The description to show for the endpoint |
| `ImageFileName` | The image file to show for the endpoint |
| `ClearDisplayProperties` | Set this when your code manages all of these values at once, as an editor does. Then an empty name, description, or image is saved as empty, instead of being left out. That's how you clear a stored value |
| `MatchCriteria` | The `MidiServiceConfigEndpointMatchCriteria` that says which endpoint this applies to |
| `Provenance` | A [`MidiServiceEndpointCustomizationProvenance`]({{ site.baseurl }}/sdk-reference/ServiceConfig/MidiServiceEndpointCustomizationProvenance/) that records which device this entry was made for, so people can still recognize the entry if the endpoint's id changes later. It isn't used for matching. Set it whenever you create or edit a customization. `MidiServiceEndpointCustomizationProvenance.CreateForEndpoint` fills it in for you |
| `RequiresNoteOffTranslation` | True if the endpoint needs a Note On with a velocity of 0 changed to a Note Off |
| `SupportsMidiPolyphonicExpression` | True if the endpoint supports MIDI Polyphonic Expression (MPE) |
| `RecommendedControlChangeIntervalMilliseconds` | The recommended time, in milliseconds, between control change messages |
| `OutgoingLatencyTicks` | The outgoing latency, in `MidiClock` ticks, to make up for when scheduling outgoing messages. It can be negative, for an endpoint that runs early once the endpoints around it are adjusted |
| `UseCustomOutgoingLatency` | Whether to use `OutgoingLatencyTicks` instead of the value the transport worked out. Setting it either way makes the choice clear, so you can keep a measured value while the adjustment is turned off. If you don't set it, a latency other than zero still means the value is used |
| `Midi1PortNamingApproach` | The `Midi1PortNamingApproach` to use when naming MIDI 1.0 ports |

## Methods

| Method | Description |
| ------ | ----------- |
| `AddMidi1SourcePortCustomName(group, name)` | Adds a custom name for the MIDI 1.0 source port on this group |
| `AddMidi1DestinationPortCustomName(group, name)` | Adds a custom name for the MIDI 1.0 destination port on this group |

An empty `Name`, `Description`, or `ImageFileName` is left out of the saved configuration, so saving a name doesn't erase a stored description. Because of that, writing over an entry never removes it. To delete a stored customization completely, use `MidiServiceEndpointCustomizationRemovalConfig`.
