# API Specifications: Devices and Endpoints

=======================================================
TODO: This needs to be updated after the SDK changes
=======================================================

These are not final designs, but rather are brainstorming design ideas. All code is pseudo-c code with just enough syntax to get the point across. You will notice a real lack of semicolons, for example. :)

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
