---
layout: sdk_reference_page
title: MidiProfileInquiryResponse
namespace: Windows.Devices.Midi2.CapabilityInquiry
type: runtimeclass
description: What a responder said about the profiles at one address
---

Both lists being empty is a real answer. It means the responder supports no profiles where it was asked, which is not the same as it supporting none anywhere, and not the same as it not answering.

## Properties

| Property | Description |
| -------- | ----------- |
| `Status` | How the request ended |
| `ResponderMuid` | Which responder answered |
| `DeviceId` | The address the question was asked at: a channel from `0x00` to `0x0F`, a group with `0x7E`, or the whole function block with `0x7F` |
| `EnabledProfiles` | Profiles the responder supports and which are active now |
| `DisabledProfiles` | Profiles the responder can support but which are not active. These are the ones worth offering to turn on |
