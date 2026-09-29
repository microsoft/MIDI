---
layout: sdk_reference_page
title: MidiBasicLoopbackSavedEntry
namespace: Windows.Devices.Midi2.Transports.BasicLoopback
type: runtimeclass
description: A basic loopback saved in the configuration file
---

A basic loopback saved in the configuration file. The service creates it every time it starts. `MidiBasicLoopbackManager.GetSavedLoopbackEntries` returns one of these for each saved loopback.

## Properties

| Property | Description |
| -------- | ----------- |
| `AssociationId` | The GUID that identifies this loopback. It's the same as `MidiBasicLoopbackEntry.AssociationId` when the loopback is running |
| `EndpointDefinition` | A `MidiBasicLoopbackEndpointDefinition` with the saved name, description, unique id and picture |
| `IsMuted` | True if the loopback is saved as muted |
| `FeedbackProtection` | The saved [`MidiBasicLoopbackFeedbackProtection`]({{ site.baseurl }}/sdk-reference/Transports/BasicLoopback/MidiBasicLoopbackFeedbackProtectionEnum/) setting. A transport that can't watch for feedback ignores it |

## Saved isn't the same as running

This list comes from the configuration file, not from the service. It tells you what the service creates the next time it starts, and it works even when the service isn't running.

A loopback can be saved and not running, or running and not saved. To show both, combine this list with `MidiBasicLoopbackManager.GetActiveLoopbackEntries`, matching on `AssociationId`.

The endpoint definition is a copy. Changing it doesn't change anything that's saved. To change a saved loopback, save a [`MidiBasicLoopbackUpdateConfig`]({{ site.baseurl }}/sdk-reference/Transports/BasicLoopback/MidiBasicLoopbackUpdateConfig/).
