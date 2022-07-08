# API ideas

These are not final designs, but rather are brainstorming design ideas. All code is pseudo-c code with just enough syntax to get the point across. You will notice a real lack of semicolons, for example. :)

## General Approach

* We always provide first-class access to raw data
  * We always provide classes to process that data into friendly formats, but do not require their use
  * Core API is meant to stay lean and fast, with addtional features added as helper classes, as long as performance is not negatively impacted
* One exception to raw data is the required Discovery and Protocol Negotiation for MIDI-CI. We provide the final results, but the transaction itself is handled by the MIDI Services.
* Entry point into API functionality is always the session
  * An app may have as many sessions open as it needs
* Everything is multi-client and all devices in the session are available all the time
  * Only endpoints need to be opened, and that's really only to wire up handling for incoming messages
  * Device enumeration is detailed, flexible, and supports our device/transport plugin approach
* The user is in control
  * The user can override certain settings for endpoints, like disabling protocol negotiation, for example, and also setting the name to whatever they want, so that they are in control of the setup
  * We will implement and require features which give the user more visibility into the state of MIDI on the machine, even if those features aren't of obvious use to the applications themselves.
  * The user must explicitly enable any plugins
  * The configuration (setup) files are human-readable JSON, and easily accessible for viewing, editing, copying, or backup
* Unless otherwise stated, all API-level Ids are GUIDs
* All end-user accessible device, endpoint, etc. names support full unicode
* All API-consumed or delivered messages will be packaged in UMP, including MIDI 1.0
  * Helper classes will be provided to parse out strongly typed MIDI 1.0/2.0 messages
  * Helper classes will be provided to build a UMP from MIDI 1.0 or MIDI 2.0 strongly-typed messages

To level-set, here are some of the main objects/classes we're dealing with with messages. It's especially important to understand the relationship between devices, streams, and endpoints as this is different from the MIDI 1.0 APIs.

* Device: A MIDI device connected to the PC through a transport like USB, RTP, BLE, Virtual, etc.
* Endpoint: An addressible MIDI "port" on the device. In the case of MIDI 2.0. In the case of MIDI 1.0, these are uni-directional.
* Stream: An input or output buffer or stream of data coming from or going out of the endpoint. In MIDI 1.0, these were just bytes. But in MIDI 2.0, and all UMP-wrapped MIDI 1.0 messages, these are chunks of words as defined in the MIDI 2.0 specs. A stream has further subdivisions in it for channels and group/function blocks. However, the API does not do any processing at that level and instead leaves it up to applications to address different groups and channels.

![Main API Objects](img/main-api-objects.png)

## Session Management

Sessions are the entrypoint into the API. In addition to this, they provide the required user-facing diagnostic data of which apps have which sessions and endpoints open.

```cpp
// MidiSessionSettings is where we can set flags and other session-global
// settings. For example, we may decide to have flags to control the way
// messages are received (polling, events, etc.)
MidiSessionSettings settings

// session name can be a project in the DAW, a tab in a browser, etc.
// The process name / id etc. is also captured behind the scenes
_session = MidiSession.Create("My song", settings)

// do stuff with session in here
...

// closing the session closes all the open endpoints/devices
// Good practice, but should also happen automatically if session goes out of scope
_session.Close()
```

TODO: We want the user to be in control. So we may allow them the ability to terminate a session using the settings tool (with appropriate warnings). If we do that, there will be a session terminated notification sent to the app which owns the session.

For regular applications, only the app owning the session can terminate it. We will not have a public API endpoint for apps to terminate other sessions.

## Message Handling

We've had requests for both centralized message receiving as well as WinRT MIDI-style
port (endpoint)-based receiving. This API will provide both.

### Open endpoint, send message, close

We require opening the endpoint for sending messages so that there's an explicit action
that is also user-visible through diagnostic tools. Opening the endpoint is also
required for receiving messages. Finally, some protocols (like RTP MIDI 1.0 and presumably
the 2.0 version) need to take specific actions when an endpoint is opened or closed.

```cpp
MidiEndpointOpenOptions options
MidiMessageSendOptions messageOptions

_endpoint = _device.OpenEndpoint(deviceId, endpointId, options)

if (_endpoint)
{
    // there will be variations on this for larger buffers, 
    // timestamp options, etc.
    // Fails with an exception if endpoint has not been opened
    _endpoint.SendMessage(singleUmpMessageWithGroupAndChannelInIt, messageOptions)
    _endpoint.SendMessages(bunchOfCompleteUmpMessageStructs, messageOptions)
    _endpoint.SendMessages(arrayOrBufferOfUmpCompatibleWords, messageOptions)
}

_endpoint.Close()
```

TODO: As we decide on how much to offer at the session level, we should consider if
it makes sense to have SendMessage(s) functionality there, passing in either an
endpointId or an actual endpoint object. The latter isn't super useful because you
already have the instance, but the former may be.

### Receive messages at the endpoint-level

This is more WinRT-MIDI style where the messages show up at the endpoint in a delegate/event

```cpp
MidiEndpointOpenOptions options

_endpoint1 = _device.OpenEndpoint(deviceID1, endpointID1, options)
_endpoint2 = _device.OpenEndpoint(deviceID1, endpointID2, options)

// centralized message handling for all open input endpoints
_endpoint1.MessageReceived += OnMessageReceived

void OnMessageReceived(session, endpoint, messageReceivedArgs)
{
    foreach msg in MidiMessageParser.GetTypedMessages(messageReceivedArgs.Endpoint.InputStream)
    {
        ...
    }
}
```

TODO: Should we require the endpoint opening function be at the device level, or should it be
at the session level? This is a recurring theme throughout where some things may simply be
easier to manage if centralized, but that also means any given program ends up with either
a lot of parameter passing, or globals they may not want. It also makes cleaning up somewhat
more difficult when objects go out of scope.

TODO: Should endpoints in the API be 1:1 with group terminals in the driver? We go back and forth on this, but the USB MIDI Spec would seem to indicate that we really do need to in order to make sense of the devices and endpoints, and to surface them to applications which will want to address specific ports on the devices. But the groups are also contained in the UMP, which would be redundant if that were the case and would require the API to process each packet and route it to the correct API object. (see Section 3 of USB MIDI Class Driver spec)

### Receive messages at the session level

This has been requested by some of the developers so that they can centralize their own message processing

```cpp
_endpoint1 = _session.OpenEndpoint(deviceID1, endpointID1)

// centralized message handling for all open input endpoints
_endpoint1.MessageReceived += OnMessageReceived

void OnMessageReceived(session, messageReceivedArgs)
{
    foreach endpoint in (messageReceivedArgs.Endpoints)
    {
        foreach msg in MessageParser.GetTypedMessages(endpoint.InputStream)
        {
            ...
        }
    }
}
```

TODO: Polling? This has been requested, but need to see if event/delegate approach
is workable for them. Also need to sort out how often these events are raised, and
how many messages they are likely to contain.

## Enumeration

This is a different approach from Windows.Devices.Enumeration today. Reasons why:

1. We should just have all MIDI devices available all the time. There's no reason not to, and we have to have them available to listen for certain types of changes.
2. More easily support virtual and other locally-known MIDI devices that don't fit into PNP without creating custom drivers
3. Make more data available without messing with the lower-fidelity DeviceInformation structure in WinRT
4. Doesn't tie us to UWP enumeration code

### Enumerate devices

Devices are always available when connected. In some cases, we may need to track device changes (user renames the device in the settings app, or we receive a MIDI CI or other message which may impact device configuration)

```cpp
_session.DeviceRemoved += OnDeviceRemoved
_session.DeviceAdded += OnDeviceAdded
_session.DeviceChanged += OnDeviceChanged   // this is where any UMP change messages come through, too

// devices are always available as soon as the session is open
foreach device in _session.Devices
{
    ...
}
```

### Enumerate endpoints

Endpoints hang off devices and provide the actual MIDI IO. Additions/removals/changes to endpoints do not raise the DeviceChanged notification for the device they are part of.

```cpp
_device.EndpointRemoved += OnEndpointRemoved   // especially required for virtual endpoints
_device.EndpointAdded += OnEndpointAdded       // ditto
_device.EndpointChanged += OnEndpointChanged   // this is any where UMP change messages come through, too

foreach endpoint in _device.Endpoints
{

}
```

TODO: Should we also surface the endpoint events at the session level?

TODO: Should we also surface a collection of all enpoints at the session level, rather than require drilling down by device? Or maybe we provide a few WinUI controls or dialogs for device / endpoint picking?

## Virtual Devices

Virtual devices are unlike hardware devices in that they can be added at runtime by code. The implementation for virtual devices is through a device/transport plugin, not in the core API itself. This is consistent with how other device/transports will be implemented.

### Adding a virtual device

The API won't know about the plugins types, so API functions will work off of interfaces. The client
code can reference the plugin types through a metadata file

The user can create virtual devices and endpoints through the setings app (and facilitate automatic 
routing between them), but apps also need the ability to expose themselves as an endpoint at any time.

```cpp
// code-created virtual devices are scoped to the session, and disappear
// when the session is closed or goes out of scope.

MidiVirtualDeviceSettings deviceSettings        // type Supplied by plugin

_device = _session.AddDevice(guidOfVirtualDevicePlugin, nameOfDevice, (IMidiDeviceSettings)deviceSettings)

// VirtualEndpointSettings will contain things such as toggles for InputEnabled
// OutputEnabled, etc.

MidiVirtualEndpointSettings endpointSettings    // type Supplied by plugin

_endpoint = _device.AddEndpoint((IMidiEndpointSettings)endpointSettings)
```

## MIDI-CI

TODO

### Discovery and Protocol Negotiation

Endpoint instances have a user-settable flag to enable/disable protocol negotiation. This is there in case the user knows the endpoint will never support MIDI 2.0, or in case protocol negotiation somehow breaks an existing device, or takes too long to start up, or just in case the user only wants that device to be visible to Windows as a MIDI 1.0 device.

MIDI CI discovery and protocol negotiation will only be attempted on Bi-directional endpoints.

Authority level of MIDI CI messages initiatied by the API/driver shall default to the highest level (0x60-0x6F)
* This should be end-user configurable in the settings app, just in case

TODO: Decide when all this happens. For the user, on startup could be ideal, except that could greatly delay startup, which is against Windows implementation guidelines. It could happen on the first time any app uses the MIDI API, but that will create a delay at that point. It could also be something which happens as a delayed startup in Windows, but that's not ideal in case the user goes right into their DAW.

Discovery and protocol negotiation are implemented internally, so need no API other than exposing which protocol was negotiated

```cpp

// no application-level APIs for discovery or protocol negotiation
// however, properties are exposed
if (_endpoint.ProtocolType == 1)
    // MIDI 1.0
else if (_endpoint.ProtocolType == 2)
    // MIDI 2.0

```

There are other properties set by CI which will be useful for the endpoints. It may be that the endpoint itself contains only the raw capability inquiry bitmap, and that the flags are interpreted by a helper class, like what we're doing for message parsing. This keeps the core API lean.

```cpp
if (_endpoint.ProtocolNegotiationSupported)     // useful only for debugging.
if (_endpoint.ProfileConfigurationSupported)    // will be used by apps. Set by discovery/capability inquiry.
if (_endpoint.PropertyExchangeSupported)        // will be used by apps. Set by discovery/capability inquiry.

_endpoint.RawCapabilityInquiryResult            // the raw result
```

### Property Exchange

We won't store or otherwise deal with properties in the OS, but we can provide code to handle the transaction. We may also provide helper classes to parse property information. TBD.

TODO: Also need to provide a way to deal with subscriptions to property changes through MIDI CI 8.12

### Profiles

We won't store or otherwise deal with profiles in the OS, but we can provide code to handle the transaction. We may also provide helper classes to parse the results. TBD.

### Other messages not handled within the API

These are messages which will be handled by the code interfacing with the API (DAW, plugin, etc.)

(properties etc.)

## Setups

Setups are saved JSON configurations created by the user in the settings app. They contain custom
device and endpoint names, virtual ports, routing, plugin settings and more. Creating or changing
them via code was considered, but rejected due to end-user control and potential security concerns.

We will revisit if new requirements arise.

## Plug-ins

```cpp
// interface supported by all plug-ins
interface IMidiPlugin
{
    string PluginName       // Name of the plug-in
    string PluginProvider   // Name of the provider for this plug-in
    string Version          // version string for this plug-in
    //TODO: Icon resource/image
}
```

### Device / Transport / Endpoint Plug-ins

TODO. **This section isn't quite thought through yet and so may change significantly**. Intent is to allow transports to be defined as plugins so Network, BLE, and more can be implemented from user code in the simplest possible way, without the ceremony required of a full driver. USB is also built using this, with the driver behind it.

```cpp
// Transport vs Device is still being sorted out. Logically, they are the same thing.
interface IMidiTransport
{
    string TransportType                // BLE, RTP, Virtual, etc.
    string TransportGlyph               // glyph for the type of transport like BLE, RTP, Virtual, etc.

    // TODO: Actual sending implementation?
}

interface IMidiDevice
{
    string Id
    string Name                         // Name of the device provided by the user
    string DeviceSuppliedName           // name of the device provided by the device
    string Serial                       // for devices which have a serial number
    string IconPath

    string DriverDescription            // driver won't apply to most devices. Reconsider
    string DriverProvider               // ways to provide this information when available
    string DriverVersion                // and ideally strongly typed (not a prop bag)
    string DriverDate

    string DeviceLocationInformation
    string DeviceManufacturer
    string DeviceInstancePath

    IMidiTransport Transport

    List<IMidiEndpoint> Endpoints
} 

// base for all endpoint types
interface IMidiEndpoint
{
    string Id                   // GUID
    string Name                 // Name of the endpoint provided by the user
    string DeviceSuppliedName   // name of the enpoint provided by the device
}

// unidirectional endpoints end up being MIDI 1.0 only
interface IMidiInputEndpoint : IMidiEndpoint
{  
    MidiStream InputStream
}

// unidirectional endpoints end up being MIDI 1.0 only
interface IMidiOutputEndpoint : IMidiEndpoint
{  
    MidiStream OutputStream

    uint SizeOfPacket                   // see protocol negotiation spec. This is a protocol extension
    bool UseJitterReductionTimeStamps   // ditto
}

// Bi-directional endpoints could be MIDI 1.0 or MIDI 2.0
interface IMidiBiDirectionalEndpoint : IMidiEndpoint
{
    bool ProtocolNegotiationSupported     // can be overridden by user in configuration settings for this endpoint
    bool ProfileConfigurationSupported    // will be used by apps. Set by discovery/capability inquiry or overridden by user.
    bool PropertyExchangeSupported        // will be used by apps. Set by discovery/capability inquiry or overridden by user.

    uint ProtocolType                   // 1 or 2 See MIDI CI protocol negotiation spec
    uint ProtocolVersion                // 0x00 in all cases so far. See MIDI CI protocol negotiation spec

    MidiStream InputStream
    MidiStream OutputStream

    uint SizeOfPacket                   // see protocol negotiation spec. This is a protocol extension
    bool UseJitterReductionTimeStamps   // ditto
}


interface IMidiDeviceSettings
{
    string GetUIJson()
}

interface IMidiEndpointSettings
{
    uint ProtocolVersionMajor
    uint ProtocolVersionMinor

    string GetUIJson()
}
```

### Processing Plug-ins

TODO: Plug-ins for message processing. Also not fully thought through yet.

```cpp

// for processing a single message. We may need to provide
// an interface which supports processing multiple messages
// or a stream, or supports transforming the number of
// messages so that it is more or less than what was passed
// in. Those may require just passing in a stream reference
// instead, which has some threading implications
interface IMidiIncomingMessageProcessor
{
    // These return true if the message should be passed along
    // and false if they should be removed from the stream
    bool ProcessMessage(Ump32* messageToProcess)
    bool ProcessMessage(Ump64* messageToProcess)
    bool ProcessMessage(Ump96* messageToProcess)
    bool ProcessMessage(Ump128* messageToProcess)
}

interface IMidiOutgoingMessageProcessor
{
    // These return true if the message should be passed along
    // and false if they should be removed from the stream
    bool ProcessMessage(Ump32* messageToProcess)
    bool ProcessMessage(Ump64* messageToProcess)
    bool ProcessMessage(Ump96* messageToProcess)
    bool ProcessMessage(Ump128* messageToProcess)
}

```

Possible incoming messages workflow:

* Driver supplies stream to service
* Service gets stream ready to send to each listening app
  * If there are incoming message processors attached to incoming endpoint, run them in order
  * It may be necessary, if messages are removed/added, to have a second "post-process" stream which is then sent to apps
  * If there are no message processors, can maybe optimize to have only a single stream shared with apps in that case.
  * The above really depends on performance of creating a postprocess stream in all cases vs just when there are processors

Possible outgoiing messages workflow:

* App writes UMPs to stream
* Service gets stream ready to send to endpoints
  * If there are any outgoing message processors attached to output endpoint, run them in order
  * Same caveats about stream copies as above
* Service sends stream to endpoints

TBD: Who is responsible for adding anti-jitter timestamps? Presumably the service will need to do this, but that requires inserting a number of messages into a copy of the stream

TBD: This model won't support time-based continuous processing, like an arpeggiator MIDI effect and similar. The overhead of something like that as a plug-in may be too high. It may be better as an app on a virtual endpoint, or simply left to dedicted DAWs and performance effects.

### Routing Plug-ins

TBD. It may be more scalable to handle this through virtual MIDI endpoints. That's how I'm currently thinking of this.
