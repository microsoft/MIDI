---
layout: sdk_reference_page
title: MidiProfileInquiryResponse
namespace: Windows.Devices.Midi2.CapabilityInquiry
type: runtimeclass
description: What a responder said about the profiles at one address
---

If both lists are empty, that's still a real answer. It means the responder has no profiles at the address you asked about. That's not the same as having no profiles anywhere, and it's not the same as not answering.

## Properties

| Property | Description |
| -------- | ----------- |
| `Status` | How the request ended |
| `ResponderMuid` | Which responder answered |
| `DeviceId` | The address the question was asked about: a channel from `0x00` to `0x0F`, a group with `0x7E`, or the whole function block with `0x7F` |
| `EnabledProfiles` | Profiles the responder supports that are turned on now |
| `DisabledProfiles` | Profiles the responder supports that are turned off. These are the ones worth offering to turn on |
