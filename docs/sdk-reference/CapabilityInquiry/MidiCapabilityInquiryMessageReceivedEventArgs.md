---
layout: sdk_reference_page
title: MidiCapabilityInquiryMessageReceivedEventArgs
namespace: Windows.Devices.Midi2.CapabilityInquiry
type: runtimeclass
description: A capability inquiry message that arrived without being asked for
---

Profile reports, subscription updates, and a device's own Discovery all arrive this way. So does anything that couldn't be matched to a request that's waiting for an answer.

## Properties

| Property | Description |
| -------- | ----------- |
| `Message` | The message, decoded |
| `Group` | The group it arrived on |
| `Timestamp` | When it arrived |
