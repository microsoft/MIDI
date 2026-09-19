---
layout: sdk_reference_page
title: MidiCapabilityInquiryCategories
namespace: Windows.Devices.Midi2.CapabilityInquiry
type: enum
description: What a device says it can do, declared once in its Discovery message
---

A device answers only the categories it declares here, so read this before asking it for anything. It is a flags enumeration.

Protocol negotiation is in the list even though MIDI-CI version 1.2 retired it in favor of UMP stream messages. Devices built before that still declare it, and an application reading the field will see it.

| Value | Number | Description |
| ----- | ------ | ----------- |
| `None` | 0x00000000 | The device declares no capability inquiry categories at all. It still has to answer Discovery |
| `ProtocolNegotiation` | 0x00000002 | Retired in MIDI-CI version 1.2. Still declared by older devices |
| `ProfileConfiguration` | 0x00000004 | The device will answer profile inquiries and honor Set Profile On and Off |
| `PropertyExchange` | 0x00000008 | The device publishes resources such as DeviceInfo, ChannelList and ProgramList |
| `ProcessInquiry` | 0x00000010 | The device supports process inquiry, which this API does not currently model |
