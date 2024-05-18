---
layout: api_page
title: MidiClock
parent: Microsoft.Devices.Midi2
has_children: false
---

# MidiClock

The MidiClock is what is used for all timestamps in Windows MIDI Services. Although it is internally backed by `QueryPerformanceCounter`, we recommend using the MidiClock type directly instead of calling QPC yourself.

Also note that `QueryPerformanceCounter` technically returns a signed 64 bit integer, but the timestamp values used in Windows MIDI Services are unsigned 64 bit integers. Typically, this is of no practical concern as the tick resolution is currently 100ns and takes tens of thousands of years to wrap around even with a 64 bit signed integer.

> Note: The MIDI Clock is unrelated to wall clock time. It is an ever-increasing value of period 1/`TimestampFrequency` seconds that starts over when the PC is rebooted. To convert to wall clock time, you need to get the `MidiClock.Now` value at a known time, and then use that as a baseline until the next time you reboot the PC.

You can learn more about high-resolution timestamps in Windows at [https://aka.ms/miditimestamp](https://aka.ms/miditimestamp).

## Static Properties

| Static Property | Description |
| --------------- | ----------- |
| `Now` | Returns the current timestamp |
| `TimestampFrequency` | Returns the number of timestamp ticks per second. This is calculated the first time it is called, and then cached for future calls. |
| `TimestampConstantSendImmediately` | Returns the constant to use when you want to send messages immediately and bypass outgoing message scheduling. Developers may use this value or simply provide `0` in place of the timestamp when sending messages.  |

## Static Functions for Conversion

The static functions are for convenience in calculating offsets to a timestamp, and for converting between units.

| Static Function | Description |
| --------------- | ----------- |
| `ConvertTimestampToMicroseconds(timestampValue)` | Converts the provided timestamp to microseconds |
| `ConvertTimestampToMilliseconds(timestampValue)` | Converts the provided timestamp to milliseconds |
| `ConvertTimestampToSeconds(timestampValue)` | Converts the provided timestamp to seconds |

## Static Functions for Offset

When scheduling messages, you may want to use a more convenient time units. These functions make that easy.

| Static Function | Description |
| --------------- | ----------- |
| `OffsetTimestampByTicks(timestampValue, offsetTicks)` | Offsets a given timestamp by the provided (signed) number of ticks |
| `OffsetTimestampByMicroseconds(timestampValue, offsetMicroseconds)` | Offsets a given timestamp by the provided (signed) number of microseconds |
| `OffsetTimestampByMilliseconds(timestampValue, offsetMilliseconds)` | Offsets a given timestamp by the provided (signed) number of milliseconds |
| `OffsetTimestampBySeconds(timestampValue, offsetSeconds)` | Offsets a given timestamp by the provided (signed) number of seconds |

## IDL

[MidiClock IDL](https://github.com/microsoft/MIDI/blob/main/src/app-sdk/winrt-core/MidiClock.idl)
