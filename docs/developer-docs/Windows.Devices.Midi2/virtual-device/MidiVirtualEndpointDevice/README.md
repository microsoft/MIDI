# MidiVirtualEndpointDevice

Fully-featured app-to-app MIDI in a MIDI 2.0 world involves connections to a virtual device which must participate in the full MIDI 2.0 protocol, from discovery through protocol negotiation. To support this scenario, the way app-to-app MIDI works in Windows MIDI Services is for an application to define a device and then using the `MidiSession`, construct that device's endpoint. Once the device endpoint is opened, Windows MIDI Services will then construct a second application-visible multi-client endpoint which applications will use to talk to the device app. 

During that conversation, the service will also handle discovery and protocol negotiation with the virtual device just like it would any physical device. 

The `MidiVirtualEndpointDeviceDefinition` class specifies, in an easy to use format, the responses for discovery and protocol negotiation, as well as the properties to use when constructing the device endpoint.

## Properties

| Property | Description |
| --------------- | ----------- |
| DeviceDefinition | The MidiVirtualEndpointDeviceDefinition used to create this device. This should be treated as read-only data. |
| FunctionBlocks | Current list of function blocks for this device. |
| SuppressHandledMessages | True if the protocol messages handled by this class should be filtered out of the incoming message stream |

## Functions

| Function | Description |
| --------------- | ----------- |
| UpdateFunctionBlock | Update the properties of a single function block. The number of actual function blocks cannot change after creation (per the UMP specification) but blocks may be marked as active or inactive. Changes here will result in the MIDI 2.0 function block notification messages being sent out. |
| UpdateEndpointName | Update the endpoint name, and send out the appropriate endpoint name notification messages. |

## Events

| Event | Description |
| --------------- | ----------- |
| StreamConfigurationRequestReceived(device, args) | Raised when this device receives a Stream Configuration Request UMP message. |

## IDL

[MidiVirtualEndpointDevice IDL](https://github.com/microsoft/MIDI/blob/main/src/api/Client/Midi2Client/MidiVirtualEndpointDevice.idl)
