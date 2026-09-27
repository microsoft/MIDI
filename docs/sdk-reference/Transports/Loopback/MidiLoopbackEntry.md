---
layout: sdk_reference_page
title: MidiLoopbackEntry
namespace: Windows.Devices.Midi2.Transports.Loopback
type: runtimeclass
description: Represents an active loopback endpoint pair instance
---

A temporary loopback endpoint pair that exists right now. You get these from `MidiLoopbackManager.GetActiveLoopbackEntries()` and `MidiLoopbackCreationResponse.CreatedLoopbackEntry`.

## Properties

| Property | Description |
| -------- | ----------- |
| `AssociationId` | The GUID that identifies this loopback pair |
| `EndpointA` | A `MidiLoopbackEndpointEntry` for the A side |
| `EndpointB` | A `MidiLoopbackEndpointEntry` for the B side |
| `IsMuted` | True if this loopback pair is muted now |
| `FeedbackProtection` | The pair's [`MidiLoopbackFeedbackProtection`]({{ site.baseurl }}/sdk-reference/Transports/Loopback/MidiLoopbackFeedbackProtectionEnum/) setting. `Off` when the transport on this PC can't watch for feedback |
| `IsMutedForFeedback` | True when the pair was muted because MIDI was feeding back into it. Any change to the muted state clears it |
| `FeedbackDetectedTime` | When the feedback was found. Zero unless `IsMutedForFeedback` is true |
