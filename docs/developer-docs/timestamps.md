# MIDI Clock and Timestamps

## The MidiClock Type

The Windows::Devices::Midi2::MidiClock type provides access to the system-wide MIDI Timestamp system. Timestamps are high-resultion tick counts which can be converted to offsets in seconds or fractions of seconds, but do not represent a real-world time of day without additional tracking with an external reference clock (`GetSystemTimePreciseAsFileTime`, for example).


## High-resolution timestamps

Windows MIDI Services currently uses the high-resolution 64 bit QueryPerformanceCounter type. 

On most systems, the resolution is 100ns per tick, and so takes around 30,000 years to roll over. You do not need to worry about it wrapping around to zero, instead you can rely on the ticks to increase.

You can learn more about high-resolution timestamps in Windows at [https://aka.ms/miditimestamp](https://aka.ms/miditimestamp).

It's unlikely that we will change from QueryPerformanceCounter to another mechanism in the future. But the contract with applications is the MidiClock type. If you want to ensure your applications continue to work across revisions, always use `MidiClock::Now()` to acquire a timestamp, and use the other `MidiClock` methods for calculating ticks per second
