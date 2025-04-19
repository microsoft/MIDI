---
layout: kb
title: Best Practices and Performance Optimizations
audience: developers
description: Best practices for developers using the Windows MIDI Services SDK
---

Here's a list of some best practices and performance optimizations for MIDI API-consuming applications.

## Service Startup and WinMM Port creation

One of the things which takes the longest when starting the service is the creation of WinMM and WinRT MIDI 1.0 backwards-compatible ports. If those ports are not needed for a specific MIDI 2.0 endpoint, most transports include an option to turn off creation, or limit the number of ports created. This can significantly speed up service startup.

## Fast transmission of messages

For maximum compatibility across languages, and for safety, WinRT doesn't allow pointers to be exposed by any properties or as parameters or return types for any function. In addition, the by-value and by-reference semantics for parameters are not always under the control of the API developer.

For those reasons, and to maximize ease of use across a number of languages and use-cases, we have multiple ways to send and receive messages.

You will want to do your own performance testing from your application and scenarios, but in general, the send/receives with the least overhead are those which send/receive individual 32 bit words, a single 128 bit structure, or the `IMemoryBuffer`. The word and struct methods do pass copies of data, but the amount of data, for most time critical messages, is still 64 bits or less (MIDI 1.0 channel voice messages are 32 bits, MIDI 2.0 channel voice messages are 64 bits).

The `IMemoryBuffer` approach is a more advanced way to transfer data to and from the API. This wraps a buffer of data which you can reuse between calls, including send/receive, as long as you manage and avoid any potential overlaps. Internally, the COM types used to access this ensures that only pointers are passed into the API. There's a bit more ceremony to using this approach, so we recommend investing time there only if it better fits your app's programming model. In addition, because IMemoryBuffer deals with bytes and not 32 bit words, you need to ensure you are correctly copying the data in, following the endianness rules for our internal MIDI 2.0 data representation.

The most flexible, but least performant approach, is to use the `IMidiMessage` interface and the methods which return strongly typed messages. These do involve additional type allocations either on the part of the caller or in the API code.

### Data copies

In the underlying implementation, copying of data is unavoidable in places. Here are the main places where it happens.

When sending messages

1. The individual WinRT projection for your language may enforce a copy or translation of the data going into the SDK. This varies. Arrays, in particular, vary here.
2. The SDK and API copy the data (typically a `memcpy`), regardless of how it is provided, into the cross-process queue for that client endpoint connection. This is shared cross-process memory on Windows. It's also a circular queue, so we can't hold onto pointers for long, which is why 4 below operates how it does.
3. On the service side, the pointers into the buffer are provided to the client connection and the plugins in that chain. No copying here.
4. There will be copies of the data created if there are any processing plugins which must significantly manipulate the data (each plugin decides how it deals with the data), or if you schedule the message to be sent in the future (see 2 above). Translation performs copies to/from byte format, for example.
5. Finally, the messages may be copied when being supplied to the transport. In the case of USB MIDI 2.0, we make a call into a kernel driver, so have another cross process queue for that which requires we copy data into it to supply it to the driver. For USB MIDI 1.0, we make an Ioctl call, so there is no additional queue, however the Ioctl is slower than the queue processing. In the case of networking, we have to copy the data into the network buffers and transmit. In app-to-app / virtual MIDI, and also the built-in loopback endpoints, we typically just send the same message pointers through the entire process and do not copy any data in the transport.

This code is all quite efficient, and the amount of data in a single message is small, so these happen quite quickly. Nevertheless, we're always looking at places where we can further optimize, but still retain the flexibility provided by having a Windows Service which processes the messages.

When receiving messages, the process is almost exactly the opposite of sending. There's no in-bound message scheduling, but there may be data transformations that plugins perform. In addition, endpoints with multiple clients connected do require fanning out those messages into multiple queues, resulting in multiple copies across the different cross-process inbound client connection queues. That is a small price to pay for full multi-client MIDI support.

## Displaying connections to your app users

Most apps need to display device and endpoint connection information to their users. Here are some details related to that.

### Use the `MidiEndpointDeviceWatcher` to respond to device changes

MIDI devices come and go based on connecting/disconnecting USB cables, or new network endpoints coming online. In addition, properties like Function Blocks and Endpoint Name are subject to change at any time. Use the `Microsoft::Windows::Devices::Midi2::MidiEndpointDeviceWatcher` class on a background thread to monitor these endpoints, and receive notifications when anything changes. This is a much more robust approach vs simply enumerating a snapshot of devices up-front.

There's no API or service reason to require a customer to reboot or reload/restart a MIDI DAW or other application to see newly added endpoints when using Windows MIDI Services.

For more information, see the [How to Watch Endpoints](developer-how-to/how-to-watch-endpoints.html) page.

### Don't include diagnostics endpoints for most apps

Unless the app is a utility / testing app, we recommend you do not display the UMP Loopback Endpoints to the user. These are for diagnostics and testing only. By default, they are excluded during enumeration.

### Always display the Group Number, not the Group Index

Groups, like Channels, are indexed 0-15, but the actual number to present to the user is always 1-16. The Built-in `MidiGroup` and `MidiChannel` types in the SDK make it easy to ensure you are using the correct values for data or display.

### Enable drill-down into Groups (functions)

A single function block may exist on multiple groups, and multiple groups may overlap function blocks. That is the nature of the MIDI 2.0 specification. In most cases, you'll find that a function is associated with one or more groups and those groups do not span other function blocks.

We recommend that, when displaying a connection to the user, you connect them to the UMP Endpoint, but then enable some sort of drill-down to show the function block names and their associated groups. Remember that the ultimate address of most MIDI Messages is the Endpoint, Group, and Channel.

```
SynthCompany Foo Synth 5
- Synthesizer (Groups 1, 2, 3)
- Sequencer (Groups 3, 4, 5)
- MIDI DIN Out (Group 6)
```

or

```
SynthCompany Foo Synth 5
- Group 1 (Synthesizer)
- Group 2 (Synthesizer)
- Group 3 (Synthesizer, Sequencer)
- Group 4 (Sequencer)
- Group 5 (Sequencer)
- Group 6 (MIDI DIN Out)
```

Or similar based on the conventions of your application.

Note that a flat list, like what many apps used for MIDI 1.0 ports, is not as reasonable in a MIDI 2.0 world. Best practices for this will come out over time as various applications grapple with the increased address count in MIDI 2.0.

### Use `.AsEquivalentFunctionBlock()` for Group Terminal Blocks

USB MIDI 1.0 devices and some USB MIDI 2.0 devices will not have Function Blocks. Per-spec, Function Blocks are optional. However, those USB devices will have Group Terminal Blocks. The preference is to use the Function Block when available. However, to keep your data model uniform, we project Function Blocks from Group Terminal Blocks using the `.AsEquivalentFunctionBlock()` function of the `MidiGroupTerminalBlock` type. Not all properties map cleanly, but we make a best-effort attempt here to provide the application with usable data that can be presented to the user.

### Use the Function Block UI Hint to help you decide how to show functions

The UI Hint property of a Function Block was created to give the UI an indication of the intended direction of communication, as a user would see it, for a function block. This shouldn't necessarily block functions from showing up in a list that contains, for example, input devices, but it may be that you want to prioritize the ones with an appropriate UI hint, and have a "see all" option or similar to display the rest.

## Prefer not mixing legacy APIs and Windows MIDI Services in the same session

There's nothing technically preventing you from using winmm or WinRT MIDI 1.0 in the same application, at the same time as the new API, but there's also no need to beyond transitioning code. The new API will do everything the old does, plus a lot more. The older APIs don't have access to a lot of the metadata you'll need for devices, and in the case of MIDI 2.0 endpoints, will require additional message translation in the service. 

Of course, offering a choice between Windows MIDI Services and an older API in your application is perfectly acceptable, based on your use cases, and which versions of the operating systems you need to support.
