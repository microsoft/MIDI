---
layout: sdk_reference_page
title: MidiServiceEndpointCustomizationConfig
namespace: Windows.Devices.Midi2.ServiceConfig
type: runtimeclass
implements: Windows.Devices.Midi2.ServiceConfig.IMidiServiceTransportPluginConfig
description: Configuration object for customizing a MIDI endpoint in the service
---

This class is used to send endpoint customization settings to the service. It contains information that will override transport-supplied properties for the matched endpoint.

## Constructors

| Constructor | Description |
| ----------- | ----------- |
| `MidiServiceEndpointCustomizationConfig(transportId)` | Create an empty customization config for the specified transport |
| `MidiServiceEndpointCustomizationConfig(transportId, name, description)` | Create a customization config with a name and description |
| `MidiServiceEndpointCustomizationConfig(transportId, name, description, imageFileName)` | Create a customization config with name, description, and image |
| `MidiServiceEndpointCustomizationConfig(transportId, name, description, imageFileName, requiresNoteOffTranslation, supportsMidiPolyphonicExpression, recommendedControlChangeIntervalMilliseconds)` | Create a fully-specified customization config |

## Properties

| Property | Description |
| -------- | ----------- |
| `Name` | Display name for this customization |
| `Description` | Description of this customization |
| `ImageFileName` | Path to an icon file for this customization |
| `MatchCriteria` | The `MidiServiceConfigEndpointMatchCriteria` that identifies which endpoint this applies to |
| `RequiresNoteOffTranslation` | True if the endpoint requires Note On velocity 0 to be translated to Note Off |
| `SupportsMidiPolyphonicExpression` | True if the endpoint supports MIDI Polyphonic Expression (MPE) |
| `RecommendedControlChangeIntervalMilliseconds` | Recommended interval in milliseconds between control change messages |
| `OutgoingLatencyTicks` | Outgoing latency in MIDI clock ticks to add to outgoing message scheduling |
| `Midi1PortNamingApproach` | The `Midi1PortNamingApproach` to use when generating MIDI 1.0 port names |

## Methods

| Method | Description |
| ------ | ----------- |
| `AddMidi1SourcePortCustomName(group, name)` | Add a custom name for a MIDI 1.0 source port for the specified group |
| `AddMidi1DestinationPortCustomName(group, name)` | Add a custom name for a MIDI 1.0 destination port for the specified group |
