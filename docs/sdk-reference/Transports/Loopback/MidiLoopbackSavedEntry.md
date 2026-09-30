---
layout: sdk_reference_page
title: MidiLoopbackSavedEntry
namespace: Windows.Devices.Midi2.Transports.Loopback
type: runtimeclass
description: A loopback pair saved in the configuration file
---

A loopback pair saved in the configuration file. The service creates it every time it starts. `MidiLoopbackManager.GetSavedLoopbackEntries` returns one of these for each saved pair.

## Properties

| Property | Description |
| -------- | ----------- |
| `AssociationId` | The GUID that identifies this loopback pair. It's the same as `MidiLoopbackEntry.AssociationId` when the pair is running |
| `EndpointDefinitionA` | A `MidiLoopbackEndpointDefinition` with the saved name, description, unique id, picture and UMP-only setting for the A side |
| `EndpointDefinitionB` | The same, for the B side |
| `IsMuted` | True if the pair is saved as muted |
| `FeedbackProtection` | The saved [`MidiLoopbackFeedbackProtection`]({{ site.baseurl }}/sdk-reference/Transports/Loopback/MidiLoopbackFeedbackProtectionEnum/) setting. A transport that can't watch for feedback ignores it |

## Saved isn't the same as running

This list comes from the configuration file, not from the service. It tells you what the service creates the next time it starts, and it works even when the service isn't running.

A pair can be saved and not running, for example when the service couldn't create it. A pair can also be running and not saved, because an app created it with `CreateTransientLoopback`. To show both, combine this list with `MidiLoopbackManager.GetActiveLoopbackEntries`, matching on `AssociationId`.

The endpoint definitions are copies. Changing them doesn't change anything that's saved. To change a saved pair, save a [`MidiLoopbackUpdateConfig`]({{ site.baseurl }}/sdk-reference/Transports/Loopback/MidiLoopbackUpdateConfig/).
