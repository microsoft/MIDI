# MIDI API Types Overview

## Concepts

All classes for the API are in the `Windows::Devices::Midi2` namespace described in `Windows.Devices.Midi2.winmd` and implemented in `Windows.Devices.Midi2.dll`. For the developer previews, the DLL, Windows Service, apps, and runtime support are all installed through a single installer. The .winmd and the API projection files are available as a NuGet package. 

We'll cover the major ones here. In addition, there are convenience interfaces, enumerations, and more.

### Preview Notes

The developer preview is **not supported for deployment to end-user / customer machines.** It should only be installed on test and development machines. You will be able to do more end-user testing once Windows MIDI Services is in the Windows Insider and/or retail builds of Windows.

> NOTE: For Developer Preview 1, we do not have COM activation entries in the registry, only WinRT. For those of you doing direct COM activation, assuming you cannot provide a path to the DLL in your code, you will want to copy the Windows.Devices.Midi2.dll to your local folder **just for preview development purposes**.

### EndpointDeviceId

The Endpoint Device Id (also referred to as a Device Id) is the way we identify individual devices and interfaces in Windows. 

Example for one of the built-in loopback endpoints: 
`\\?\SWD#MIDISRV#MIDIU_LOOPBACK_A#{e7cce071-3c03-423f-88d3-f1045d02552b}`

| Part | Description |
| ----- | -- |
| SWD | Software device |
| MIDISRV | The name of the enumerator. For Windows MIDI Services, this is the MidiSrv Windows Service |
| MIDIU_LOOPBACK_A | Arbitrary unique identification string provided by the transport |
| GUID | The interface Id. There are three interfaces defined for UMP: In, Out, and BiDi. |

If you look at the device in Device Manager, and look at Details/Device Instance Path, you'll see all of the information here except for the interface Id. When you enumerate devices through Windows::Devices::Enumeration, the interface Id is included and required.

We don't recommend parsing these strings. If there's information you need about the device which is not contained in the properties, please let us know and we'll look into whether or not we can create a custom property to hold that.

### Endpoint Connection

Individual UMP Endpoints (USB, Network, etc.) have a single connection to the MidiSrv service, no matter how many clients are connected to them.

A client endpoint connection is a single cross-process connection to the MidiSrv service. Creating a new connection has some overhead, in the form of cross-process memory-mapped circular buffer, a set of synchronization objects, and service-side client connection objects. 

### General Flow

For the typical send/receive use-case, this is the flow:

1. Create a New Session
2. Enumerate UMP Endpoints and find one you are looking for
3. Create an Endpoint Connection
4. Add any endpoint listeners you may want to add
5. Wire up your message received handling
6. Open the connection
7. Send/receive messages

For details, see the samples under \get-started\midi-developers\app-developers\samples

## Basic Types

| Type | Description |
| ----- | -- |
| MidiChannel | Represents a single MIDI Channel with an index (0-15) and a number (1-16). Index is used in data, number is used for display. |
| MidiGroup | Represents a single MIDI Group with an index (0-15) and a number (1-16). Index is used in data, number is used for display.  |
| MidiClock | Includes static methods for getting the current MIDI Clock. Internally, we use [QueryPerformanceCounter](https://learn.microsoft.com/windows/win32/sysinfo/acquiring-high-resolution-time-stamps) with a 64 bit return value. However, that is an implementation detail that could change in the future, so we recommend always getting your timestamps from this class.|

## Session and Service

| Type | Description |
| ----- | -- |
| MidiSession | Your entry into the API. All messaging starts here with the use of functions to create endpoint connections. An application can have many open sessions (one per song, or one per page, for example). Connections are scoped to the session and its lifetime. |
| MidiService | Utility methods for interacting with the service. Most application will not need to use this class but plugins and utility apps like the MIDI Console do.|

## Endpoint Connections

| Type | Description |
| ----- | -- |
| MidiEndpointConnection | The primary UMP connection type for sending and receiving messages. In cases where a device is unidirectional (a MIDI 1.0 bytestream device, for example) only the relevant direction will function as expected. However, for simplicity in enumeration for apps, we've consolidated to a single endpoint connection type. |

| Type | Description |
| ----- | -- |
| MidiMessageReceivedEventArgs | For any class which provides messages (MidiInputEndpointConnection, for example) this is the mechanism through which the message data is provided. The events which provide this are synchronous/blocking and fired on the same thread, so you'll want to grab the message data as quick as possible (and put it into your own processing queue) and move on.  |

### Endpoint Message Plugins

MIDI 1.0 had the concept of ports. Each port was just a single cable/jack from a MIDI stream exposed by the device. The API and driver were responsible for merging all of the different cables into the single stream for outgoing data, or pulling them apart for incoming data.

In MIDI 2.0, what used to be a Port is now a Group address in the message data. Instead of speaking to N different enumerated entities for a device, the application speaks to a single bidirectional UMP endpoint which aggregates all of this information, much like the driver did behind the scenes in MIDI 1.0. We recognize that there are cases when the old model of MIDI Ports is more convenient for passing around in a DAW or similar app, particularly for incoming data. 

To help, there are plugins which implement `IMidiEndpointMessageProcessingPlugin`. The API includes a few stock plugins, but developers are free to provide their own, or add to the SDK.

| Type | Description |
| ----- | -- |
| MidiGroupEndpointListener | Fires off the `MidiMessageReceived event only for messages from groups in the provided list |
| MidiChannelEndpointListener | Fires off the `MidiMessageReceived` event only for messages which contain channel information, and where that channel is in the provided list. Optionally, a group may also be provided to limit to a single incoming group.  |
| MidiMessageTypeEndpointListener | FIres off the `MidiMessageReceived` event only for specific message types |

In all of those cases, the application would simply wire up to the `MidiMessageReceived` event in the listener and then add it to the plugin for the endpoint rather than the primary one on the endpoint connection itself.

Listener instances are 1:1 with endpoint connections. We don't support using the same listener on multiple endpoints.

## Messages

Sending and receiving functions in the API include multiple ways of sending and receiving data. We recognize that most applications will have their own internal data structure types, so we do not require the use of strongly-typed messages, but instead provide them as an option.

| Type | Description |
| ----- | -- |
| IMidiUniversalPacket | Interface common across all message types. |
| MidiMessage32 | 32 bit (single 32-bit word) UMP |
| MidiMessage64 | 64 bit (two 32-bit words) UMP |
| MidiMessage96 | 96 bit (three 32-bit words) UMP |
| MidiMessage128 | 128 bit (four 32-bit words) UMP |
| MidiMessageStruct | 128 bit UMP simple structure. Use this when you don't need any of the other conveniences of UMP manipulation. Functions which fill this return the number of valid words written to it. The state of all other words are undefined. |

For the most part, we do not provide strongly-typed message types in the API as that is a moving target, and many applications also include their own message creation and processing functions using their own libraries or any of the libraries included on [https://midi2.dev](https://midi2.dev). If there's demand for strongly-typed messages, we may provide them in the SDK.

## Message Utilities

Although we know there are many native libraries which can do message manipulation, we've exposed some of the basics here. In general, if we found it useful and generally non-volatile, and had to have an internal version for our own processing or for you to make use of another type we provide, we exposed it here.

| Type | Description |
| ----- | -- |
| MidiMessageUtility | A set of utilities for manipulating UMP data. |
| MidiMessageBuilder | Builder for the major types of messages. This doesn't include builders for every possible message, as that is evolving. We may add that to the SDK. This builder is typically where you'd want to start when creating new messages. |
| MidiStreamMessageBuilder | Functions for creating and parsing Stream (type F) messages |
| MidiMessageConverter | Functions for converting MIDI Messages to/from bytestream messages, and Windows.Devices.Midi WinRT message types |
| MidiMessageTranslator | Functions for converting between MIDI 1.0 and MIDI 2.0 protocols, all in UMP |

## Metadata

The Windows Service intercepts (but does not remove from the stream) Endpoint metadata notifications. For example, we'll intercept Endpoint Name notifications and use those to provide a new endpoint-supplied name for the device. These are cached in the SWD properties for the Endpoint Device.

In addition to the endpoint data, we also capture and store block data. The block data should be used by applications to identify which groups are active and how to display them to the user. For example, you may want to display a function block name including group numbers like "Sequencer (Groups 1, 2, 3)" in a way similar to how you treated ports in the past.

| Type | Description |
| ----- | -- |
| MidiFunctionBlock | A single function block for the UMP Endpoint |
| MidiGroupTerminalBlock | Similar to function blocks, but is specific to USB. When both these and function blocks are available, you should prefer the function blocks.|

The MIDI 2.0 UMP specification goes into more detail on the different properties of the function blocks. The USB MIDI 2.0 specification describes the earlier (and now deprecated) Group Terminal Blocks.

## Enumeration

Finally, we have enumeration. Of course, enumeration will work through standard Windows::Devices::Enumeration. However, for ease of accessing all of the custom properties, which must be requested at enumeration time, and for parsing the Function Block, Group Terminal Block, and other binary data, and for presenting the correct endpoint name in your application, we've created specialized versions of the enumeration classes.

| Type | Description |
| ----- | -- |
| MidiEndpointDeviceInformation | Replacement for DeviceInformation. No async calls required. Includes the superset of known custom properties for MIDI devices. |
| MidiEndpointDeviceWatcher | Replacement for the DeviceWatcher. No async calls required. |

Whenever reasonable, prefer the EndpointDeviceWatcher on a background thread vs taking a static snapshot of the connected devices. The watcher will notify you when devices are added or removed, and also when key properties (like name and function blocks) are updated.

> TIP Use the `MidiEndpointDeviceWatcher` to gracefully handle device connects/disconnections, function block and name updates, new network devices coming online, and more.

The samples area includes enumeration examples including how to use the watcher.