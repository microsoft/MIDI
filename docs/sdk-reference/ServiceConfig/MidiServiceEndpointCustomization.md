---
layout: sdk_reference_page
title: MidiServiceEndpointCustomization
namespace: Windows.Devices.Midi2.ServiceConfig
type: runtimeclass
description: One stored endpoint customization as the service currently holds it
---

Read-only. This is the other half of `MidiServiceEndpointCustomizationConfig`: that type writes a customization, this one reports what is stored, including entries which match no endpoint on this machine.

Obtain these from `MidiServiceTransportPluginConfigManager.GetEndpointCustomizations`.

## Properties

| Property | Description |
| -------- | ----------- |
| `TransportId` | The transport whose section holds this entry |
| `MatchCriteria` | The `MidiServiceConfigEndpointMatchCriteria` which identifies the endpoint. This is also the entry's identity in the configuration file |
| `Provenance` | The `MidiServiceEndpointCustomizationProvenance` describing the device this was created for |
| `Name` | The customer's name for the endpoint |
| `Description` | The customer's description |
| `ImageFileName` | The picture the customer chose, as a bare file name |
| `RequiresNoteOffTranslation` | True when a Note On of zero velocity should be translated to a Note Off |
| `SupportsMidiPolyphonicExpression` | True when the endpoint is known to support MPE |
| `RecommendedControlChangeIntervalMilliseconds` | Recommended minimum interval between control change messages |
| `OutgoingLatencyTicks` | Stored outgoing latency compensation. May be negative |
| `UseCustomOutgoingLatency` | Whether the stored latency is used in place of the value the transport calculated |
| `Midi1PortNamingApproach` | The naming approach chosen for this endpoint's MIDI 1.0 ports |
| `Midi1SourcePortCustomNames` | Custom MIDI 1.0 source port names, keyed by group index |
| `Midi1DestinationPortCustomNames` | Custom MIDI 1.0 destination port names, keyed by group index |
| `ResolvedEndpointDeviceId` | The endpoint this entry currently applies to, or empty when it matches nothing |
| `IsOrphaned` | True when nothing on this machine matches |
| `HasUserContent` | False when the entry holds nothing the customer would miss |

## Orphaned entries

An orphaned customization is not a fault and nothing has been lost. The service keeps the entry and applies it the moment something matches again, which is what lets a customization survive a device being unplugged.

What usually causes it is an endpoint identifier changing. For USB, the identifier is derived from the device instance id, and a device which reports no serial number is identified by the hub and port it is plugged into. Move it to a different port and Windows gives it a different identity, so the stored entry no longer matches.

`IsOrphaned` is therefore a prompt to re-link the entry, not an error. The MIDI Settings app offers that, and `midi endpoint customizations relink` does the same from the command line.

## Entries which hold nothing

`HasUserContent` is false when every stored value is its default. An editor which saves the whole set at once can leave an entry behind holding only defaults, which is worth nothing to the customer and should not be offered for re-linking.

It counts more than the visible fields. A measured outgoing latency, a control change interval, the behavior flags and a non-default port naming approach all count, because a measured latency needs a loopback cable and a measurement to reproduce where a name takes seconds to retype.

## Changing or deleting an entry

This type does not write. To change a customization send a `MidiServiceEndpointCustomizationConfig`, and to delete one save a `MidiServiceEndpointCustomizationRemovalConfig`.

Moving an entry onto a different endpoint is both of those, in that order. The stored match object is the entry's identity in the configuration file, so writing a customization under a new match appends a second entry rather than replacing the first. Save the new entry, confirm it, and only then save the removal of the old one. Doing it the other way round risks losing the customization if the write fails.
