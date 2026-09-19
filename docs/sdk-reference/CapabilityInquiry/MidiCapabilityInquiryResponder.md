---
layout: sdk_reference_page
title: MidiCapabilityInquiryResponder
namespace: Windows.Devices.Midi2.CapabilityInquiry
type: runtimeclass
implements: Windows.Foundation.IStringable
description: A device which answered Discovery, described by what it said about itself
---

One endpoint may hold several of these. A responder is a function block rather than a device, and each function block has its own identifier.

## Properties

| Property | Description |
| -------- | ----------- |
| `Muid` | Changes every time the device powers up, so it identifies the responder for as long as the session lasts and no longer. Never store it |
| `Identity` | The manufacturer, family, model and revision the responder declared |
| `SupportedCategories` | What the responder says it can do |
| `ReceivableMaximumSystemExclusiveSize` | The largest System Exclusive message this responder says it can receive. Everything sent to it is sized against this rather than against what the sender is able to send |
| `OutputPathId` | Which of our outputs the reply came back on, echoed from the Discovery we sent |
| `FunctionBlockNumber` | Which function block this responder is |
| `MessageVersion` | The MIDI-CI version the responder used. A device below version 2 leaves out fields that later messages carry |
| `SupportsPropertyExchange` | Read from `SupportedCategories`, and worth having by name because it decides whether a request is worth sending at all |
| `SupportsProfiles` | The same, for profile configuration |
| `SupportsProcessInquiry` | The same, for process inquiry |
| `MaximumSimultaneousPropertyRequests` | How many property exchange requests this responder is willing to have outstanding at once, from its reply to the capabilities inquiry. Zero until that reply arrives |

## Methods

| Method | Description |
| ------ | ----------- |
| `ToString` | (From `IStringable`) The identifier and the categories |
