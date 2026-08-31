---
layout: sdk_reference_page
title: MidiServiceEndpointCustomizationRemovalConfig
namespace: Windows.Devices.Midi2.ServiceConfig
type: runtimeclass
description: Deletes a stored endpoint customization from the configuration file
---

Implements `IMidiServiceTransportPluginConfig`.

Saving a `MidiServiceEndpointCustomizationConfig` merges into whatever is already stored, so that changing a name does not wipe a stored description. That merge means an entry can never be emptied by overwriting it. This type is how an entry is taken back out of the configuration file.

## Constructors

| Constructor | Description |
| -------- | ----------- |
| `MidiServiceEndpointCustomizationRemovalConfig(transportId)` | Creates a removal for the given transport. Set `MatchCriteria` before saving. |
| `MidiServiceEndpointCustomizationRemovalConfig(transportId, matchCriteria)` | Creates a removal for the endpoint identified by the match criteria. |

## Properties

| Property | Description |
| -------- | ----------- |
| `MatchCriteria` | The `MidiServiceConfigEndpointMatchCriteria` identifying the entry to delete. Only the properties which are set need to match. |
| `TransportId` | The transport whose section holds the entry. |
| `ConfigJson` | The configuration file representation of this removal. |

Pass this to `MidiServiceTransportPluginConfigManager.SaveUpdate`. The removal is carried out while the file is being written and is not itself stored, so the file never accumulates instructions to delete things which are already gone.

Removing a stored customization does not revert an endpoint which is already running. To do both, send a `MidiServiceEndpointCustomizationConfig` with `ClearDisplayProperties` set, then save the removal.
