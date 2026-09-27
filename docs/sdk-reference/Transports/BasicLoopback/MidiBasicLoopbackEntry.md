---
layout: sdk_reference_page
title: MidiBasicLoopbackEntry
namespace: Windows.Devices.Midi2.Transports.BasicLoopback
type: runtimeclass
description: Represents an active basic MIDI 1.0-style loopback endpoint instance
---

A temporary basic loopback endpoint that exists right now. You get these from `MidiBasicLoopbackManager.GetActiveLoopbackEntries()` and `MidiBasicLoopbackCreationResponse.CreatedLoopbackEntry`.

## Properties

| Property | Description |
| -------- | ----------- |
| `AssociationId` | The GUID that identifies this loopback |
| `EndpointDeviceId` | The full endpoint device id of this loopback endpoint |
| `Name` | The name of the loopback endpoint |
| `Description` | The description of the loopback endpoint |
| `ImageFileName` | The file name of the endpoint's picture in the shared endpoint assets folder, or empty if there's none |
| `IsMuted` | True if this loopback is muted now |
| `FeedbackProtection` | The loopback's [`MidiBasicLoopbackFeedbackProtection`]({{ site.baseurl }}/sdk-reference/Transports/BasicLoopback/MidiBasicLoopbackFeedbackProtectionEnum/) setting. `Off` when the transport on this PC can't watch for feedback |
| `IsMutedForFeedback` | True when the loopback was muted because MIDI was feeding back into it. Any change to the muted state clears it |
| `FeedbackDetectedTime` | When the feedback was found. Zero unless `IsMutedForFeedback` is true |
| `MessageCount` | The total number of UMP messages this loopback has passed from its destination back to its source since it was created. It starts over when the endpoint is created again, and doesn't count while the loopback is muted |
