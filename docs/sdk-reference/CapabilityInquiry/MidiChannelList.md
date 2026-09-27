---
layout: sdk_reference_page
title: MidiChannelList
namespace: Windows.Devices.Midi2.CapabilityInquiry
type: runtimeclass
description: What a device says about each of its channels, from its ChannelList resource
---

This list also tells you which program list each channel uses. A device with more than one collection of programs points each channel at the one it uses, so following those links is the only way to reach all of them.

## Constructors

| Constructor | Description |
| ----------- | ----------- |
| `MidiChannelList()` | Creates an empty list |

## Properties

| Property | Description |
| -------- | ----------- |
| `Entries` | The channels the device described |

## Methods

| Method | Description |
| ------ | ----------- |
| `GetEntryForChannel(oneBasedChannel)` | The entry for a channel, or null if the device didn't describe it. Channels are numbered from 1, the same way the resource numbers them |
| `GetProgramListLinks()` | Every different program list the channels point at, in the order they're first found. Go through these to get all of a device's programs. Each list appears only once, so a device with sixteen channels isn't asked for the same collection sixteen times |
| `GetJson()` | The list as JSON. The ChannelList resource is a JSON array of entries |

## Static Methods

| Static Method | Description |
| ------------- | ----------- |
| `FromJson(json)` | Reads a list from the resource's JSON array |
