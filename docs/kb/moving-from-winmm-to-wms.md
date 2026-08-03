---
layout: kb
title: Moving from WinMM to Windows MIDI Services
audience: developers
description: Concepts and mapping from the primary MIDI 1 API to Windows MIDI Services
---

# Moving from WinMM to Windows MIDI Services

This article is targeted to developers looking to move from WinMM to Windows MIDI Services APIs.

## Free your mind from MIDI 1.0 constraints

In WinMM, ports are a free-floating entity which have no obvious association with a device. In reality, for USB, they each represent a virtual cable in one of two directions within a MIDI stream from that device. The missing context of the actual device makes WinMM ports slightly more confusing to customers, as they do not always know what the port actually represents. As a result, manufacturers try to stuff as much information as possible within the 31 characters available, but it's still a compromise, and leads to confusion. Many DAWs even include name customization / mapping inside the DAW to get around this.

For Windows MIDI Services, what used to be a Port in WinMM is a single group in a single direction on an endpoint.

Here's the hierarchy in order from broad to specific

| Term | Description |
| ---- | ----------- |
| Parent Device | The device presented by the driver or transport. For USB hardware, this is usually the USB device itself. |
| Endpoint | The stream on the device. For a MIDI 1.0 device, we create a UMP endpoint to aggregate all the directions in this single stream. |
| Group Terminal Block | For MIDI 1.0 devices, we create these to be 1:1 with the group. For MIDI 2.0 devices, these have been superceded by Function Blocks, but in both cases, can contain multiple groups. For MIDI 1.0 devices, you can think of these as your Port |
| Group | For a MIDI 1.0 device, this is your port's address. It's logically equivalent to the virtual cable number on the device. |
| Channel | The address within the message. |

## How this came about

In the driver (and USB MIDI 1.0 specification), USB MIDI 1.0 sends and receive normal MIDI messages using 32 bit packets. There is one stream to/from the USB endpoint.

| Field | Size (bits) | Description |
| ----- | ---- | ----------- |
| Cable Number  | 4 | This is effectively the "port" in the stream |
| Code Index Number | 4 | This indicates the type of message, like Note on or Off, SysEx start/continue/end |
| Byte 1 | 8 | Data or status byte |
| Byte 2 | 8 (but 7 usable) | Data byte |
| Byte 3 | 8 (but 7 usable) | Data byte |

In the UMP data format, we have the equivalent information. Here's the smallest UMP message packet, which also happens to be used for MIDI 1.0 messages.

| Field | Size (bits) | Description |
| ----- | ---- | ----------- |
| Message Type | 4 | This indicates the type of message: 32, 64, 96, or 128 bit, but also the use (System real-time, MIDI 1.0 or MIDI 2.0 channel voice, SysEx 7, etc.) |
| Group Index  | 4 | This is the group on stream, much like the Cable Number in USB MIDI 1.0 |
| Remaining Data | n | The remaining data is split across a defined number of 32 bit words. In MIDI 1.0 protocol, the total message is 1 word long, so there are 26 more bits of data

Every UMP starts with the same two fields, and the definition of the next fields is set based upon the message type. Here's an expansion for MIDI 1.0 channel voice Note On messages

| Field | Size (bits) | Description |
| ----- | ---- | ----------- |
| Message Type | 4 | as above |
| Group Index  | 4 | as above |
| Status OpCode | 4 | As used by MIDI 1.0. Dictates how the rest of the message is parsed |
| Status Channel | 4 | As used by MIDI 1.0 |
| Note Number | 8 (7 used) | MIDI 1.0 note number, 0-127 |
| Velocity | 8 (7 used) | MIDI 1.0 velocity value, 0-127 |

The UMP Specification explains how to move MIDI 1.0 data into this format. It's not the same for all messages (like RPN/NRPN and clock) but for Note On/Off, you see that you can simply copy the 3 message bytes into the Status, Note Number, and Velocity fields of the UMP.

The biggest advantage of moving the addressing information directly into the messages, is now other transports, like Network MIDI 2.0, can fully support MIDI 2.0 without having to recreate USB-specific functionality that is needed to address messages.

## Transforming MIDI 1.0 devices into UMP endpoints

MIDI 2.0 devices naturally use UMP as their data format, and fully embrace the "single stream, multiple groups" approach. Although MIDI 1.0 will never disappear (I love my 80s synths), we developed the Windows MIDI Services API and underlying service to fully embrace the future.

Because the group index is now part of the message, we can no longer address ports when sending/receiving. Otherwise, a mismatch between the Group Index and the Virtual Cable would result in undefined behavior. Some implementations of MIDI in other operating systems had this behavior early on, and MIDI Association members agreed it would be confusing even if the group indexes were forced to match the destination group.

Because in Windows we decided to have a unified UMP-based API, we also had to tackle this issue. What we decided to do is to map all MIDI 1.0 streams into UMP-compatible formats. That required aggregating all the MIDI 1.0 endpoints on the device into a stream, and then internally mapping the Group Index with the kernel stream pin number. Each MIDI 1.0 UMP endpoint contains a map which states that `Group 0 is Pin 3 on Interface X`, and `Group 1 is Pin 5 on Interface Y`, etc. 

This is all managed behind the scenes. For USB devices, it's quite a clean mapping because they can have only 16 source and 16 destination cables. 

Custom drivers for things like loopbacks were more troublesome, because inside Windows, they can expose as many pins as they need to, without the USB limitation. For those, we had to change the parsing logic to spin up additional logical endpoints to contain the overflow. That's not as clean, but works. The lack of this feature was part of the source of the initial rollout problem for loopMIDI and similar drivers.

> Note that we ultimately want to eliminate the need for any kernel drivers for MIDI beyond the in-box ones. We know we will not meet that goal 100%, but it is why we have built-in loopback endpoints, as well as virtual device support, and an open architecture for creating transport service plugins using COM. Devices should not become unusable due to a vendor no longer maintaining a driver, and vendor driver bugs should not hold back innovation in MIDI in Windows.

# Concrete Concept Mapping

WinMM is a simple API, but part of that simplicity is because it provides very limited information about ports, and lacks a lot of features customers and developers have requested over the past 30+ years. As a result of that and the integration of MIDI 2.0, the Windows MIDI Services API is somewhat more complex. Here's a mapping of concepts from WinMM to the Windows MIDI Services API.

## A note on threading

Many WinMM apps do all the MIDI work on the UI thread. Although this was never explicitly a forbidden approach, it was never a best practice. We're not judging here, though, just informing.

In Windows MIDI Services, the service startup can take multiple seconds. For the SDK, we were explicitely asked by partner developers to **not** have Async calls, as those can be a pain when you are already managing your own threading. Therefore, **please ensure that your MIDI work, especially initialization and message sending, is not running on the UI thread** in your app. Otherwise, calls which take more than a five seconds can cause your app to "ghost" or "fade" and be reported as unresponsive.

In modern apps, the UI thread should handle UI, and not MIDI. We recommend you initialize WinRT/COM in an MTA (Multi-threaded apartment)-enabled thread to ensure there are no blocking calls.

## Initialization

There are three things you want to do before opening a connection to a MIDI Endpoint

1. Initialize the WinRT (and COM) apartment. In C++/WinRT this is done using `winrt::init_apartment()`
2. Ensure the MIDI Service has started. The MIDI Service will start when the first call is made to it to open any endpoint. However, you need to have those endpoints enumerated first, so it's best to call `MidiApi::EnsureServiceAvailable()` and check the return value. If the user has disabled the service, is using the MIDI Legacy Mode (no service) or the service cannot start for some reason, this will return false. When the service is demand-started in this way, enumeration of MIDI Endpoints begins as soon as it has started up. You can also check the currently set API mode in the `MidiApi` type, and bail (or fall back to older APIs) if it is set to legacy mode. **NOTE: Service startup typically takes several seconds, more if there are many MIDI devices attached.** This is true even when using WinMM under the new MIDI stack.
3. Create a `MidiSession`. An app can have multiple MIDI Sessions if there's logical separation between them (think different open projects). I recommend giving your session a meaningful name that the customer will recognize if they look at active sessions using any of the tools we make available.

Once you have done those three things, you can start working with MIDI Endpoints

## Discovering and identifying MIDI Sources and Destinations

In WinMM, a common pattern is to loop `midiInGetNumDevs` and `midiOutGetNumDevs` waiting for changes in the number of ports, then calling `midiInGetDevCaps` and `midiOutGetDevCaps` on any newly found ports.

This works, but especially when done on the UI thread, is not great for application responsiveness. Another approach is to watch for physical hardware connections. That will not be reliable in Windows MIDI Services because

1. There is not always associated hardware. We support virtual devices, loopback, Network MIDI, etc.
2. Enumeration in the service is asynchronous, especially for MIDI 2.0 devices which have a discovery process to run through before they are officially available for use. This can take several seconds.

Windows MIDI Services provides specializations of the WinRT `DeviceWatcher` which raises events when Endpoints or LegacyApiPorts are added, removed, or (important because port indexes and port names can change) updated. When endpoints show up in the watcher, they are published and available for use.

- The `MidiEndpointDeviceWatcher` watches for changes in UMP Endpoints
- The `MidiLegacyPortDeviceWatcher` watches for changes in Legacy API (WinMM) Ports

> Note: If any .drv WinMM drivers are present, like Coolsoft MIDI Mapper, which modify the WinMM port numbers, the information provided by Windows MIDI Services will not be correct. This was never really an intended use of the WinMM .drv drivers.

> Note 2: In WinMM, there's another asynchronous process which looks for new WinMM ports before it makes them available with the specified port number. If you try to use a WinMM API to open a port inside the Watcher's Added event, it's possible the port number will not yet be recognized by WinMM.

You can find samples of both types of watcher at [https://aka.ms/midisamples](https://aka.ms/midisamples)

### Best Practice: Don't store names

In WinMM, the only persistent identifier we gave you was the port name. That meant users could never rename them, which was the second-most requested feature after multi-client support. In Windows MIDI Services, customers are free to rename ports using either the legacy names (WinMM-style), new-style names (uses iJack when available), or completely custom names.

> Names are not a persistent or durable identifier

In Windows MIDI Services, you have other ways to identify endpoints, using parent device information, unique identifiers, and more. While there is no one solution which will fit every scenario, we recommend, at a minimum, using these as a backup to ensure users are not punished for choosing more meaningful names for their devices.

More [information about some of the identifiers here](.\identifiers.md) and more [information on names here](.\midi1-name-mapping.md). 

## Connecting to a source or destination

Remember that WinMM ports are really just Virtual Cables (for USB).

In Windows MIDI Services, you will open a connection to the bidirectional endpoint, not to an individual cable. The Group Terminal Block (or Function Block for MIDI 2.0 endpoints) will tell you which groups are active and in which direction. The actual group information is part of the UMP message, just like channel information.

After creating the connection using the Session object, you will need to wire up your event handlers or listeners if used, and then call `Open()`. Only at that point will you begin to receive messages.

> Your app needs to be ready for incoming messages as soon as you call Open. That means your COM Extensions callbacks are set up if you are using them, message listeners and event handlers are set up if you are using those. In WinMM, it was common in the past to not have buffers set up until after opening the port. This was the cause of a number of app compat issues when we rolled out Windows MIDI Services, because the new stack is just faster for receiving incoming messages.

You can find samples for at [https://aka.ms/midisamples](https://aka.ms/midisamples)

### Receiving messages only from a single group

The group index is included in the UMP messages sent out, so sending to the correct group is something apps must take care of themselves.

For convenience, the Windows MIDI Services client API includes message listeners. These are provided to make apps which are used to listening to a single MIDI 1 port have an equivalent without opening up multiple (expensive) connections to the same endpoint.

> Note: Connections are somewhat expensive. Each time a connection is opened, a cross-process buffer is allocated for communication between the app and the service. In addition, the service has one more client to service when it is processing data. There's no need to be overly conservative with connections, but you wouldn't want to open 32 connections to the same endpoint just for the convenience of mapping 1:1 with the legacy ports. Instead, use the MidiGroupEndpointListener for incoming message processing. You can add any number of listeners to a single connection, and then wire up separate event handlers to each listener. Note that listners (including the one which implements Virtual Devices) cannot be used with the COM Extensions.

There are three types of listeners included in the SDK in the `Windows.Devices.Midi2.ClientPlugins` namespace. You can also create your own listeners by implmenting the `Windows.Devices.Midi2.IMidiEndpointMessageProcessingPlugin` and `Windows.Devices.Midi2.IMidiMessageReceivedEventSource` interfaces.

You can find samples for Endpoint Listeners at [https://aka.ms/midisamples](https://aka.ms/midisamples). 

## Getting parent device information like VID/PID

In WinMM, some apps will send the `DRV_QUERYDEVICEINTERFACE` message to get some parent device information, and then manually parse the VID/PID from the returned string.

Parent device metadata is included in the `Windows.Devices.Midi2.Enumeration.MidiParentDeviceInformation` type. Not all information will be available for all transports, but given that the additional metadata tends to be used primarily with apps looking to identify specific USB devices, this is where you can find the VID, PID, and if available, Serial, as well as the ServiceName (helps indicate if a third-party driver is in use) and more.

The `MidiEndpointDeviceInformation` and `MidiLegacyPortDeviceInformation` types both expose methods to get the parent device.

This is also where you will find the parent device name.

## Translating messages, and constructing UMPs from MIDI 1 byte data

The MIDI 2.0 and UMP Specification available at https://midi.org includes instructions on how to move MIDI 1.0 byte format messages into UMPs.

In addition, the service will handle translation as required between devices and APIs. However, sometimes you will have data in older formats that you want to send through the new API. The `Windows.Devices.Midi2.Utilities.Messages` namespace has several types which can be used to translate between different types of message formats, construct UMP messages, and more.

For converting MIDI 1 data format to MIDI 1.0 Protocol in UMP, the `MidiMessageConverter` type has most of what you will need, including functions to convert byte arrays (including SysEx) and also hex string representations of bytes (useful for parsing entered SysEx data).

For converting long SysEx split across multiple buffers/arrays, use this method with a state object:

```cpp
static IVector<UInt32> ConvertMidi1CompleteMessageBytesToUmpWords(
            Windows.Devices.Midi2.MidiGroup group,
            IIterable<UInt8> midi1Bytes,
            Boolean allowRunningStatus,
            Windows.Devices.Midi2.Utilities.Messages.MidiBytestreamToUmpMessageConverterState converterState
```

## Sending and receiving messages

In WinMM, there are two primary ways to send messages. `midiOutLongMsg` was used for SysEx messages, and `midiOutShortMsg` for others. In Windows MIDI Services, all MIDI messages are sent the same way. You do not need to distinguish between SysEx or Channel Voice messages other than ensuring they are formatted using the correct UMP message types per the UMP specification.

All methods to send messages are on the `MidiEndpointConnection` type, or the associated COM extension.

Similarly, Windows MIDI Services receives all messages through the `MidiEndpointConnection` type. This is via an event if you use the WinRT events, or via a COM callback if you use the COM extensions.

> The COM callbacks are the only way to receive multiple messages together. The event support is for a single message at a time.

### Best practice: Receive and handle messages quickly for best performance

When you receive an event callback or a COM Extensions callback indicating new message data is available, handle it quickly. You want to keep MIDI flowing and responsive, and the buffer between the service and your app drained, so that means you will want to copy the message data you need into your own processing queue, and then return from the event. If you have time-consuming processing to do, or additional messages to send in response, that should be handled separately outside the event or callback handler.

Although there were no cross-process queues involved, the same advice was true in WinMM before Windows MIDI Services so many professional apps already handle the data in this way.

## Outgoing timestamps

To send a message immediately, as you would in WinMM, pass the `MidiClock::TimestampConstantSendImmediately()` value. You can also use `MidiClock::Now()` but the constant bypasses an additional check in the service and so is ever so slightly more efficient.

You can also schedule messages to be sent a few seconds in the future. We'll continue to tune this feature, as device latency can vary significantly from device to device and driver to driver.

Although the timestamps are currently based on `QueryPerformanceCounter` Always use the MidiClock type in case that implementation changes in the future. The MidiClock type also includes methods to convert between these tick values and human time.

## Incoming timestamps

In WinMM and WinRT MIDI 1.0, incoming message timestamps are an offset from when the port was opened. In Windows MIDI Services, outgoing and incoming timestamps are offsets from when the PC was booted up. They are 64 bit values, currently in 100ns units, and so will not wrap around in our lifetimes, even if the PC is left on every single day. You do not need to handle any sort of wrapping of these numbers.


