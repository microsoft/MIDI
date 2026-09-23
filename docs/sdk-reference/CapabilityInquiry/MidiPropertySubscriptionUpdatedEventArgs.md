---
layout: sdk_reference_page
title: MidiPropertySubscriptionUpdatedEventArgs
namespace: Windows.Devices.Midi2.CapabilityInquiry
type: runtimeclass
description: A device telling a subscriber that a resource it asked about has changed
---

Raised on [`MidiCapabilityInquirySession`]({{ site.baseurl }}/sdk-reference/CapabilityInquiry/MidiCapabilityInquirySession) when a device sends an update for a subscription this session holds. The reply the specification requires is already sent by the time this is raised, so there is nothing to answer.

`Command` says what the device did:

- `full` carries the whole resource, and is what most devices send.
- `partial` carries only what changed, in the form the specification defines for a partial set.
- `notify` says only that something changed. Ask for the resource yourself if you need the value.
- `end` means the device dropped the subscription. `IsSubscriptionEnded` is true and the subscription is no longer active.

A device is allowed to end a subscription at any time, so handle `end` rather than assuming a subscription lasts as long as the session. Subscribe again if you still need the resource.

## Properties

| Property | Description |
| -------- | ----------- |
| `Subscription` | The subscription this update belongs to |
| `Command` | What the device did: `full`, `partial`, `notify` or `end` |
| `IsSubscriptionEnded` | True when the device ended the subscription |
| `Update` | The new data, when the update carried any, as a [`MidiPropertyExchangeResponse`]({{ site.baseurl }}/sdk-reference/CapabilityInquiry/MidiPropertyExchangeResponse) |
