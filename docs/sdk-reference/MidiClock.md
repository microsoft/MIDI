---
layout: sdk_reference_page
title: MidiClock
namespace: Windows.Devices.Midi2
type: runtimeclass
description: Used for all timestamps in Windows MIDI Services.
---

The `MidiClock` is what is used for all timestamps in Windows MIDI Services. Although it is internally backed by `QueryPerformanceCounter`, we recommend using the MidiClock type directly instead of calling QPC yourself.

Also note that `QueryPerformanceCounter` technically returns a signed 64 bit integer, but the timestamp values used in Windows MIDI Services are unsigned 64 bit integers. Typically, this is of no practical concern as the tick resolution is currently 100ns and takes tens of thousands of years to wrap around even with a 64 bit signed integer.

> Note: The MIDI Clock is unrelated to wall clock time. It is an ever-increasing value of period `1/TimestampFrequency` seconds that starts over when the PC is rebooted. To convert to wall clock time, you need to get the `MidiClock.Now` value at a known time, and then use that as a baseline until the next time you reboot the PC.

You can learn more about high-resolution timestamps in Windows at [https://aka.ms/miditimestamp](https://aka.ms/miditimestamp).

## Static Properties

| Static Property | Description |
| --------------- | ----------- |
| `Now` | Returns the current timestamp |
| `TimestampFrequency` | Returns the number of timestamp ticks per second. This is calculated the first time it is called, and then cached for future calls. |
| `TimestampConstantSendImmediately` | Returns the constant to use when you want to send messages immediately and bypass outgoing message scheduling. Developers may use this value or simply provide `0` in place of the timestamp when sending messages.  |
| `TimestampConstantMessageQueueMaximumFutureTicks` | Returns the maximum future timestamp value that the scheduler will accept. Messages scheduled too far in the future will fail to send. |

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

A negative offset moves the timestamp earlier, which is how you compensate for a known output
latency. An offset which would take the timestamp below zero returns zero rather than wrapping
around to a very large value.

## Static Functions for Windows Timer Frequency

Windows supports putting the system timer into a low-latency / high-frequency mode. Drivers have more control over this, but applications can also specify that they want to enter a low-latency period, providing better timing characteristics. If you call the `BeginLowLatencyTimerPeriod` function, your application **must** call the `EndLowLatencyTimePeriod` before it closes. If drivers have not already set the timer period to the lowest value, these functions typically change the timer period from around 15ms to around 1ms.

| Static Function | Description |
| --------------- | ----------- |
| `GetCurrentSystemTimerInfo` | Returns a `MidiSystemTimerSettings` struct containing the current timer characteristics |
| `BeginLowLatencySystemTimerPeriod` | Request a low-latency timer period for **this process**. Internally calls `timeBeginPeriod` |
| `EndLowLatencySystemTimerPeriod` | End the started low-latency timer period. You must call this before the application closes if you previously called the Begin function. Internally calls `timeEndPeriod` |

### These calls are counted, so independent components can each ask

`timeBeginPeriod` and `timeEndPeriod` are reference counted by Windows precisely so that independent
components in one process can each ask for a low-latency period without coordinating. These
functions follow that model.

This matters because the SDK is frequently loaded into a host which also loads plugins. If a host
asks for a low-latency period and a plugin asks as well, both receive `true`, and the period is only
released once both have called the End function. A caller which is told `true` can rely on being in
a low-latency period regardless of whether it was the one that started it.

`BeginLowLatencySystemTimerPeriod` returns `false` only when the request actually failed.
`EndLowLatencySystemTimerPeriod` returns `false` when there was no outstanding request to release,
which usually means it has been called more times than Begin was.

### The benefit is per-process, even though the cost is not

This is the part which catches people out, and it is worth being precise about because it changed in Windows 10 2004 and the older behavior is still widely assumed.

`timeBeginPeriod` does still raise the **global** timer interrupt rate, so the power cost of asking for a low-latency period is paid by the whole machine. What changed is that the scheduling **benefit** no longer spreads to other processes. A process which has not called `timeBeginPeriod` itself now sees roughly the default `Sleep` and wait granularity even while another process is holding the interrupt rate high.

Two consequences for MIDI applications:

- **You cannot raise the timer period on someone else's behalf, and nobody can raise it on yours.** If your application needs fine-grained waits, it has to ask for them itself. Multi-process applications need to call this in each process which does timing-sensitive work, not just in a main or controller process.
- **Calling this does not make the MIDI service schedule outgoing messages more precisely.** Message scheduling happens in the service, not in your process. This function affects your own waits and timeouts.

Bruce Dawson's [Windows Timer Resolution: The Great Rule Change](https://randomascii.wordpress.com/2020/10/04/windows-timer-resolution-the-great-rule-change/) is the clearest write-up of the behavior and how it was measured.

### Do not use a `Sleep` count as a timeout

Related, and a common bug: `Sleep(1)` does not sleep for one millisecond. It sleeps until the next timer interrupt at or after one millisecond, which by default is up to about 15.6ms away. A retry loop bounded by an iteration count therefore runs far longer than intended — a "5000 iteration" loop of `Sleep(1)` can easily take over a minute rather than five seconds.

Bound retry loops by a deadline you read from a clock, not by counting iterations.

## Samples

The `OffsetTimestampBy...` functions are how you schedule a message for the future. Read `Now` once
and offset that single value, rather than reading the clock again for each message. There was no
WinMM equivalent for scheduled sending.

* [C++/WinRT scheduled-send-messages](https://github.com/microsoft/MIDI/tree/main/samples/cpp-winrt/scheduled-send-messages)
* [C# scheduled-send-messages](https://github.com/microsoft/MIDI/tree/main/samples/csharp-net/scheduled-send-messages)
* [C++/WinRT scheduled-messages-com-extensions](https://github.com/microsoft/MIDI/tree/main/samples/cpp-winrt/scheduled-messages-com-extensions)
