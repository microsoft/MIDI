---
layout: sdk_reference_page
title: MidiChannelList
namespace: Windows.Devices.Midi2.CapabilityInquiry
type: runtimeclass
description: What a device says about each of its channels, from its ChannelList resource
---

This is also the map from channels to program lists. A device offering more than one collection of programs points each channel at the one it uses, so following the links is the only way to reach all of them.

## Constructors

| Constructor | Description |
| ----------- | ----------- |
| `MidiChannelList()` | Constructs an empty list |

## Properties

| Property | Description |
| -------- | ----------- |
| `Entries` | The channels the device described |

## Methods

| Method | Description |
| ------ | ----------- |
| `GetEntryForChannel(oneBasedChannel)` | The entry for a channel, or null when the device did not describe it. The argument is one based, matching the numbering the resource itself uses |
| `GetProgramListLinks()` | Every distinct program list the channels point at, in the order first encountered. This is what a caller walks to fetch all of a device's programs, and de-duplicating it here keeps a sixteen channel device from being asked for the same collection sixteen times |
| `GetJson()` | The ChannelList resource is a JSON array of entries |

## Static Methods

| Static Method | Description |
| ------------- | ----------- |
| `FromJson(json)` | Reads a list from the JSON array the resource is |
