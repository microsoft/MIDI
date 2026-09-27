---
layout: sdk_reference_page
title: MidiSystemExclusive7MessageBuilder
namespace: Windows.Devices.Midi2.Utilities.Messages
type: runtimeclass
description: Splits System Exclusive data across as many UMP packets as it needs
---

This class is the partner of [`MidiSystemExclusive7MessageHelper`]({{ site.baseurl }}/sdk-reference/Utilities/Messages/MidiSystemExclusive7MessageHelper/), which puts back together what this class splits up.

The data is the bytes **between** the `0xF0` and `0xF7` markers, not the markers themselves. Each UMP packet's own status says where a message starts and ends, so the markers aren't sent.

Every byte must be a 7-bit value. If any byte has its high bit set, you get back an empty list, not a message with that bit removed, because quietly changing data is worse than refusing to send it.

## Static Methods

| Static Method | Description |
| ------------- | ----------- |
| `BuildSystemExclusive7Messages(timestamp, group, dataBytes)` | Returns the UMP messages that carry this data, in the order they must be sent. Returns an empty list if any byte isn't a 7-bit value |
| `GetMessageCountForDataByteCount(dataByteCount)` | How many packets data of this size needs, without building anything. Empty data is still one packet, because it's a complete (empty) System Exclusive message |

## Static Properties

| Static Property | Description |
| --------------- | ----------- |
| `MaxDataBytesPerMessage` | How many data bytes one packet holds: six |
