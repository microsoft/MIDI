---
layout: sdk_reference_page
title: MidiCapabilityInquiryCategories
namespace: Windows.Devices.Midi2.CapabilityInquiry
type: enum
description: What a device says it can do, declared once in its Discovery message
---

A device answers only the kinds of questions it lists here, so check this before you ask it anything. These values are flags, so a device can list more than one.

Protocol negotiation is on the list even though MIDI-CI version 1.2 dropped it, because UMP stream messages now do that job. Devices made before then still list it, so you may see it.

| Value | Number | Description |
| ----- | ------ | ----------- |
| `None` | 0x00000000 | The device lists no categories. It still has to answer Discovery |
| `ProtocolNegotiation` | 0x00000002 | Dropped in MIDI-CI version 1.2. Older devices still list it |
| `ProfileConfiguration` | 0x00000004 | The device answers questions about profiles, and responds to Set Profile On and Set Profile Off |
| `PropertyExchange` | 0x00000008 | The device publishes resources such as DeviceInfo, ChannelList, and ProgramList |
| `ProcessInquiry` | 0x00000010 | The device supports process inquiry. This API doesn't have types for process inquiry yet |
