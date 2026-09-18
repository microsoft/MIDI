---
layout: sdk_reference_page
title: MidiChannelListEntry
namespace: Windows.Devices.Midi2.CapabilityInquiry
type: runtimeclass
implements: Windows.Foundation.IStringable
description: One channel a device describes, from its ChannelList resource
---

The channel number here is **one based, and runs from 1 to 256** rather than 1 to 16. The specification numbers channels from the first channel of the first group and runs across all sixteen groups, which is why this cannot be a [`MidiChannel`]({{ site.baseurl }}/sdk-reference/MidiChannel): that type addresses a channel within one group and stops at 16.

Note also that the channel number is one based while the bank and program values are zero based. That is the specification's own inconsistency, carried through as it arrives rather than quietly corrected into something a caller would have to undo before transmitting.

## Constructors

| Constructor | Description |
| ----------- | ----------- |
| `MidiChannelListEntry()` | Constructs an empty entry |
| `MidiChannelListEntry(channel, title)` | Constructs an entry for a channel |

## Properties

| Property | Description |
| -------- | ----------- |
| `Title` | Display name for the channel, from the entry's `title` |
| `Channel` | One based, and 1 to 256 rather than 1 to 16 |
| `BankMsb` | Bank select most significant byte of whatever is selected. Zero based |
| `BankLsb` | Bank select least significant byte. Zero based |
| `ProgramChange` | Program change number. Zero based |
| `ProgramTitle` | Name of whatever is currently selected on the channel, when the device reports one |
| `Links` | Where this channel's programs come from. A device offering several collections is only reachable by following these |

## Methods

| Method | Description |
| ------ | ----------- |
| `GetJson()` | The entry as it appears inside a ChannelList resource |
| `ToString` | (From `IStringable`) A readable summary of the entry |

## Static Methods

| Static Method | Description |
| ------------- | ----------- |
| `FromJson(json)` | Reads one entry from its JSON object |
