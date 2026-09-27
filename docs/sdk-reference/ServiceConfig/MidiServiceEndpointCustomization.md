---
layout: sdk_reference_page
title: MidiServiceEndpointCustomization
namespace: Windows.Devices.Midi2.ServiceConfig
type: runtimeclass
description: One stored endpoint customization as the service currently holds it
---

Read-only. This is the other half of `MidiServiceEndpointCustomizationConfig`. That type writes a customization. This one reports what's stored, including entries that don't match any endpoint on this PC.

Get these from `MidiServiceTransportPluginConfigManager.GetEndpointCustomizations`.

## Properties

| Property | Description |
| -------- | ----------- |
| `TransportId` | The transport this entry is stored under |
| `MatchCriteria` | The `MidiServiceConfigEndpointMatchCriteria` that says which endpoint this is for. It's also how the configuration file tells entries apart |
| `Provenance` | The `MidiServiceEndpointCustomizationProvenance` that describes the device this was made for |
| `Name` | The name the person gave the endpoint |
| `Description` | The description the person gave it |
| `ImageFileName` | The picture the person chose, as a file name with no folder |
| `RequiresNoteOffTranslation` | True when a Note On with a velocity of zero should be changed to a Note Off |
| `SupportsMidiPolyphonicExpression` | True when the endpoint is known to support MIDI Polyphonic Expression (MPE) |
| `RecommendedControlChangeIntervalMilliseconds` | The recommended shortest time, in milliseconds, between control change messages |
| `OutgoingLatencyTicks` | The stored outgoing latency adjustment. It can be negative |
| `UseCustomOutgoingLatency` | Whether the stored latency is used instead of the value the transport worked out |
| `Midi1PortNamingApproach` | How this endpoint's MIDI 1.0 ports are named |
| `Midi1SourcePortCustomNames` | Custom MIDI 1.0 source port names, by group index |
| `Midi1DestinationPortCustomNames` | Custom MIDI 1.0 destination port names, by group index |
| `ResolvedEndpointDeviceId` | The endpoint this entry applies to right now, or empty when it matches nothing |
| `IsOrphaned` | True when nothing on this PC matches |
| `HasUserContent` | False when the entry holds nothing the person would miss |

## Orphaned entries

An orphaned customization isn't an error, and nothing has been lost. The service keeps the entry and uses it again as soon as something matches. That's how a customization survives a device being unplugged.

It usually happens because an endpoint's id changed. For USB, the id comes from the device instance id. A device that reports no serial number is known by the hub and port it's plugged into. Plug it into a different port and Windows gives it a different id, so the stored entry no longer matches.

So `IsOrphaned` is a hint to link the entry to the device again, not an error. MIDI Settings can do that, and so can `midi endpoint customizations relink` from the command line.

## Entries that hold nothing

`HasUserContent` is false when every stored value is the default. An editor that saves everything at once can leave behind an entry with only defaults in it. That entry is worth nothing to the person, so don't offer to link it again.

It checks more than the fields people can see. A measured outgoing latency, a control change interval, the behavior flags, and a port naming approach other than the default all count. A name takes seconds to type again, but a measured latency needs a loopback cable and a new measurement.

## Changing or deleting an entry

This type can't make changes. To change a customization, send a `MidiServiceEndpointCustomizationConfig`. To delete one, save a `MidiServiceEndpointCustomizationRemovalConfig`.

To move an entry to a different endpoint, do both, in that order. The configuration file tells entries apart by their match criteria. So saving a customization with new match criteria adds a second entry, and doesn't replace the first. Save the new entry, check that it worked, and only then save the removal of the old one. If you do it the other way around and the save fails, the customization is lost.
