---
layout: kb
title: Moving from DirectMusic to Windows MIDI Services
audience: developers
description: Concepts, timestamps and API mapping for applications that send and receive MIDI with DirectMusic
categories:
  - Developer Guidance
---

This article is for developers of applications that use the DirectMusic core interfaces (`IDirectMusic8`, `IDirectMusicPort8` and `IDirectMusicBuffer8`) to send and receive MIDI, and want to move to the Windows MIDI Services API, `Windows.Devices.Midi2`.

> **DirectMusic device drivers don't work with Windows MIDI Services.** This article is about applications, not drivers. A device that only has a DirectMusic driver can't be used through Windows MIDI Services at all. Customers who still need one can switch the PC to [Legacy API mode]({{ site.baseurl }}/kb/how-to-change-api-mode/), which puts back the MIDI stack Windows had before. That costs them multi-client access, the built-in loopbacks, Network MIDI 2.0, Bluetooth LE MIDI, and USB MIDI 2.0 devices.

Windows MIDI Services [doesn't provide backwards compatibility for DirectMusic]({{ site.baseurl }}/kb/api-back-compat/), and DirectMusic isn't part of our testing. If your application uses DirectMusic for MIDI today, moving to the new API is the way to keep it working well, and to give your customers multi-client access, MIDI 2.0 devices and everything else the new stack adds.

Read [Moving from WinMM to Windows MIDI Services]({{ site.baseurl }}/kb/moving-from-winmm-to-wms/) as well. Most of what it says about threading, initialization, finding devices, names and identifiers applies to you too. This article covers what's different when you're coming from DirectMusic, and spends most of its time on timestamps, because that's where DirectMusic and Windows MIDI Services are both most alike and easiest to get wrong.

## What this article doesn't cover

The DirectMusic *performance* layer (`IDirectMusicPerformance8`, segments, styles, bands, the composer and audio scripts) has no equivalent in Windows MIDI Services. Windows MIDI Services is a MIDI API, not a music playback engine. If you used the performance layer only to play Standard MIDI Files, look at the [`MidiSequencePlayer`]({{ site.baseurl }}/sdk-reference/Utilities/Sequencing/MidiSequencePlayer/) and [`MidiStandardFileReader`]({{ site.baseurl }}/sdk-reference/Utilities/Files/MidiStandardFileReader/) types. For anything more, keep that logic in your own code and use Windows MIDI Services to send the messages it produces.

The Microsoft Software Synthesizer and DLS instrument downloads are also out of scope. See [Synthesizers and DLS](#synthesizers-and-dls) below.

## Quick reference: what you used to call, and what to call now

| Task | DirectMusic | Windows MIDI Services | Reference |
| ---- | ----------- | --------------------- | --------- |
| Start up | `CoCreateInstance(CLSID_DirectMusic)`, then `SetDirectSound` | `winrt::init_apartment()`, check the API is there, call `MidiApi.EnsureServiceAvailable()`, then `MidiSession.Create` | [MidiApi]({{ site.baseurl }}/sdk-reference/MidiApi/), [MidiSession]({{ site.baseurl }}/sdk-reference/MidiSession/) |
| Get a list of devices | Call `EnumPort` with an index that goes up until it fails, reading `DMUS_PORTCAPS` for each | `MidiEndpointDeviceInformation.FindAll` | [MidiEndpointDeviceInformation]({{ site.baseurl }}/sdk-reference/Enumeration/MidiEndpointDeviceInformation/) |
| React to devices arriving and leaving | No notification. You enumerated again | `MidiEndpointDeviceWatcher`, with `Added`, `Removed` and `Updated` events | [MidiEndpointDeviceWatcher]({{ site.baseurl }}/sdk-reference/Enumeration/MidiEndpointDeviceWatcher/) |
| Identify a device | `DMUS_PORTCAPS.guidPort` | `MidiEndpointDeviceInformation.EndpointDeviceId`, a string | [Identifiers]({{ site.baseurl }}/kb/identifiers/) |
| Tell input from output | `DMUS_PORTCAPS.dwClass` is `DMUS_PC_INPUTCLASS` or `DMUS_PC_OUTPUTCLASS`. A port goes one way | An endpoint goes both ways. Its group terminal blocks or function blocks say which groups go which way | [MidiGroupTerminalBlock]({{ site.baseurl }}/sdk-reference/Enumeration/MidiGroupTerminalBlock/), [MidiFunctionBlock]({{ site.baseurl }}/sdk-reference/Enumeration/MidiFunctionBlock/) |
| Address more than 16 channels | Channel groups, set with `SetNumChannelGroups` and passed to `PackStructured` | Groups. Every UMP carries a group number from 0 to 15 | [MidiGroup]({{ site.baseurl }}/sdk-reference/MidiGroup/) |
| Open a port | `CreatePort` with a `DMUS_PORTPARAMS8`, then `Activate(TRUE)` | `MidiSession.CreateEndpointConnection`, attach your handlers, then `Open` | [MidiEndpointConnection]({{ site.baseurl }}/sdk-reference/MidiEndpointConnection/) |
| Read the clock | `IDirectMusic8::GetMasterClock`, then `IReferenceClock::GetTime` | `MidiClock.Now` | [MidiClock]({{ site.baseurl }}/sdk-reference/MidiClock/) |
| Send a message right away | Stamp it with the time from the port's latency clock | Pass a timestamp of `0`, or `MidiClock.TimestampConstantSendImmediately` | [MidiClock]({{ site.baseurl }}/sdk-reference/MidiClock/) |
| Send a message at a future time | `PackStructured(rt, ...)` with a future `rt`, then `PlayBuffer` | Pass the future timestamp to any send function | [MidiEndpointConnection]({{ site.baseurl }}/sdk-reference/MidiEndpointConnection/) |
| Send a channel message | `CreateMusicBuffer`, `PackStructured`, `PlayBuffer` | `SendSingleMessageWords`, built with `MidiMessageBuilder.BuildMidi1ChannelVoiceMessage` if you like | [MidiMessageBuilder]({{ site.baseurl }}/sdk-reference/Utilities/Messages/MidiMessageBuilder/) |
| Send SysEx | `PackUnstructured`, then `PlayBuffer` | `MidiMessageConverter.ConvertMidi1CompleteMessageBytesToUmpWords`, then a multiple-message send. For a whole file, `MidiSystemExclusiveSender` | [MidiMessageConverter]({{ site.baseurl }}/sdk-reference/Utilities/Messages/MidiMessageConverter/), [MidiSystemExclusiveSender]({{ site.baseurl }}/sdk-reference/Utilities/SysExTransfer/MidiSystemExclusiveSender/) |
| Send a buffer of messages with no allocation | Reuse an `IDirectMusicBuffer8` | `IMidiEndpointConnectionRaw::SendMidiMessagesRaw` from the COM extensions | [COM Extensions]({{ site.baseurl }}/sdk-reference/MidiEndpointConnection_COM-Extensions/) |
| Receive messages | `SetReadNotificationHandle`, wait on the event, then `Read` and `GetNextEvent` in a loop | The COM extensions callback, which hands you one or more messages per call, or the `MessageReceived` event, one message per call | [COM Extensions]({{ site.baseurl }}/sdk-reference/MidiEndpointConnection_COM-Extensions/), [MidiMessageReceivedEventArgs]({{ site.baseurl }}/sdk-reference/MidiMessageReceivedEventArgs/) |
| Receive from one group or channel only | Check `dwChannelGroup` and the status byte yourself | `MidiGroupEndpointListener` or `MidiChannelEndpointListener` | [Client plugins]({{ site.baseurl }}/sdk-reference/ClientPlugins/) |
| Pass input straight to an output | `IDirectMusicThru8::ThruChannel` | Nothing built in. Resend from your receive handler, or let the customer set up a route in [Windows MIDI Patchbay]({{ site.baseurl }}/tools/midipatchbay/) | |
| Allow for output delay | Read the port's latency clock with `GetLatencyClock` | The service's scheduler sends early by the endpoint's outgoing latency compensation, so you don't have to | [MidiEndpointUserSuppliedInfo]({{ site.baseurl }}/sdk-reference/Enumeration/MidiEndpointUserSuppliedInfo/) |
| Close a port | `Activate(FALSE)`, then `Release` | `MidiSession.DisconnectEndpointConnection`, or close the session | [MidiSession]({{ site.baseurl }}/sdk-reference/MidiSession/) |

## Before you start

- **Windows MIDI Services is for 64-bit applications.** If your DirectMusic application is still 32-bit, moving it to 64-bit is part of this work. See [Application Backwards Compatibility]({{ site.baseurl }}/kb/api-back-compat/).
- **Check that the API is there before you use it.** The PC needs Windows 11 25H2 or later, the `Windows.Devices.Midi2.MidiApi` type has to resolve, and `MidiApi.EnsureServiceAvailable()` has to return true. The customer may have chosen [Legacy API mode]({{ site.baseurl }}/kb/how-to-change-api-mode/). The [detect-midi-services](https://github.com/microsoft/MIDI/tree/main/samples/cpp-winrt/detect-midi-services) sample shows the whole check. If the answer is no, fall back to WinMM, not DirectMusic.
- **Don't do MIDI work on the UI thread.** Starting the service can take several seconds, and the API is synchronous on purpose. Initialize WinRT in a multi-threaded apartment on a worker thread. The [threading section of the WinMM article]({{ site.baseurl }}/kb/moving-from-winmm-to-wms/#a-note-on-threading) has the details.

## Ports, channel groups, endpoints and groups

DirectMusic is the one older Windows MIDI API that already put a group number on every event. When you called `PackStructured(rt, dwChannelGroup, dwChannelMessage)`, the channel group told the port which set of 16 channels the message was for. That idea carries straight over to Windows MIDI Services, where every Universal MIDI Packet (UMP) carries a group number in its first word, next to the message type.

The details are different, though:

| | DirectMusic | Windows MIDI Services |
| --- | --- | --- |
| What you open | A port, which goes one way. Input and output on the same device are two ports | An endpoint, which goes both ways. You open one connection for both directions |
| What a group is | A channel group: a set of 16 channels on that port. Mostly used by software synthesizers to get more than 16 channels | A group: one of 16 numbered streams inside the endpoint. For a MIDI 1.0 USB device, each group is one of the device's ports, which DirectMusic and WinMM showed as separate ports |
| Where the group number lives | A separate argument, next to the message | Inside the message, in the first word of the UMP |
| How many | Whatever the port reports in `dwMaxChannelGroups` | At most 16 per endpoint |
| How you find out which ones exist | `GetNumChannelGroups` | The endpoint's group terminal blocks (USB MIDI 1.0 devices) or function blocks (MIDI 2.0 devices), which also say which way each group goes |

So when you move your code:

- **Don't assume DirectMusic channel group *n* is group *n* on the endpoint.** Look at the group terminal blocks or function blocks, and let the customer pick. A device that showed up as four DirectMusic ports may now be one endpoint with four groups.
- **Groups are numbered 0 to 15 inside the message.** People and the apps' screens count them from 1 to 16. Check which way your DirectMusic code counted channel groups before you convert the number.
- **Channel numbers are still 0 to 15 in the message**, just as in the MIDI 1.0 status byte.

The [WinMM article]({{ site.baseurl }}/kb/moving-from-winmm-to-wms/#free-your-mind-from-midi-10-constraints) explains the hierarchy of parent device, endpoint, block, group and channel in more depth, and why USB MIDI 1.0 cables became groups.

## Timestamps

DirectMusic and Windows MIDI Services both use **absolute** timestamps. A timestamp is a point on a clock, not a delay from now. If your DirectMusic code already scheduled messages ahead of time, the way you think about time carries over. What changes is which clock you read, what its ticks mean, and who does the waiting.

| | DirectMusic | Windows MIDI Services |
| --- | --- | --- |
| Type | `REFERENCE_TIME`, a signed 64-bit number | `UInt64`, an unsigned 64-bit number |
| Tick length | Always 100 nanoseconds | `1 / MidiClock.TimestampFrequency` seconds. That's currently 100 nanoseconds, but read it rather than assume it |
| Which clock | The DirectMusic master clock, an `IReferenceClock`. DirectMusic picked the system clock, but an application could choose another, such as a sound card's clock | `MidiClock`, which reads the same counter as `QueryPerformanceCounter`. It starts at zero when the PC starts. You can't choose another clock |
| Sending right away | Stamp the message with the time from the port's latency clock | Use `0`. `MidiClock.TimestampConstantSendImmediately` is the same value. It skips the scheduler |
| How far ahead | A buffer usually covered a short time, typically under 200 ms | Up to `MidiClock.TimestampConstantMessageQueueMaximumFutureTicks`, currently five minutes. Keep it much shorter than that in practice |
| Who waits | The port and its driver | The MIDI service's scheduler |
| Output delay | You read the port's latency clock and stamped messages no earlier than that | The scheduler sends each message early by the endpoint's outgoing latency compensation |
| Incoming messages | `GetNextEvent` gives you the time on the master clock | `Timestamp` is the `MidiClock` time when the service received the message |
| Same timestamp, more than one message | No promise of order within a buffer | Send them in one call, in the order you want them sent |

### Change the clock, not just the numbers

**Never pass a `REFERENCE_TIME` from the DirectMusic master clock to a Windows MIDI Services send function.** Both are 64-bit numbers, and today both have 100-nanosecond ticks, so the code compiles and may even seem to work. But they're readings from different clocks with different starting points. A message stamped with a master clock time can land far in the future, so it fails to send because it's past the five-minute limit, or land in the past.

Replace every read of the master clock (`GetMasterClock`, then `IReferenceClock::GetTime`) with `MidiClock.Now`. Then the times you read and the times you send are on the same clock.

If you have a good reason to keep another clock, for example because your audio engine runs on it, work out the gap between the two clocks:

1. Read the other clock and `MidiClock.Now` one right after the other. Do it a few times and keep the pair that was read closest together.
2. Add that gap to every time you send.
3. Measure it again now and then, because two clocks that run from different hardware drift apart.

### Don't build times from raw ticks

DirectMusic code often adds a fixed number of 100-nanosecond units, like `rt + 10000` for "one millisecond later". Use the `MidiClock` helpers instead, so your code stays right if the tick length ever changes:

```cpp
// DirectMusic: one beat (500 ms at 120 BPM) after the start time
// REFERENCE_TIME rt = rtStart + 500 * 10000;

// Windows MIDI Services
auto start = MidiClock::Now();
auto timestamp = MidiClock::OffsetTimestampByMilliseconds(start, 500);
```

There are also `OffsetTimestampByTicks`, `OffsetTimestampByMicroseconds` and `OffsetTimestampBySeconds`, and functions to convert a tick count to nanoseconds, microseconds, milliseconds or seconds. For the fastest code, read `MidiClock::TimestampFrequency` once and do the math yourself.

Read `Now` once at the start of a run of messages and offset that one value for each message, instead of reading the clock again for each one. That keeps the spacing between messages exact.

### The latency clock goes away

In DirectMusic, each port had a latency clock. Its time was the earliest moment a message could still be played on time, and stamping a message with it meant "as soon as you can". To play a message at a musical time, you stamped it with that time and made sure that time wasn't earlier than the latency clock.

Windows MIDI Services doesn't have a latency clock. Instead:

- **To send right away, use a timestamp of `0`.** The message skips the scheduler and goes straight out.
- **To send at a time, stamp the message with that time on `MidiClock`.** The scheduler holds the message and sends it then.
- **The scheduler takes care of known output delay.** Each endpoint can have an outgoing latency compensation value. The transport works it out for some kinds of devices, such as Bluetooth and Network MIDI 2.0, and the customer can set their own. The scheduler sends each message that much early, so it reaches the device at the time you asked for. You can read the values in effect from `GetUserSuppliedInfo()` on the endpoint: `CalculatedMidiOutgoingLatencyTicks`, `CustomMidiOutgoingLatencyTicks`, and `UseCustomMidiOutgoingLatencyTicksForScheduling` to say which one is used. See [MidiEndpointUserSuppliedInfo]({{ site.baseurl }}/sdk-reference/Enumeration/MidiEndpointUserSuppliedInfo/).
- **To make up for a delay of your own**, use a negative offset, such as `MidiClock::OffsetTimestampByMilliseconds(timestamp, -3)`. A negative offset that would go below zero gives you zero, not a huge number.

### One timestamp per send call

An `IDirectMusicBuffer8` could hold many events, each with its own time, and `PlayBuffer` sent them all. In Windows MIDI Services, most send functions that take several messages also take **one** timestamp for all of them. That includes the `SendMultipleMessages...` functions and `SendMidiMessagesRaw` in the COM extensions.

So don't move a whole DirectMusic buffer into one call. Group the messages that share a time, and send each group with its own timestamp. `SendMultipleMessagesPacketList` is the exception: each `IMidiUniversalPacket` in the list carries its own timestamp. It's handy, but it creates an object for each message, so it isn't the fastest choice.

### Don't schedule too much at once

The scheduler is made for messages a short time ahead, not for a whole song. It slows down when it holds thousands of messages. Keep a short window of messages scheduled ahead of time, the way your DirectMusic code probably kept a short window of buffers queued, and top it up as time moves on. Messages scheduled for an endpoint are lost if the endpoint disconnects.

### Incoming timestamps

DirectMusic gave each incoming event the master clock time from `GetNextEvent`. In Windows MIDI Services, each message has the `MidiClock` time when the service received it: `Timestamp` on [MidiMessageReceivedEventArgs]({{ site.baseurl }}/sdk-reference/MidiMessageReceivedEventArgs/), or the `timestamp` argument to the COM extensions callback. It's on the same clock you send with, so you can line up input and output directly. It's not an offset from when you opened the connection.

`MidiClock` has nothing to do with the time of day, and it starts over when the PC restarts. Don't save timestamps to a file and expect them to mean anything after a restart.

## Sending and receiving messages

DirectMusic used two ways to put a message in a buffer. `PackStructured` took a channel message packed into a `DWORD`, and `PackUnstructured` took SysEx and other long messages as bytes. In Windows MIDI Services, every message is one or more 32-bit words in UMP format, and they're all sent the same way.

- **Channel messages:** `MidiMessageBuilder.BuildMidi1ChannelVoiceMessage(timestamp, group, status, channel, byte3, byte4)` builds a one-word MIDI 1.0 channel voice message. Or put the group and the three MIDI 1.0 bytes into the word yourself. The [UMP data format primer]({{ site.baseurl }}/kb/ump-data-format-primer/) shows the layout.
- **SysEx and other MIDI 1.0 bytes:** `MidiMessageConverter.ConvertMidi1CompleteMessageBytesToUmpWords` turns bytes into UMP words. Use the overload that takes a `MidiBytestreamToUmpMessageConverterState` when one SysEx message is split across several of your buffers.
- **How much fits in one call:** call `GetSupportedMaxMidiWordsPerTransmission` on the connection. Don't hard-code a number.

### Use the COM extensions if you already handle UMP

If your code already builds and reads UMP words, or you're happy to write that part, use the COM extensions on the connection. They're the fastest way to send and receive:

- **`IMidiEndpointConnectionRaw::SendMidiMessagesRaw`** sends a buffer of whole UMPs, with a timestamp, without creating any WinRT objects. That's the closest thing to filling and reusing an `IDirectMusicBuffer8`.
- **`IMidiEndpointConnectionMessagesReceivedCallback`** is called with a pointer to one or more incoming messages. **It's the only way to receive more than one message in a single call.** The `MessageReceived` event always hands you one message at a time. If your DirectMusic code drained a port with `Read` and `GetNextEvent` in a loop, this callback is the nearest match.

Register the callback with `SetMessagesReceivedCallback` *before* you call `Open()`. While it's registered, the connection raises no `MessageReceived` events and runs no message listeners. That's why you can't use it together with `MidiGroupEndpointListener`, `MidiChannelEndpointListener` or a virtual device. Remove it with `RemoveMessagesReceivedCallback` before you close the connection.

The COM extensions are for C++ and other languages that can call COM. From C# and other languages, use the WinRT send functions and the `MessageReceived` event. See [COM Extensions]({{ site.baseurl }}/sdk-reference/MidiEndpointConnection_COM-Extensions/) and the [com-extensions](https://github.com/microsoft/MIDI/tree/main/samples/cpp-winrt/com-extensions) and [scheduled-messages-com-extensions](https://github.com/microsoft/MIDI/tree/main/samples/cpp-winrt/scheduled-messages-com-extensions) samples.

### Receiving

With DirectMusic, you waited on an event handle and then pulled data out with `Read`. Windows MIDI Services calls you instead, on a thread it owns. So:

- **Be ready before you open.** Attach your `MessageReceived` handler, your listeners or your COM callback, and only then call `Open()`. Messages can arrive right away.
- **Return quickly.** Copy what you need into your own queue and do the real work on another thread. Don't hold on to the event arguments or the callback's pointer after you return, because the data they point to goes away.
- **There's no separate thru.** `IDirectMusicThru8::ThruChannel` had the port pass input to an output for you. To do that now, resend each message from your receive handler with a timestamp of `0`. If it's a route the customer wants all the time, rather than part of your app, point them to [Windows MIDI Patchbay]({{ site.baseurl }}/tools/midipatchbay/).

## Finding devices

`IDirectMusic8::EnumPort` gave you a flat list of ports, each with a GUID, a direction, a class and a description. In Windows MIDI Services:

- Use `MidiEndpointDeviceWatcher` to keep a list up to date. It tells you when endpoints arrive, leave, or change. Changes happen much more often than they did with DirectMusic, because MIDI 2.0 devices describe themselves after they connect, and customers can rename endpoints. Use `MidiEndpointDeviceInformation.FindAll` only for a one-time list.
- Save the `EndpointDeviceId` to find the same device next time, not the name. Names can change at any time. See [Windows MIDI Services identifiers]({{ site.baseurl }}/kb/identifiers/).
- For a USB device's vendor and product ids, use `GetParentDeviceInformation()`. To tell what kind of device an endpoint is, use `GetTransportSuppliedInfo()`.
- The software synthesizer, the "[Emulated]" ports that DirectMusic built on top of WinMM, and the kernel-mode DirectMusic ports all go away. You see one list of endpoints, whatever transport they come from: USB, Bluetooth, network, loopback, or another application.

## Synthesizers and DLS

DirectMusic's default output was the Microsoft Software Synthesizer, and applications could download DLS instruments to it. Windows MIDI Services doesn't support DLS, and has no way to download instruments.

Windows MIDI Services has a built-in General MIDI synthesizer, available now as a preview for developers. It shows up as a normal endpoint. `MidiSynthManager.EndpointDeviceId` gives you its id, and you play it like any other endpoint. See [WinRT API Support for the General MIDI Synthesizer]({{ site.baseurl }}/sdk-reference/Transports/Synth/). If your application depends on custom DLS sounds, it needs its own synthesizer.

## Samples

All the samples are at [https://aka.ms/midisamples](https://aka.ms/midisamples). The ones that matter most when you're coming from DirectMusic:

- [basics](https://github.com/microsoft/MIDI/tree/main/samples/cpp-winrt/basics): start up, open a connection, send and receive
- [scheduled-send-messages](https://github.com/microsoft/MIDI/tree/main/samples/cpp-winrt/scheduled-send-messages): send messages at future times
- [com-extensions](https://github.com/microsoft/MIDI/tree/main/samples/cpp-winrt/com-extensions) and [scheduled-messages-com-extensions](https://github.com/microsoft/MIDI/tree/main/samples/cpp-winrt/scheduled-messages-com-extensions): send and receive with no per-message objects
- [watch-endpoints](https://github.com/microsoft/MIDI/tree/main/samples/cpp-winrt/watch-endpoints): keep a list of endpoints up to date
- [endpoint-listeners](https://github.com/microsoft/MIDI/tree/main/samples/cpp-winrt/endpoint-listeners): receive from one group or channel
- [detect-midi-services](https://github.com/microsoft/MIDI/tree/main/samples/cpp-winrt/detect-midi-services): decide at startup whether the new API is there
