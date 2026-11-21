---
layout: kb
title: Moving from WinRT MIDI 1.0 to Windows MIDI Services
audience: developers
description: Concepts, enumeration, timestamps and API mapping for applications that use Windows.Devices.Midi
categories:
  - Developer Guidance
---

This article is for developers of applications that use `Windows.Devices.Midi`, the WinRT MIDI 1.0 API introduced with Windows 10, and want to move to the Windows MIDI Services API, `Windows.Devices.Midi2`.

Read [Moving from WinMM to Windows MIDI Services]({{ site.baseurl }}/kb/moving-from-winmm-to-wms/) as well. Most of what it says about threading, initialization, groups, names and identifiers applies to you too. This article covers what's different when you're coming from WinRT MIDI 1.0, and spends extra time on device enumeration, because WinRT MIDI 1.0 used the general `Windows.Devices.Enumeration` types and Windows MIDI Services has its own.

WinRT MIDI 1.0 keeps working on a PC with Windows MIDI Services. It goes through the MIDI service, so your application gets multi-client access without any changes. But it only ever sees MIDI 1.0 ports and MIDI 1.0 messages. Message scheduling, MIDI 2.0 messages, function blocks, endpoint metadata, virtual devices and loopbacks you create yourself are only in the new API. See [Application Backwards Compatibility]({{ site.baseurl }}/kb/api-back-compat/).

## Quick reference: what you used to call, and what to call now

| Task | WinRT MIDI 1.0 | Windows MIDI Services | Reference |
| ---- | -------------- | --------------------- | --------- |
| Work out whether the new API is available | Nothing to check | Check that the PC is Windows 11 25H2 or later, resolve `Windows.Devices.Midi2.MidiApi`, then call `MidiApi.EnsureServiceAvailable` | [MidiApi]({{ site.baseurl }}/sdk-reference/MidiApi/) |
| Get a list of devices | `DeviceInformation.FindAllAsync(MidiInPort.GetDeviceSelector())`, and again with `MidiOutPort.GetDeviceSelector()` | `MidiEndpointDeviceInformation.FindAll()`, which is synchronous and returns both directions in one list. However, the `MidiEndpointDeviceWatcher` is preferred over a static list | [MidiEndpointDeviceInformation]({{ site.baseurl }}/sdk-reference/Enumeration/MidiEndpointDeviceInformation/) |
| React to devices arriving and leaving | `DeviceInformation.CreateWatcher` with one of those selectors | `MidiEndpointDeviceWatcher.Create()` | [MidiEndpointDeviceWatcher]({{ site.baseurl }}/sdk-reference/Enumeration/MidiEndpointDeviceWatcher/) |
| Open an input | `await MidiInPort.FromIdAsync(id)` | `MidiSession.CreateEndpointConnection(endpointDeviceId)`, attach handlers, then `Open()`. One connection does both directions and all Groups or "Ports" for the endpoint. | [MidiSession]({{ site.baseurl }}/sdk-reference/MidiSession/), [MidiEndpointConnection]({{ site.baseurl }}/sdk-reference/MidiEndpointConnection/) |
| Open an output | `await MidiOutPort.FromIdAsync(id)` | The same connection as the input. Opening a second connection to the same endpoint wastes cross-process resources. | [MidiEndpointConnection]({{ site.baseurl }}/sdk-reference/MidiEndpointConnection/) |
| Receive messages | `MidiInPort.MessageReceived`, with a typed `IMidiMessage` | The COM extensions callback, which hands you one or more messages per call, or `MidiEndpointConnection.MessageReceived`, one message per call | [COM Extensions]({{ site.baseurl }}/sdk-reference/MidiEndpointConnection_COM-Extensions/), [MidiMessageReceivedEventArgs]({{ site.baseurl }}/sdk-reference/MidiMessageReceivedEventArgs/) |
| Receive from one port only | One `MidiInPort` per port, so this was built in | `MidiGroupEndpointListener`, because one connection carries up to 16 groups | [MidiGroupEndpointListener]({{ site.baseurl }}/sdk-reference/ClientPlugins/MidiGroupEndpointListener/) |
| Send a message | `SendMessage(new MidiNoteOnMessage(channel, note, velocity))` | `SendSingleMessageWords`, or `SendSingleMessagePacket` with a message built by `MidiMessageBuilder` | [MidiMessageBuilder]({{ site.baseurl }}/sdk-reference/Utilities/Messages/MidiMessageBuilder/) |
| Send a message you already built as an `IMidiMessage` | `SendMessage` | `MidiMessageConverter.ConvertMidi1MessageToUmpWords(group, message)`, then send the words | [MidiMessageConverter]({{ site.baseurl }}/sdk-reference/Utilities/Messages/MidiMessageConverter/) |
| Send raw MIDI 1.0 bytes | `SendBuffer` | `MidiMessageConverter.ConvertMidi1CompleteMessageBytesToUmpWords` | [MidiMessageConverter]({{ site.baseurl }}/sdk-reference/Utilities/Messages/MidiMessageConverter/) |
| Send SysEx | `SendMessage(new MidiSystemExclusiveMessage(buffer))` | The same converter, then a multiple-message send. For a whole `.syx` file, `MidiSystemExclusiveSender` | [MidiSystemExclusiveSender]({{ site.baseurl }}/sdk-reference/Utilities/SysExTransfer/MidiSystemExclusiveSender/) |
| Send a message at a future time | Not available | Pass a future `MidiClock` timestamp to any send function | [MidiClock]({{ site.baseurl }}/sdk-reference/MidiClock/) |
| Play the built-in synthesizer | `await MidiSynthesizer.CreateAsync()` | Open a connection to `MidiSynthManager.EndpointDeviceId`. Currently available in preview. | [Synth transport]({{ site.baseurl }}/sdk-reference/Transports/Synth/) |
| Get the USB vendor and product ids | Read device properties yourself, or parse the device id | `GetParentDeviceInformation()`, then `UsbVendorId` and `UsbProductId` | [MidiParentDeviceInformation]({{ site.baseurl }}/sdk-reference/Enumeration/MidiParentDeviceInformation/) |
| Close a port | `Dispose()` or `Close()` on the port | `MidiSession.DisconnectEndpointConnection`, or close the session | [MidiSession]({{ site.baseurl }}/sdk-reference/MidiSession/) |

## Before you start

- **The new API is synchronous.** WinRT MIDI 1.0 used `async` for opening ports and enumerating devices. Developers who build music applications asked us not to do that, because they already manage their own threads. So nothing in the main `Windows.Devices.Midi2` message flow returns an `IAsyncOperation`. That means **you must not call it from the UI thread**. Starting the service can take several seconds, and your window would stop responding. Do MIDI work on a worker thread, with WinRT initialized in a multi-threaded apartment.
- **Check that the API is there before you use it.** The PC needs Windows 11 25H2 or later, the `Windows.Devices.Midi2.MidiApi` type has to resolve, and `MidiApi.EnsureServiceAvailable()` has to return true. The customer may have chosen [Legacy API mode]({{ site.baseurl }}/kb/how-to-change-api-mode/). When any answer is no, keep using WinRT MIDI 1.0. The [detect-midi-services](https://github.com/microsoft/MIDI/tree/main/samples/cpp-winrt/detect-midi-services) sample shows the whole check.
- **Two types have the same name.** Both APIs have a `MidiMessageReceivedEventArgs`, one in `Windows.Devices.Midi` and one in `Windows.Devices.Midi2`. If your code uses both namespaces during the move, qualify the name or use an alias.
- **Packaged and UWP applications.** WinRT MIDI 1.0 works in UWP and packaged applications. We're still checking Windows MIDI Services in those application types. See the table in [Application Backwards Compatibility]({{ site.baseurl }}/kb/api-back-compat/).
- **Bluetooth MIDI is different.** WinRT MIDI 1.0 has its own Bluetooth LE MIDI support, which is separate from Windows MIDI Services. Windows MIDI Services has its own Bluetooth LE MIDI transport, which is much more compatible with devices, and is available now as a preview. Devices it connects show up as normal endpoints. See [MIDI Bluetooth Setup]({{ site.baseurl }}/tools/midibluetoothsetup/).

## Ports, endpoints and groups

In WinRT MIDI 1.0, an input port and an output port were separate devices with separate ids, and each one carried a single MIDI 1.0 stream. A USB device with four MIDI 1.0 ports in each direction showed up as eight `DeviceInformation` entries.

In Windows MIDI Services, that device is **one endpoint** with one id. You open one connection, and it carries both directions. Each of the old ports is now a **group** on that endpoint, and the group index is part of every message, next to the channel index. The endpoint's group terminal blocks (USB MIDI 1.0 devices) or function blocks (MIDI 2.0 devices) tell you which groups exist and which way each one goes. For MIDI 2.0 devices, the function block's "UI Hint" tells you the intended use of that block.

So when you move your code:

- Replace your input port list and output port list with one endpoint list, and show the groups inside each endpoint.
- When you send, put the group index in the message. When you receive, look at the group in the message, or use a `MidiGroupEndpointListener` for each group you care about.
- Don't open one connection for each group. Connections cost more than listeners do.
- Groups are indexed 0 to 15 inside the message. People and the apps' screens count them from 1 to 16, just like channels. The `MidiGroup` and `MidiChannel` types can handle this for you.

The [WinMM article]({{ site.baseurl }}/kb/moving-from-winmm-to-wms/#free-your-mind-from-midi-10-constraints) explains this hierarchy in more depth, and why USB MIDI 1.0 cables became groups.

## From Windows.Devices.Enumeration to Windows.Devices.Midi2.Enumeration

WinRT MIDI 1.0 didn't have its own enumeration types. You built a selector string with `MidiInPort.GetDeviceSelector()` or `MidiOutPort.GetDeviceSelector()`, and gave it to the general-purpose `DeviceInformation` and `DeviceWatcher` types in `Windows.Devices.Enumeration`. Those types work for every kind of device, so they know nothing about MIDI.

Windows MIDI Services wraps those same Windows types in MIDI-specific ones. Underneath, it's still `DeviceInformation` and `DeviceWatcher`, so the same rules about events and threads apply. But the MIDI types request all the MIDI properties for you and give them back with proper types, and they're synchronous. You don't need to do any manual propery lookups or requests.

### Types

| Windows.Devices.Enumeration with WinRT MIDI 1.0 | Windows.Devices.Midi2.Enumeration |
| ----------------------------------------------- | --------------------------------- |
| `DeviceInformation` for an input port or an output port | `MidiEndpointDeviceInformation` for an endpoint, which goes both ways |
| `DeviceWatcher` | `MidiEndpointDeviceWatcher` |
| `DeviceInformationUpdate` | `MidiEndpointDeviceInformationUpdatedEventArgs`, which also indicates *what* changed. |
| `DeviceInformation` passed to `Added` | `MidiEndpointDeviceInformationAddedEventArgs.AddedDevice` |
| `DeviceInformationUpdate` passed to `Removed` | `MidiEndpointDeviceInformationRemovedEventArgs.RemovedDevice` |
| `DeviceInformationCollection` | `IVectorView<MidiEndpointDeviceInformation>` from `FindAll` |
| A list you kept yourself from watcher events | `MidiEndpointDeviceWatcher.EnumeratedEndpointDevices`, kept up to date for you |
| `DeviceWatcherStatus` | The same enumeration, from `MidiEndpointDeviceWatcher.Status` |

### Functions

| Windows.Devices.Enumeration with WinRT MIDI 1.0 | Windows.Devices.Midi2.Enumeration |
| ----------------------------------------------- | --------------------------------- |
| `DeviceInformation.FindAllAsync(MidiInPort.GetDeviceSelector())` | `MidiEndpointDeviceInformation.FindAll()` |
| `FindAllAsync(selector, additionalProperties)` to get more properties | Not needed. The properties are already there, with types |
| `DeviceInformation.CreateFromIdAsync(id)` | `MidiEndpointDeviceInformation.CreateFromEndpointDeviceId(endpointDeviceId)` |
| `DeviceInformation.CreateWatcher(selector)` | `MidiEndpointDeviceWatcher.Create()`, or `Create(filters)` to choose which kinds of endpoints to include |
| `watcher.Start()` and `watcher.Stop()` | The same |
| `Added`, `Removed`, `Updated`, `EnumerationCompleted` and `Stopped` events | The same events, with the MIDI-specific arguments above |
| Building your own selector to filter devices | `MidiEndpointDeviceInformationFilters`, passed to `FindAll` or `Create` |
| `DeviceInformation.FindAllAsync` with a container id in the selector | `MidiEndpointDeviceInformation.FindAllForContainer(containerId)` |

If you really do need to call `Windows.Devices.Enumeration` yourself, for example to merge MIDI devices into a list of other devices, use `MidiEndpointConnection.GetDeviceSelector()` for the selector and `MidiEndpointDeviceInformation.GetAdditionalPropertiesList()` for the properties to ask for. Most applications don't need this.

### Properties

| `DeviceInformation` with WinRT MIDI 1.0 | `MidiEndpointDeviceInformation` |
| --------------------------------------- | ------------------------------- |
| `Id`, which you passed to `FromIdAsync` | `EndpointDeviceId`, which you pass to `CreateEndpointConnection` |
| `Name` | `Name`. It includes any name the customer chose, and can change at any time |
| Port direction, from which selector found it | Each group's direction, from `GetGroupTerminalBlocks()` or `GetDeclaredFunctionBlocks()` |
| `Properties["System.Devices.ContainerId"]` | `ContainerId`, and `GetContainerDeviceInformation()` for the whole container |
| Parent device properties you looked up yourself | `GetParentDeviceInformation()`, with the parent's name, USB vendor and product ids, serial number when there is one, and driver details |
| Nothing | `IsMuted` tells you whether a customer muted the endpoint, so no messages get through. Currently available only for loopback endpoints. |
| Nothing | `GetTransportSuppliedInfo()` indicates the kind of device, and what the device or driver originally named it |
| Nothing | `GetUserSuppliedInfo()` provides what the customer set up for this endpoint, including its name, image, and outgoing latency |
| Nothing | `GetDeclaredEndpointInfo()`, `GetDeclaredDeviceIdentity()` and `GetDeclaredFunctionBlocks()`, with what a MIDI 2.0 device said about itself |
| `Properties` | `Properties` is still there, but don't depend on the raw property keys and values. They aren't part of what the API promises, and everything useful in them has a typed property or function |

### Handle Updated for as long as the watcher runs

With WinRT MIDI 1.0, you could mostly ignore the watcher's `Updated` event. With Windows MIDI Services, you can't. A MIDI 2.0 endpoint describes itself after it first appears, so its name, function blocks and groups can arrive over the next few seconds. Customers can also rename an endpoint or change it in MIDI Settings while your application runs. Each update says which groups of properties changed, such as `IsNameUpdated` and `AreFunctionBlocksUpdated`. Treat each one as "read again what you care about", not as the final state. See [Endpoint arrival and update ordering]({{ site.baseurl }}/kb/endpoint-arrival-and-update-ordering/).

### Moving saved port ids to endpoints

If your application saved WinRT MIDI 1.0 port ids, for example in a project file, you can find which endpoint and group they belong to now. `Windows.Devices.Midi2.Enumeration.Legacy.MidiLegacyPortDeviceInformation` describes the MIDI 1.0 ports that Windows MIDI Services creates for its endpoints, and its `PortDeviceId` is the `DeviceInformation.Id` you got from WinRT MIDI 1.0 for those ports. This can't be used reliably across PCs or OS versions, or when a USB device with no serial number is moved between USB ports, as the ids will be different.

```cpp
auto port = MidiLegacyPortDeviceInformation::CreateFromPortDeviceId(savedWinRtPortId);

if (port != nullptr)
{
    auto endpointDeviceId = port.AssociatedEndpointDeviceId();
    auto group = port.Group();
    auto flow = port.Flow();    // source (input) or destination (output)

    // save endpointDeviceId and the group number from now on
}
```

This only works while the device is connected, and only for ports Windows MIDI Services created. Ports from WinRT MIDI 1.0's own Bluetooth support aren't among them. If the port isn't found, ask the customer to pick the device again. After that, save the `EndpointDeviceId` and the group number, never the name. See [Windows MIDI Services identifiers]({{ site.baseurl }}/kb/identifiers/) and [Integrating MIDI 1.0 and MIDI 2.0 APIs]({{ site.baseurl }}/kb/integrating-midi1-and-midi2-apis/).

If you need to do a lookup by name, you can use the transport-supplied information described above to get the device name originally provided by the driver to give you a bit of a fuzzy match. However, names in general are not durable ids for devices or ports -- the user may change them at any point to use new-style names, classic WinMM-style names, or completely custom names.

## Sending and receiving messages

WinRT MIDI 1.0 gave you a class for each kind of MIDI 1.0 message, such as `MidiNoteOnMessage` and `MidiControlChangeMessage`, plus `MidiSystemExclusiveMessage` and the raw bytes in `RawData`. Windows MIDI Services uses Universal MIDI Packets (UMP) for everything. A MIDI 1.0 channel message is one 32-bit word that holds the group and the same three bytes you're used to. The [UMP data format primer]({{ site.baseurl }}/kb/ump-data-format-primer/) shows the layout.

You have a few ways to move your sending code:

- **Keep your `IMidiMessage` objects for now.** `MidiMessageConverter::ConvertMidi1MessageToUmpWords(group, message)` turns any WinRT MIDI 1.0 message into UMP words. There are also converters for each message type, such as `ConvertMidi1NoteOnMessage`. This is the quickest way to get going, but it creates objects for every message.
- **Build UMP words directly.** `MidiMessageBuilder::BuildMidi1ChannelVoiceMessage` builds a channel message, or you can put the bits together yourself and call `SendSingleMessageWords`. That's the fastest path through the WinRT functions.
- **Bytes and SysEx.** `MidiMessageConverter::ConvertMidi1CompleteMessageBytesToUmpWords` turns MIDI 1.0 bytes, SysEx included, into UMP words. To send a `.syx` file, use `MidiSystemExclusiveSender`, and to receive one, `MidiSystemExclusiveReceiver`.

To turn incoming UMP back into MIDI 1.0 bytes, for code that isn't ready to change yet, use `MidiMessageConverter::ConvertSingleGroupCompleteMessageUmpWordsToMidi1Bytes`.

Call `GetSupportedMaxMidiWordsPerTransmission` on the connection to find out how much you can send in one call. Don't hard-code a number. Remember that one MIDI UMP Word is equivalent to one 1,2, or 3 byte MIDI 1.0 byte data format message. SysEx has a completely different calculation.

### Use the COM extensions if you already handle UMP

If your code already builds and reads UMP words, or you're happy to write that part, use the COM extensions on the connection. They're the fastest way to send and receive:

- **`IMidiEndpointConnectionRaw::SendMidiMessagesRaw`** sends a buffer of whole UMPs, with a timestamp, without creating any WinRT objects.
- **`IMidiEndpointConnectionMessagesReceivedCallback`** is called with a pointer to one or more incoming messages. **It's the only way to receive more than one message in a single call.** Both `MessageReceived` events, in WinRT MIDI 1.0 and in Windows MIDI Services, hand you one message at a time, which could require additional correlation for you when it comes to logical messages (like (N)RPN) which in MIDI 1.0 are comprised of multiple separate messages.

Register the callback with `SetMessagesReceivedCallback` *before* you call `Open()`. While it's registered, the connection raises no `MessageReceived` events and runs no message listeners, so you can't use it together with `MidiGroupEndpointListener` or a virtual device app implementation. Remove it with `RemoveMessagesReceivedCallback` before you close the connection.

The COM extensions are for C++ and other languages that can call COM. From other languages, use the WinRT send functions and the `MessageReceived` event. See [COM Extensions]({{ site.baseurl }}/sdk-reference/MidiEndpointConnection_COM-Extensions/) and the [com-extensions](https://github.com/microsoft/MIDI/tree/main/samples/cpp-winrt/com-extensions) sample.

### Receiving

- **Be ready before you open.** Attach your `MessageReceived` handler, your listeners or your COM callback, and only then call `Open()`. Messages can arrive right away.
- **Return quickly.** Copy what you need into your own queue and do the real work on another thread, not inside the event handler or callback.
- **Don't keep the event arguments.** In Windows MIDI Services, the data behind `MidiMessageReceivedEventArgs` exists only while your handler runs. Copy the message out first, with `FillWords`, `FillMessageStruct` or `GetMessagePacket`. This is different from WinRT MIDI 1.0, where you could keep the `IMidiMessage`, which was already a copy of the data.

## Timestamps

| | WinRT MIDI 1.0 | Windows MIDI Services |
| --- | --- | --- |
| Type | `TimeSpan` on each `IMidiMessage` | `UInt64` |
| Incoming | Time since you opened the port | The absolute `MidiClock` time when the service received the message, counted from when the PC started |
| Outgoing | Not used. Every message goes out right away | Any send function takes a timestamp. `MidiClock::TimestampConstantSendImmediately` sends right away. A future time schedules the message |
| Tick length | `TimeSpan` ticks of 100 ns | `1 / MidiClock.TimestampFrequency` seconds. That's currently 100 ns, but read it rather than assume it |

**The incoming timestamps are on a different timeline.** In WinRT MIDI 1.0, a timestamp near zero meant "just after I opened the port". In Windows MIDI Services, timestamps are large numbers counted from when the PC started. If your code compares a timestamp to the time it opened the port, or turns timestamps into `TimeSpan` values, change it to work with `MidiClock` values. Read `MidiClock::Now` when you need "now", and use the `MidiClock::ConvertTimestampTicksTo...` functions for differences between two timestamps.

Because incoming and outgoing timestamps are on the same clock, you can schedule a reply at a set time after a message arrived, or line up input from two endpoints, without any conversion.

### Scheduling messages

WinRT MIDI 1.0 couldn't send a message at a future time, so you probably ran your own timer. With Windows MIDI Services, you can hand that to the service:

```cpp
auto start = MidiClock::Now();

connection.SendSingleMessageWords(MidiClock::OffsetTimestampByMilliseconds(start, 0), noteOn);
connection.SendSingleMessageWords(MidiClock::OffsetTimestampByMilliseconds(start, 500), noteOff);
```

Read `Now` once and offset that one value for each message, so the spacing between messages stays exact. Keep a short window of messages scheduled ahead, not a whole song, because the scheduler slows down when it holds thousands of messages. The limit is `MidiClock::TimestampConstantMessageQueueMaximumFutureTicks`, currently five minutes ahead, but subject to change. The scheduler also sends early by the endpoint's outgoing latency compensation, so a message reaches a Bluetooth or network device on time. See [MidiClock]({{ site.baseurl }}/sdk-reference/MidiClock/) and the [scheduled-send-messages](https://github.com/microsoft/MIDI/tree/main/samples/cpp-winrt/scheduled-send-messages) sample. *

\* The outgoing message scheduler logic in midisrv has been updated to use less CPU and be more accurate. That update goes out in the first half of 2027.

## The built-in synthesizer

WinRT MIDI 1.0 gave you the Microsoft GS Wavetable Synth through `MidiSynthesizer`. Windows MIDI Services has a new built-in General MIDI synthesizer, available now as a preview for developers. It's a normal endpoint: get its id from `MidiSynthManager.EndpointDeviceId`, open a connection, and send it messages. An empty id means it isn't installed or the customer turned it off. See [WinRT API Support for the General MIDI Synthesizer]({{ site.baseurl }}/sdk-reference/Transports/Synth/).

## Samples

All the samples are at [https://aka.ms/midisamples](https://aka.ms/midisamples), most in both C++/WinRT and C#. The ones that matter most when you're coming from WinRT MIDI 1.0:

- [basics](https://github.com/microsoft/MIDI/tree/main/samples/cpp-winrt/basics) ([C#](https://github.com/microsoft/MIDI/tree/main/samples/csharp-net/basics)): start up, open a connection, send and receive
- [watch-endpoints](https://github.com/microsoft/MIDI/tree/main/samples/cpp-winrt/watch-endpoints) ([C#](https://github.com/microsoft/MIDI/tree/main/samples/csharp-net/watch-endpoints)): keep a list of endpoints up to date
- [static-enum-endpoints](https://github.com/microsoft/MIDI/tree/main/samples/cpp-winrt/static-enum-endpoints) ([C#](https://github.com/microsoft/MIDI/tree/main/samples/csharp-net/static-enum-endpoints)): a one-time list, with group terminal blocks and function blocks
- [watch-midi1-ports](https://github.com/microsoft/MIDI/tree/main/samples/cpp-winrt/watch-midi1-ports) ([C#](https://github.com/microsoft/MIDI/tree/main/samples/csharp-net/watch-midi1-ports)): the MIDI 1.0 ports Windows MIDI Services creates, and the endpoints they belong to
- [endpoint-listeners](https://github.com/microsoft/MIDI/tree/main/samples/cpp-winrt/endpoint-listeners) ([C#](https://github.com/microsoft/MIDI/tree/main/samples/csharp-net/endpoint-listeners)): receive from one group or channel
- [scheduled-send-messages](https://github.com/microsoft/MIDI/tree/main/samples/cpp-winrt/scheduled-send-messages) ([C#](https://github.com/microsoft/MIDI/tree/main/samples/csharp-net/scheduled-send-messages)): send messages at future times
- [com-extensions](https://github.com/microsoft/MIDI/tree/main/samples/cpp-winrt/com-extensions): send and receive with no per-message objects (C++ only)
- [detect-midi-services](https://github.com/microsoft/MIDI/tree/main/samples/cpp-winrt/detect-midi-services): decide at startup whether the new API is there
