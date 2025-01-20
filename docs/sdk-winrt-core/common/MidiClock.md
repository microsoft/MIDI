---
layout: api_page
title: MidiClock
parent: Midi2 core
has_children: false
---

# MidiClock

The MidiClock is what is used for all timestamps in Windows MIDI Services. Although it is internally backed by `QueryPerformanceCounter`, we recommend using the MidiClock type directly instead of calling QPC yourself.

Also note that `QueryPerformanceCounter` technically returns a signed 64 bit integer, but the timestamp values used in Windows MIDI Services are unsigned 64 bit integers. Typically, this is of no practical concern as the tick resolution is currently 100ns and takes tens of thousands of years to wrap around even with a 64 bit signed integer.

> Note: The MIDI Clock is unrelated to wall clock time. It is an ever-increasing value of period 1/`TimestampFrequency` seconds that starts over when the PC is rebooted. To convert to wall clock time, you need to get the `MidiClock.Now` value at a known time, and then use that as a baseline until the next time you reboot the PC.

You can learn more about high-resolution timestamps in Windows at [https://aka.ms/miditimestamp](https://aka.ms/miditimestamp).

## Location

| Namespace | Microsoft.Windows.Devices.Midi2 |
| Library | Microsoft.Windows.Devices.Midi2 |

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
| `ConvertTimestampTicksToNanoseconds(timestampValue)` | Converts the provided timestamp to nanoseconds |
| `ConvertTimestampTicksToMicroseconds(timestampValue)` | Converts the provided timestamp to microseconds |
| `ConvertTimestampTicksToMilliseconds(timestampValue)` | Converts the provided timestamp to milliseconds |
| `ConvertTimestampTicksToSeconds(timestampValue)` | Converts the provided timestamp to seconds |

## Static Functions for Offset

When scheduling messages, you may want to use a more convenient time units. These functions make that easy.

| Static Function | Description |
| --------------- | ----------- |
| `OffsetTimestampByTicks(timestampValue, offsetTicks)` | Offsets a given timestamp by the provided (signed) number of ticks |
| `OffsetTimestampByMicroseconds(timestampValue, offsetMicroseconds)` | Offsets a given timestamp by the provided (signed) number of microseconds |
| `OffsetTimestampByMilliseconds(timestampValue, offsetMilliseconds)` | Offsets a given timestamp by the provided (signed) number of milliseconds |
| `OffsetTimestampBySeconds(timestampValue, offsetSeconds)` | Offsets a given timestamp by the provided (signed) number of seconds |

## Static Functions for Windows Timer Frequency

Windows supports putting the system timer into a low-latency / high-frequency mode. Drivers have more control over this, but applications can also specify that they want to enter a low-latency period, providing better timing characteristics. If you call the `BeginLowLatencyTimerPeriod` function, your application **must** call the `EndLowLatencyTimePeriod` before it closes. If drivers have not already set the timer period to the lowest value, these functions typically change the timer period from around 15ms to around 1ms.

| Static Function | Description |
| --------------- | ----------- |
| `GetCurrentSystemTimerInfo` | Returns a `MidiSystemTimerSettings` struct containing the current timer characteristics |
| `BeginLowLatencySystemTimerPeriod` | Signal that this application is putting the system into a low-latency timer period. Internally calls `timeBeginPeriod` |
| `EndLowLatencySystemTimerPeriod` | End the started low-latency timer period. You must call this before the application closes if you previously called the Begin function. Internally calls `timeEndPeriod` |

## IDL

[MidiClock IDL](https://github.com/microsoft/MIDI/blob/main/src/app-sdk/winrt/MidiClock.idl)
