# API Specifications: Virtual MIDI Plug-in

=======================================================
TODO: This needs to be updated after the SDK changes
=======================================================


These are not final designs, but rather are brainstorming design ideas. All code is pseudo-c code with just enough syntax to get the point across. You will notice a real lack of semicolons, for example. :)

Virtual MIDI is implemented as another Device + Endpoint combination.

Virtual devices are unlike hardware devices in that they can be added at runtime by code. The implementation for virtual devices is through a device/transport plugin, not in the core API itself. This is consistent with how other device/transports will be implemented.

## Using a virtual device

API functions work off of interfaces. To use a virtual device, the client code does not need to know the device/endpoints are virtual. Instead, they are used like any other device/endpoint.

## Adding a virtual device from code

The API won't know about the plugin types, so API functions will work off of interfaces. The client
code can reference the plugin types through a metadata file

The user can create virtual devices and endpoints through the setings app (and facilitate automatic 
routing between them), but apps also need the ability to expose themselves as an endpoint at any time.

```cpp
// code-created virtual devices are scoped to the session, and 
// disappear when the session is closed or goes out of scope.

MidiVirtualDeviceSettings deviceSettings        // type Supplied by plugin

_device = _session.AddDevice(typeOfPlugin, (IMidiDeviceSettings)deviceSettings)

// VirtualEndpointSettings will contain things such as toggles for InputEnabled
// OutputEnabled, etc.

MidiVirtualEndpointSettings endpointSettings    // type Supplied by plugin

_endpoint = _device.AddEndpoint((IMidiEndpointSettings)endpointSettings)
```

The settings for the endpoint are particularly important here.

```cpp
// TODO: Settings the app will need to provide when creating the endpoint
class MidiVirtualEndpointSettings
{
    // TODO: Protocol support
    // TODO: Groups
    // TODO: MIDI CI information
    // TODO: Endpoint name
    // TODO: Type of streams (in, out, both)
    // etc.
}
```

Device settings

```cpp
// TODO: Settings the app will need to provide when creating the device
class MidiVirtualDeviceSettings
{
    // TODO: Device name
    // TODO: ANy other metadata
}
```

TODO

## Adding a virtual device through the settings app

TODO: UI specs
