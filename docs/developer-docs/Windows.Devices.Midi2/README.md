# Windows.Devices.Midi2

The Windows.Devices.Midi2 types are documented in these pages.

Typical API workflow:

1. **Create a new session**, with an appropriate name. The name will be visible to users and so should be meaningful. Each application may open more than one session at a time (for example, different songs in a DAW, or different tabs in a browser). A single session manages the lifetime of the connections opened through it.
2. **Connect to an endpoint**. Typically, you'll get the endpoint's id through the enumeration functions.
3. **Wire up a MidiMessageReceived event handler**. This is how you will receive incoming messages from the endpoint. Messages are received individually, with one event per message.
4. **Optionally, add any processing plugins**. If you want to filter messages or provide multiple "views" into a stream, you can add the appropriate client message processing plugins.
5. **Open the connection**. Once the connection is open, you may send and receive messages.

## Enumeration

Enumeration is how you discover endpoints and get notified of endpoints when they are added, updated, or removed. For the best user experience, keep a `MidiEndpointDeviceWatcher` running in a background thread so you can monitor device removal, and property updates (name, function blocks, etc.)

* [Overview](./enumeration/)
* [MidiEndpointDeviceInformation](./enumeration/MidiEndpointDeviceInformation/)
* [MidiEndpointDeviceWatcher](./enumeration/MidiEndpointDeviceWatcher/)
* [MidiEndpointDeviceInformationUpdateEventArgs](./enumeration/MidiEndpointDeviceInformationUpdateEventArgs/)

## Session

Interaction with a MIDI Endpoint always starts with creating a session.

* [Overview](./session/)
* [MidiSession](./session/MidiSession/)
* [MidiSessionSettings](./session/MidiSessionSettings/)

## Connections

Once you have a session, you will create one or more connections to send and receive messages.

* [Overview](./connections/)
* [MidiEndpointConnection](./connections/MidiEndpointConnection/)
* [MidiMessageReceivedEventArgs](./connections/MidiMessageReceivedEventArgs/)

## Clock

The MIDI clock is used for creating timestamps for use in sending MIDI messages.

* [Overview](./clock/)
* [MidiClock](./clock/MidiClock/)

## Messages

MIDI Messages are discrete packets of data of a known length. In the MIDI 2.0 specification, they are known as Universal MIDI Packets. In Windows MIDI Services, even MIDI 1.0 bytestream messages are presented in their equivalent Universal MIDI Packet format. The API includes several classes not only for the messages, but also to help construct and parse them.

* [Overview](./messages/)
* [Midi Message Types](./messages/midi-message-types/)
* [MidiMessageBuilder](./messages/MidiMessageBuilder/)
* [MidiMessageConverter](./messages/MidiMessageConverter/)
* [MidiMessageTranslator](./messages/MidiMessageTranslator/)
* [MidiMessageUtility](./messages/MidiMessageUtility/)
* [MidiStreamMessageBuilder](./messages/MidiStreamMessageBuilder/)

## Metadata

Function Blocks and Group Terminal Blocks are important types of MIDI 2.0 metadata which describe an endpoint.

* [Overview](./metadata/)
* [MidiFunctionBlock](./metadata/MidiFunctionBlock/)
* [MidiGroupTerminalBlock](./metadata/MidiGroupTerminalBlock/)

## Client-Side Processing Plugins

Connections allocate service resources (time and memory), so we recommend applications maintain only a single connection to an endpoint within any session. But because the new endpoint stream-focused approach aggregates what used to be considered ports, we provide processing plugins to parcel out the incoming messages based on criteria set by the application. In this way, an application can have the logical equivalent of several input ports, without the associated resource usage.

* [Overview](./processing-plugins/)
* [MidiChannelEndpointListener](./processing-plugins/MidiChannelEndpointListener/)
* [MidiGroupEndpointListener](./processing-plugins/MidiGroupEndpointListener/)
* [MidiMessageTypeEndpointListener](./processing-plugins/MidiMessageTypeEndpointListener/)

## Virtual Devices

A virtual device is the mechanism through which app-to-app MIDI works through the API. One application acts as the MIDI Endpoint Device, and other applications connect to it. In addition to the service component, it is implemented in the client API as a type of Client-SIde Processing Plugin

* [Overview](./virtual-device/)
* [MidiVirtualEndpointDevice](./virtual-device/MidiVirtualEndpointDevice/)
* [MidiVirtualEndpointDeviceDefinition](./virtual-device/MidiVirtualEndpointDeviceDefinition/)
* [MidiStreamConfigurationRequestReceivedEventArgs](./virtual-device/MidiStreamConfigurationRequestReceivedEventArgs/)

## Simple Types

There are several simple or basic types used in Windows MIDI Services. These types provide formatting and validation to help ensure applications display data in similar ways.

* [Overview](./simple-types/)
* [MidiGroup](./simple-types/MidiGroup/)
* [MidiChannel](./simple-types/MidiChannel/)
* [MidiUniqueId](./simple-types/MidiUniqueId/)

## Service

The MidiService class is a utility class which provides access to health and status information related to the MidiSrv Service.

* [Overview](./service/)
* [MidiService](./service/MidiService/)
* [MidiServiceMessageProcessingPluginInformation](./service/MidiServiceMessageProcessingPluginInformation/)
* [MidiServiceTransportPluginInformation](./service/MidiServiceTransportPluginInformation/)
* [MidiServicePingResponse](./service/MidiServicePingResponse/)
* [MidiServicePingResponseSummary](./service/MidiServicePingResponseSummary/)
* [MidiSessionInformation](./service/MidiSessionInformation/)
* [MidiSessionConnectionInformation](./service/MidiSessionConnectionInformation/)
