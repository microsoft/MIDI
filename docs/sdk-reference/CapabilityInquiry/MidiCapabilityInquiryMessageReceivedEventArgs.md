---
layout: sdk_reference_page
title: MidiCapabilityInquiryMessageReceivedEventArgs
namespace: Windows.Devices.Midi2.CapabilityInquiry
type: runtimeclass
description: A capability inquiry message which arrived without having been asked for
---

Profile reports, subscription updates and a device's own Discovery all come this way, as does anything which could not be matched to an outstanding request.

## Properties

| Property | Description |
| -------- | ----------- |
| `Message` | The message, decoded |
| `Group` | The group it arrived on |
| `Timestamp` | When it arrived |
