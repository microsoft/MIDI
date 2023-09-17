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
`\\?\SWD#MIDISRV#MIDIU_DEFAULT_LOOPBACK_BIDI_A#{e7cce071-3c03-423f-88d3-f1045d02552b}`

| Part | Description |
| ----- | -- |
| SWD | Software device |
| MIDISRV | The name of the enumerator. For Windows MIDI Services, this is the MidiSrv Windows Service |
| MIDIU_DEFAULT_LOOPBACK_BIDI_A | Arbitrary identification string provided by the transport |
| GUID | The interface Id. There are three interfaces defined for UMP: In, Out, and BiDi. |

If you look at the device in Device Manager, and look at Details/Device Instance Path, you'll see all of the information here except for the interface Id. When you enumerate devices through Windows::Devices::Enumeration, the interface Id is included.

We don't recommend parsing these strings. If there's information you need about the device which is not contained in the properties, please let us know and we'll look into whether or not we can create a custom property to hold that.

### Endpoint Connection

Individual UMP Endpoints (USB, Network, etc.) have a single connection to the MidiSrv service, no matter how many clients are connected to them.

A client endpoint connection is a single cross-process connection to the MidiSrv service. Creating a new connection has some overhead, in the form of cross-process memory-mapped circular buffer, a set of synchronization objects, and service-side client connection objects. 

### General Flow

For the typical send/receive use-case, this is the flow:

1. Create a New Session
2. Enumerate UMP Endpoints and find one you are looking for
3. Create an Endpoint Connection (typically a Bidirectional one)
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
| MidiBidirectionalEndpointConnection | The primary UMP connection type for sending and receiving messages. It handles MIDI 2.0 protocol work such as endpoint discovery, function block discovery, and setting protocol. |
| MidiInputEndpointConnection | In case we have transports which only expose an input connection |
| MidiOutputEndpointConnection | In case we have transports which only expose an output connection |
| MidiBidirectionalAggregatedEndpointConnection | If you have input and output devices, but need to aggregate them to treat them like a bidirectional endpoint connection, use this class. |

Each of these classes has a Settings class optionally used for configuring them. In addition, there's an endpoint-defined settings class which the SDK can build upon to provide transport-specific settings for connecting (like an IP address, for example). The API simply sends the JSON up to the service for that type of setting.

| Type | Description |
| ----- | -- |
| MidiMessageReceivedEventArgs | For any class which provides messages (MidiInputEndpointConnection, for example) this is the mechanism through which the message data is provided. The events which provide this are synchronous/blocking and fired on the same thread, so you'll want to grab the message data as quick as possible (and put it into your own processing queue) and move on.  |

### Built-in Endpoint types

There are five build-in UMP Endpoints available on every system with Windows MIDI Services installed. These are all used for testing and development. They are always available, so that a support person can ask a customer to run a MIDI Console command, or an in-app feature, to test that the system is healthy. They are also useful, especially in the early days of MIDI 2.0, for developers to use in their own testing and app development. We use them in our own unit and integration tests, for example.

You could use these as simple app-to-app MIDI, but given that these are multi-client and static (you cannot create additional instances) this is not a supported scenario. We will provide proper virtual and app-to-app MIDI support in Windows MIDI Services.

| Type | Description |
| ----- | -- |
| Loopback UMP Endpoint A (BiDi) | Messages sent out on this arrive on Endpoint B's input  |
| Loopback UMP Endpoint B (BiDi) | Messages sent out on this arrive on Endpoint A's input|
| Loopback UMP Endpoint (Out) | Messages sent out here arrive on the loopback In endpoint |
| Loopback UMP Endpoint (In) | Messages arriving from the loopback out endpoint |
| Ping UMP Endpoint (BiDi) | This is for internal use. The MidiService::PingService method uses this to test the connection to the Windows Service. You can see this in action in the MIDI Console app |

### Endpoint Message Plugins

MIDI 1.0 had the concept of ports. Each port was just a single cable/jack from a MIDI stream exposed by the device. The API and driver were responsible for merging all of the different cables into the single stream for outgoing data, or pulling them apart for incoming data.

In MIDI 2.0, what used to be a Port is now a Group address in the message data. Instead of speaking to N different enumerated entities for a device, the application speaks to a single bidirectional UMP endpoint which aggregates all of this information, much like the driver did behind the scenes in MIDI 1.0. We recognize that there are cases when the old model of MIDI Ports is more convenient for passing around in a DAW or similar app, particularly for incoming data. 

To help, there are plugins which implement `IMidiEndpointMessageProcessingPlugin` and optionally one or both of `IMidiEndpointMessageListener` or `IMidiEndpointMessageResponder`. The API includes a few stock plugins, but developers are free to provide their own, or add to the SDK.

| Type | Description |
| ----- | -- |
| MidiGroupEndpointListener | Fires off the `MidiMessageReceived event only for messages from groups in the provided list |
| MidiChannelEndpointListener | Fires off the `MidiMessageReceived` event only for messages which contain channel information, and where that channel is in the provided list. Optionally, a group may also be provided to limit to a single incoming group.  |
| MidiMessageTypeEndpointListener | |

In all of those cases, the application would simply wire up to the `MidiMessageReceived` event in the listener and then add it to the plugin for the endpoint rather than the primary one on the endpoint connection itself.

Listener instances are 1:1 with endpoint connections. We don't support using the same listener on multiple endpoints.

The other types of plugins are used for internal infrastructure. For example, the `MidiEndpointInformationConfigurator`, the `MidiFunctionBlockEndpointListener`, and the `MidiStreamConfigurationEndpointListener` are all used to initiate device queries, catch incoming changes in endpoint information, and update the cache information for the endpoint. 

## Messages

Sending and receiving functions in the API include multiple ways of sending and receiving data. We recognize that most applications will have their own internal data structure types, so we do not require the use of strongly-typed messages, but instead provide them as an option.

| Type | Description |
| ----- | -- |
| IMidiUmp | Interface common across all message types. |
| MidiUmp32 | 32 bit (single 32-bit word) UMP |
| MidiUmp64 | 64 bit (two 32-bit words) UMP |
| MidiUmp96 | 96 bit (three 32-bit words) UMP |
| MidiUmp128 | 128 bit (four 32-bit words) UMP |

For the most part, we do not provide strongly-typed message types in the API as that is a moving target, and many applications also include their own message creation and processing functions. If there's demand for strongly-typed messages, we may provide them in the SDK.

## Message Utilities

Although we know there are many native libraries which can do message manipulation, we've exposed some of the basics here. In general, if we found it useful and generally non-volatile, and had to have an internal version for our own processing anyway, we exposed it here to help other developers.

| Type | Description |
| ----- | -- |
| MidiUmpUtility | A set of utilities for manipulating UMP data. |
| MidiMessageBuilder | Builder for the major types of messages. This doesn't include builders for every possible message, as that is evolving. We may add that to the SDK. This builder is typically where you'd want to start when creating new messages. |

## Metadata

The in-protocol metadata is a bit odd compared to how we normally set up devices outside of MIDI. This information can't be reasonably gathered by the operating system upon device connection, and can also change at any time. For those reasons, at least in version 1.0 of Windows MIDI Services, we are using the message listener infrastructure to update the runtime metadata cache. We may consider moving this to the service in the future, but we generally prefer to have the service do as little message inspection as possible.

| Type | Description |
| ----- | -- |
| MidiEndpointInformation | Aggregate of all the MIDI Endpoint message information discovered through the stream messages |
| MidiFunctionBlock | A single function block for the UMP Endpoint |
| MidiGroupTerminalBlock | Unlike the other metadata, this is provided at device enumeration time, and is specific to USB. |
| MidiEndpointMetadataCache | Applications generally don't need to interact with this. It's public to allow plugins to function |
| MidiGlobalCache | Applications generally don't need to interact with this. It's public to allow plugins to function |

> Important: For the developer preview, the cache is local to the API instance, and is not shared across applications. When the cache service is available, we'll rewire to use that, which will share the information across all applications

## Enumeration

Finally, we have enumeration. For that, use the built-in `Windows::Devices::Enumeration` namespace using the device selectors provided by the endpoint connection types.

Because we're adding additional properties to the devices, we're considering the best ways to expose them to you. We may end up with some enumeration wrappers in the SDK which request the correct property Ids when enumerating, and interpret the results.