---
layout: sdk_reference_page
title: MidiSystemExclusive7MessageBuilder
namespace: Windows.Devices.Midi2.Utilities.Messages
type: runtimeclass
description: Splits a System Exclusive payload across as many UMP packets as it needs
---

This class is the counterpart to [`MidiSystemExclusive7MessageHelper`]({{ site.baseurl }}/sdk-reference/Utilities/Messages/MidiSystemExclusive7MessageHelper), which reassembles what this produces.

The payload is the bytes **between** the `0xF0` and `0xF7` markers, not including them. The UMP packet's own status says where a message starts and ends, so the markers have no place on the wire here.

Every byte must be seven bit. A payload containing a byte with its high bit set returns an empty list rather than a masked message, because silently altering data is worse than refusing to send it.

## Static Methods

| Static Method | Description |
| ------------- | ----------- |
| `BuildSystemExclusive7Messages(timestamp, group, dataBytes)` | Returns the UMP messages which carry this payload, in the order they must be sent. Returns an empty list when any byte is not seven bit. |
| `GetMessageCountForDataByteCount(dataByteCount)` | How many packets a payload of this size will take, without building anything. An empty payload is still one packet: it is a complete, if empty, System Exclusive message. |

## Static Properties

| Static Property | Description |
| --------------- | ----------- |
| `MaxDataBytesPerMessage` | How many payload bytes one packet holds. Six. |
