---
layout: sdk_reference_page
title: MidiServiceEndpointCustomizationRemovalConfig
namespace: Windows.Devices.Midi2.ServiceConfig
type: runtimeclass
description: Deletes a stored endpoint customization from the configuration file
---

Implements `IMidiServiceTransportPluginConfig`.

Saving a `MidiServiceEndpointCustomizationConfig` merges it with what's already stored, so changing a name doesn't erase a stored description. Because of that merge, writing over an entry never removes it. Use this type to take an entry back out of the configuration file.

## Constructors

| Constructor | Description |
| -------- | ----------- |
| `MidiServiceEndpointCustomizationRemovalConfig(transportId)` | Creates a removal for this transport. Set `MatchCriteria` before you save it |
| `MidiServiceEndpointCustomizationRemovalConfig(transportId, matchCriteria)` | Creates a removal for the endpoint these match criteria identify |

## Properties

| Property | Description |
| -------- | ----------- |
| `MatchCriteria` | The `MidiServiceConfigEndpointMatchCriteria` for the entry to delete. Only the properties you set need to match |
| `TransportId` | The transport the entry is stored under |
| `ConfigJson` | This removal, as JSON |

Pass this to `MidiServiceTransportPluginConfigManager.SaveUpdate`. The entry is removed while the file is being written, and the removal itself isn't stored. So the file doesn't fill up with instructions to delete things that are already gone.

Removing a stored customization doesn't change an endpoint that's already running. To change both, send a `MidiServiceEndpointCustomizationConfig` with `ClearDisplayProperties` set, and then save the removal.
