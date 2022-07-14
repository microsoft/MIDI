# API Specifications

These are not final designs, but rather are brainstorming design ideas. All code is pseudo-c code with just enough syntax to get the point across. You will notice a real lack of semicolons, for example. :)



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
