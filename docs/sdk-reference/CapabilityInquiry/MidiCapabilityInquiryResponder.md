---
layout: sdk_reference_page
title: MidiCapabilityInquiryResponder
namespace: Windows.Devices.Midi2.CapabilityInquiry
type: runtimeclass
implements: Windows.Foundation.IStringable
description: A device that answered Discovery, described by what it said about itself
---

One endpoint can have several of these. A responder is a function block, not a whole device, and each function block has its own identifier.

## Properties

| Property | Description |
| -------- | ----------- |
| `Muid` | Changes every time the device powers up, so it identifies the responder only for as long as the session lasts. Never store it |
| `Identity` | The manufacturer, family, model, and version the responder gave |
| `SupportedCategories` | What the responder says it can do |
| `ReceivableMaximumSystemExclusiveSize` | The largest System Exclusive message this responder says it can receive. Everything sent to it is sized to fit this, not to what the sender could send |
| `OutputPathId` | Which of your outputs the reply came back on. The device sends back the value from the Discovery message you sent |
| `FunctionBlockNumber` | Which function block this responder is |
| `MessageVersion` | The MIDI-CI version the responder used. A device below version 2 leaves out fields that later versions include |
| `SupportsPropertyExchange` | Read from `SupportedCategories`. It has its own property because it decides whether a request is worth sending at all |
| `SupportsProfiles` | The same, for profile configuration |
| `SupportsProcessInquiry` | The same, for process inquiry |
| `MaximumSimultaneousPropertyRequests` | How many property exchange requests this responder will take at once, from its answer to the capabilities inquiry. Zero until that answer arrives |

## Methods

| Method | Description |
| ------ | ----------- |
| `ToString` | (From `IStringable`) The identifier and the categories |
