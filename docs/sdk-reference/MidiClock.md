---
layout: sdk_reference_page
title: MidiClock
namespace: Windows.Devices.Midi2
type: runtimeclass
description: Used for all timestamps in Windows MIDI Services.
---

Every timestamp in Windows MIDI Services comes from `MidiClock`. It reads the same counter as `QueryPerformanceCounter`, but we recommend using `MidiClock` instead of calling `QueryPerformanceCounter` yourself, so your timestamps always match the API's.

`QueryPerformanceCounter` returns a signed 64-bit number, but Windows MIDI Services timestamps are unsigned 64-bit numbers. That difference doesn't matter in practice. Each tick is currently 100 nanoseconds, and even a signed 64-bit counter would take tens of thousands of years to run out.

> **Note:** The MIDI clock has nothing to do with the time of day. It's a count that goes up by one every `1/TimestampFrequency` seconds, and it starts over when the PC restarts. To turn a timestamp into a time of day, read `MidiClock.Now` at a known time of day, and measure from there until the PC restarts.

To learn more about high-resolution timestamps in Windows, see [https://aka.ms/miditimestamp](https://aka.ms/miditimestamp).

## Static Properties

| Static Property | Description |
| --------------- | ----------- |
| `Now` | Returns the current timestamp |
| `TimestampFrequency` | The number of timestamp ticks in one second. It's worked out on the first call and then remembered |
| `TimestampConstantSendImmediately` | The timestamp that means "send this right away, don't schedule it." Its value is `0`, so you can also pass `0` |
| `TimestampConstantMessageQueueMaximumFutureTicks` | How far ahead you can schedule a message, in ticks. It's currently five minutes. A message scheduled further ahead than this fails to send |

## Static Functions for Conversion

These functions convert a timestamp from ticks to other units.

| Static Function | Description |
| --------------- | ----------- |
| `ConvertTimestampTicksToNanoseconds(timestampValue)` | Converts the timestamp to nanoseconds |
| `ConvertTimestampTicksToMicroseconds(timestampValue)` | Converts the timestamp to microseconds |
| `ConvertTimestampTicksToMilliseconds(timestampValue)` | Converts the timestamp to milliseconds |
| `ConvertTimestampTicksToSeconds(timestampValue)` | Converts the timestamp to seconds |

## Static Functions for Offset

When you schedule messages, it's usually easier to think in milliseconds or seconds than in ticks. These functions add an amount of time, in the unit you choose, to a timestamp.

| Static Function | Description |
| --------------- | ----------- |
| `OffsetTimestampByTicks(timestampValue, offsetTicks)` | Adds a number of ticks, which can be negative, to the timestamp |
| `OffsetTimestampByMicroseconds(timestampValue, offsetMicroseconds)` | Adds a number of microseconds, which can be negative, to the timestamp |
| `OffsetTimestampByMilliseconds(timestampValue, offsetMilliseconds)` | Adds a number of milliseconds, which can be negative, to the timestamp |
| `OffsetTimestampBySeconds(timestampValue, offsetSeconds)` | Adds a number of seconds, which can be negative, to the timestamp |

A negative offset moves the timestamp earlier, which is how you make up for a known output delay. If the offset would take the timestamp below zero, you get zero back, not a huge number.

## Static Functions for Windows Timer Frequency

Windows can run its system timer faster, which makes waits and timeouts more precise. Drivers have the most control over this, but an application can ask for a faster timer too. If drivers haven't already sped it up, these functions usually change the timer period from about 15 ms to about 1 ms. If your application calls `BeginLowLatencySystemTimerPeriod`, it **must** call `EndLowLatencySystemTimerPeriod` before it closes.

| Static Function | Description |
| --------------- | ----------- |
| `GetCurrentSystemTimerInfo()` | Returns a `MidiSystemTimerSettings` with the timer's current period, and the shortest and longest periods it supports |
| `BeginLowLatencySystemTimerPeriod()` | Asks for a low-latency timer period for **this process**. It calls `timeBeginPeriod` |
| `EndLowLatencySystemTimerPeriod()` | Ends the low-latency timer period. If you called Begin, call this before your application closes. It calls `timeEndPeriod` |

### Each part of your application can ask separately

Windows counts `timeBeginPeriod` and `timeEndPeriod` calls, so separate parts of one process can each ask for a low-latency period without checking with each other. These functions count calls the same way.

This matters because the WinRT API is often loaded into a host application that also loads plugins. If the host asks for a low-latency period and a plugin asks too, both get `true`, and the period ends only after both have called End. If you get `true`, you're in a low-latency period, whether or not your call is the one that started it.

`BeginLowLatencySystemTimerPeriod` returns `false` only when the request failed. `EndLowLatencySystemTimerPeriod` returns `false` when there was nothing left to end, which usually means End was called more times than Begin.

### The benefit stays in your process, but the cost doesn't

This part surprises people. It changed in Windows 10 version 2004, and many people still expect the old behavior.

`timeBeginPeriod` still speeds up the timer for the **whole PC**, so the whole PC pays the power cost. What changed is that the **benefit** no longer reaches other processes. A process that hasn't called `timeBeginPeriod` itself still gets the normal, slower `Sleep` and wait timing, even while another process is holding the timer at the faster rate.

This means two things for MIDI applications:

- **You can't speed up the timer for another process, and no other process can speed it up for yours.** If your application needs precise waits, it has to ask for them itself. An application made of several processes needs to call this in each process that does timing work, not only in the main one.
- **Calling this doesn't make the MIDI service send scheduled messages more precisely.** Scheduling happens in the service, not in your process. This function only changes your own waits and timeouts.

Bruce Dawson's [Windows Timer Resolution: The Great Rule Change](https://randomascii.wordpress.com/2020/10/04/windows-timer-resolution-the-great-rule-change/) is the clearest explanation of the change and how it was measured.

### Don't use a count of `Sleep` calls as a timeout

A common bug: `Sleep(1)` doesn't sleep for one millisecond. It sleeps until the next timer tick that's at least one millisecond away, and by default that can be about 15.6 ms. So a retry loop that stops after a set number of tries runs far longer than you'd expect. A loop of 5,000 `Sleep(1)` calls can take more than a minute instead of five seconds.

End a retry loop at a deadline you read from a clock, not after a number of tries.

## Samples

Use the `OffsetTimestampBy...` functions to schedule a message for later. Read `Now` once and offset that one value for each message, instead of reading the clock again for every message. WinMM had no way to schedule messages like this.

* [C++/WinRT scheduled-send-messages](https://github.com/microsoft/MIDI/tree/main/samples/cpp-winrt/scheduled-send-messages)
* [C# scheduled-send-messages](https://github.com/microsoft/MIDI/tree/main/samples/csharp-net/scheduled-send-messages)
* [C++/WinRT scheduled-messages-com-extensions](https://github.com/microsoft/MIDI/tree/main/samples/cpp-winrt/scheduled-messages-com-extensions)
