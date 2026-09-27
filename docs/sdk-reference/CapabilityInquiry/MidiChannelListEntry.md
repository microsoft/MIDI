---
layout: sdk_reference_page
title: MidiChannelListEntry
namespace: Windows.Devices.Midi2.CapabilityInquiry
type: runtimeclass
implements: Windows.Foundation.IStringable
description: One channel a device describes, from its ChannelList resource
---

The channel number here **starts at 1 and goes up to 256**, not 16. The specification numbers channels across all sixteen groups, starting with the first channel of the first group. That's why this can't be a [`MidiChannel`]({{ site.baseurl }}/sdk-reference/MidiChannel), which is a channel within one group and stops at 16.

Also, the channel number starts at 1, but the bank and program numbers start at 0. That's how the specification does it. The values are passed through exactly as they arrive, so you don't have to undo a change before you send them back to the device.

## Constructors

| Constructor | Description |
| ----------- | ----------- |
| `MidiChannelListEntry()` | Creates an empty entry |
| `MidiChannelListEntry(channel, title)` | Creates an entry for a channel |

## Properties

| Property | Description |
| -------- | ----------- |
| `Title` | The channel's display name, from the entry's `title` |
| `Channel` | The channel number, from 1 to 256 |
| `BankMsb` | The bank select most significant byte of whatever is selected. Starts at 0 |
| `BankLsb` | The bank select least significant byte. Starts at 0 |
| `ProgramChange` | The program change number. Starts at 0 |
| `ProgramTitle` | The name of whatever is selected on the channel now, if the device reports one |
| `Links` | Where this channel's programs come from. For a device with several collections, following these is the only way to reach them |

## Methods

| Method | Description |
| ------ | ----------- |
| `GetJson()` | The entry as it appears inside a ChannelList resource |
| `ToString` | (From `IStringable`) A readable summary of the entry |

## Static Methods

| Static Method | Description |
| ------------- | ----------- |
| `FromJson(json)` | Reads one entry from its JSON object |
