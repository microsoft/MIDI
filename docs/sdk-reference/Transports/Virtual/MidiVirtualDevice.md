---
layout: sdk_reference_page
title: MidiVirtualDevice
namespace: Windows.Devices.Midi2.Transports.Virtual
type: runtimeclass
implements: IMidiEndpointMessageProcessingPlugin
description: Represents a virtual device in app-to-app MIDI
---

A virtual device app uses this class to work with the virtual device it defined. Create one with `MidiVirtualDeviceManager`.

## Properties

| Property | Description |
| --------------- | ----------- |
| `DeviceEndpointDeviceId` | The endpoint device id that the app that created the virtual device uses to connect to it |
| `AssociationId` | The id that links the client endpoint and the device endpoint |
| `FunctionBlocks` | The device's function blocks right now |
| `IsClientEndpointInUse` | True when one or more apps are connected to the endpoint that other apps see for this device. You can read it at any time, even before any app has connected |
| `CapabilityInquiry` | Answers MIDI Capability Inquiry for this device, the same way this class already answers endpoint discovery. It does nothing until your app turns it on and gives it something to share. See [`MidiCapabilityInquiryDeviceResponder`]({{ site.baseurl }}/sdk-reference/CapabilityInquiry/MidiCapabilityInquiryDeviceResponder/) |
| `SuppressHandledMessages` | **True by default.** When true, the endpoint discovery and stream configuration messages that this class answers are removed from the incoming messages, so your app doesn't have to filter out protocol messages it never asked for. Set it to false if you want to see those messages yourself, for example to debug how a client does discovery. The virtual device still answers them either way |

## Methods

| Method | Description |
| --------------- | ----------- |
| `UpdateFunctionBlock(block)` | Changes the properties of one function block. The UMP specification says the number of function blocks can't change after the device is created, but you can mark blocks active or inactive. A change here sends out the MIDI 2.0 function block notification messages. Returns true if it worked |
| `UpdateEndpointName(name)` | Changes the endpoint name, and sends out the endpoint name notification messages. Returns true if it worked |

## Events

This class is a message processing plugin, so incoming messages wait for your `StreamConfigRequestReceived` handler to finish. Keep your handler fast.

Applications are usually much faster than devices. But if your handler can't keep up, the incoming message queue can fill up and cause errors. MIDI 2.0 has no speed limit for devices, and USB 3 and network devices, among others, can send a lot of messages in a very short time.

If you need to do slow work with incoming messages, copy them to your own queue and process them on another thread. Still, send the protocol negotiation answer right away, as the UMP specification says.

| Event | Description |
| --------------- | ----------- |
| `StreamConfigRequestReceived(device, args)` | Raised when this device gets a Stream Configuration Request message. Your app should answer as the UMP MIDI 2.0 protocol negotiation specification describes |
| `ClientEndpointInUseChanged(device, args)` | Raised when an app connects to, or disconnects from, the endpoint other apps see for this device. See `MidiVirtualDeviceClientEndpointInUseChangedEventArgs` |

## Remarks

`IsClientEndpointInUse` and `ClientEndpointInUseChanged` tell you whether *any* app is connected, not how many. The service keeps one connection to the transport for each endpoint, however many apps are using it. So you can only see the change from no apps to some apps, and back. Ten apps connecting raise one event, not ten.

The event comes from a device property change, so expect a short delay. Because you can read the property at any time, an app that starts late or misses an event can just read `IsClientEndpointInUse`, instead of waiting for the next change.

When the virtual device is removed, `IsClientEndpointInUse` goes back to false, and no more events are raised. With an older service that doesn't report this, `IsClientEndpointInUse` stays false and the event is never raised. You won't get wrong values.

## Examples

* [C++ Sample](https://github.com/microsoft/MIDI/tree/main/samples/cpp-winrt/simple-app-to-app-midi)
* [C# Sample](https://github.com/microsoft/MIDI/tree/main/samples/csharp-net/virtual-device-app-winui)
* [C++ Sample: answering capability inquiry](https://github.com/microsoft/MIDI/tree/main/samples/cpp-winrt/capability-inquiry-virtual-device)
* [C# Sample: answering capability inquiry](https://github.com/microsoft/MIDI/tree/main/samples/csharp-net/capability-inquiry-virtual-device)
