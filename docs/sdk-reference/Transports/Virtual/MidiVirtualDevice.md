---
layout: sdk_reference_page
title: MidiVirtualDevice
namespace: Windows.Devices.Midi2.Transports.Virtual
type: runtimeclass
implements: IMidiEndpointMessageProcessingPlugin
description: Represents a virtual device in app-to-app MIDI
---

This is the class that a virtual device application uses as its interface to the virtual device it has defined. Use the `MidiVirtualDeviceManager` to construct an instance of this type.

## Properties

| Property | Description |
| --------------- | ----------- |
| `DeviceEndpointDeviceId` | The EndpointDeviceId to be used by the app creating the virtual device |
| `AssociationId` | The id used to associate the client and device endpoints |
| `FunctionBlocks` | Current list of function blocks for this device. |
| `IsClientEndpointInUse` | True when one or more applications are connected to this device's client-visible endpoint. Readable at any time, including before any client has ever connected. |
| `SuppressHandledMessages` | True if the protocol messages handled by this class should be filtered out of the incoming message stream |

## Functions

| Function | Description |
| --------------- | ----------- |
| `UpdateFunctionBlock` | Update the properties of a single function block. The number of actual function blocks cannot change after creation (per the UMP specification) but blocks may be marked as active or inactive. Changes here will result in the MIDI 2.0 function block notification messages being sent out. |
| `UpdateEndpointName` | Update the endpoint name, and send out the appropriate endpoint name notification messages. |

## Events

This class is a message processing plugin, so `StreamConfigRequestReceived` is raised synchronously on the incoming message path and needs to be handled quickly and efficiently by the calling application.

Applications are typically much faster than devices at handling messages. However, failing to drain the incoming message queue fast enough can result in transmission errors. With MIDI 2.0 there is no upper performance limit on devices, and USB 3 and Network MIDI devices, among others, are capable of transmitting a large number of messages in a very short period of time.

If you need to do long-running processing of incoming messages, add them to your own incoming queue and have them processed by another application thread. Note that the protocol negotiation response itself should still be sent promptly, per the UMP specification.

| Event | Description |
| --------------- | ----------- |
| `StreamConfigRequestReceived (device, args)` | Raised when this device receives a Stream Configuration Request UMP message. The virtual device application should respond per the UMP MIDI 2.0 protocol negotiation specification. |
| `ClientEndpointInUseChanged (device, args)` | Raised when an application connects to, or disconnects from, this device's client-visible endpoint. See `MidiVirtualDeviceClientEndpointInUseChangedEventArgs`. |

## Remarks

`IsClientEndpointInUse` and `ClientEndpointInUseChanged` report whether *any* application is connected, not how many. The service maintains a single connection to the transport for each endpoint regardless of how many applications are attached, so only the transitions from none-connected to some-connected, and back, are observable. Ten applications connecting raise one event, not ten.

The event is raised through device property change notification, so expect a short delay rather than immediate delivery. Because the property is readable at any time, an application which starts late or misses an event can simply read `IsClientEndpointInUse` instead of waiting for the next change.

When the virtual device is torn down, `IsClientEndpointInUse` returns to false and no further events are raised. Applications running against an older service which does not report this information will see `IsClientEndpointInUse` remain false and the event never raise, rather than receiving incorrect values.

## Examples

[C++ Sample](https://github.com/microsoft/MIDI/blob/main/samples/cpp-winrt/simple-app-to-app-midi/main.cpp)
[C# Sample](https://github.com/microsoft/MIDI/tree/main/samples/csharp-net/virtual-device-app-winui)
