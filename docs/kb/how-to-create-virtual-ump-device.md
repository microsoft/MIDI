---
layout: kb
title: How to Create Virtual Devices from Code
audience: developers
description: How to create a virtual UMP device (app-to-app MIDI) from code
---

# How to Create Virtual Devices at Runtime

If you develop an application which should appear as a new MIDI device to other applications on Windows, you want to create a Virtual UMP Device. Your app may be a controller app, a sound generator/synthesizer, or a bridge to accessibility or other controllers. Anything a hardware MIDI device can do is open to you here.

## How Virtual Devices work

A virtual device enables an application to appear as a UMP Endpoint to other applications.

[More information on how Virtual Devices work may be found here](../endpoints/virtual-device-app.html).

## Steps to Create a Virtual Device

1. [Check for and bootstrap Windows MIDI Services](./how-to-check-for-windows-midi-services.html)
2. Create a MIDI session
3. Define the Virtual MIDI Device, its function blocks, and other properties
4. Create the Device and get the `EndpointDeviceId` for the device-side application endpoint
5. Connect to the Device like any other MIDI connection
6. Wire up event handlers for message received and optionally for stream configuration
7. Open the connection
8. Respond to any protocol negotiation or message received events
9. When the application no longer needs to expose the virtual device, close the connection.

On the service-side, the Virtual Device works like any other native UMP MIDI 2.0 device, including for endpoint metadata capture and protocol negotiation.

## Code

We'll assume you've already performed the [Windows MIDI Services bootstrapping steps](./how-to-check-for-windows-midi-services.html).

The first step is to define the virtual device by creating the different metadata declarations and then assemble them together using the `MidiVirtualDeviceCreationConfig` type. 

This information is all required so that the virtual device responder can handle the MIDI 2.0 endpoint discovery and protocol negotiation messages on your behalf. This removes the complexity of message parsing and (in the case of names and ids) message assembly.

> When creating the device's software device id (SWD) only the first 32 characters of the `ProductInstanceId` are used. This must be unique among all **virtual UMP devices** currently running in Windows MIDI Services, or else the device creation will fail. One recommendation for uniqueness is to use a GUID with all non-alphanumeric characters removed. Another would be to use the app name and an internal index or differentiator. 

```cpp
// endpoint information returned from endpoint discovery
midi2::MidiDeclaredEndpointInfo declaredEndpointInfo{ };
declaredEndpointInfo.Name = endpointSuppliedName;
declaredEndpointInfo.ProductInstanceId = L"PMB_APP2_3263827";   // must be unique
declaredEndpointInfo.SpecificationVersionMajor = 1; // see latest MIDI 2 UMP spec
declaredEndpointInfo.SpecificationVersionMinor = 1; // see latest MIDI 2 UMP spec
declaredEndpointInfo.SupportsMidi10Protocol = true;
declaredEndpointInfo.SupportsMidi20Protocol = true;
declaredEndpointInfo.SupportsReceivingJitterReductionTimestamps = false;
declaredEndpointInfo.SupportsSendingJitterReductionTimestamps = false;
declaredEndpointInfo.HasStaticFunctionBlocks = true;

midi2::MidiDeclaredDeviceIdentity declaredDeviceIdentity{ };
// todo: set any device identity values if you want. This is optional

midi2::MidiEndpointUserSuppliedInfo userSuppliedInfo{ };
userSuppliedInfo.Name = userSuppliedName;           // for names, this will bubble to the top in priority
userSuppliedInfo.Description = userSuppliedDescription;

// create the config type to aggregate all this info
virt::MidiVirtualDeviceCreationConfig config(
    transportSuppliedName,                          // this could be a different "transport-supplied" name value here
    transportSuppliedDescription,                   // transport-supplied description
    transportSuppliedManufacturerName,              // transport-supplied company name
    declaredEndpointInfo,                           // for endpoint discovery
    declaredDeviceIdentity,                         // for endpoint discovery
    userSuppliedInfo
);
```

We're not quite done yet, however. The config type is also where you'll set function blocks. At least one function block is needed.

```cpp
// Function blocks. The MIDI 2 UMP specification covers the meanings of these values
midi2::MidiFunctionBlock block1{ };
block1.Number(0);
block1.Name(L"Pads Output");
block1.IsActive(true);
block1.UIHint(midi2::MidiFunctionBlockUIHint::Sender);
block1.FirstGroupIndex(0);
block1.GroupCount(1);
block1.Direction(midi2::MidiFunctionBlockDirection::Bidirectional);
block1.RepresentsMidi10Connection(midi2::MidiFunctionBlockRepresentsMidi10Connection::Not10);
block1.MaxSystemExclusive8Streams(0);
block1.MidiCIMessageVersionFormat(0);

config.FunctionBlocks().Append(block1);

midi2::MidiFunctionBlock block2{ };
block2.Number(1);
block2.Name(L"A Function Block");
block2.IsActive(true);
block2.UIHint(midi2::MidiFunctionBlockUIHint::Sender);
block2.FirstGroupIndex(1);
block2.GroupCount(2);
block2.Direction(midi2::MidiFunctionBlockDirection::Bidirectional);
block2.RepresentsMidi10Connection(midi2::MidiFunctionBlockRepresentsMidi10Connection::Not10);
block2.MaxSystemExclusive8Streams(0);
block2.MidiCIMessageVersionFormat(0);

config.FunctionBlocks().Append(block2);
```

Now, the virtual device is fully defined. The next step is to open a session and then actually create the device in the service.

```cpp
// create the session. The name here is just convenience.
m_session = midi2::MidiSession::Create(config.Name());

if (m_session == nullptr) return; // return if unable to create session

// create the virtual device, so we can get the endpoint device id to connect to
m_virtualDevice = virt::MidiVirtualDeviceManager::CreateVirtualDevice(config);

if (m_virtualDevice == nullptr) return; // return if unable to create virtual device

// create the endpoint connection to the device-side endpoint
// to prevent confusion, this endpoint is not enumerated to 
// apps when using the standard set of enumeration filters
m_connection = m_session.CreateEndpointConnection(
    m_virtualDevice.DeviceEndpointDeviceId());

// add the virtual device as a message processing plugin so it receives the messages
m_connection.AddMessageProcessingPlugin(m_virtualDevice);

// wire up the stream configuration request received handler
auto streamEventToken = m_virtualDevice.StreamConfigRequestReceived(
    { this, &MainWindow::OnStreamConfigurationRequestReceived });

// wire up the message received handler on the connection itself
auto messageEventToken = m_connection.MessageReceived(
    { this, &MainWindow::OnMidiMessageReceived });

// the client-side endpoint will become visible to other apps once Open() completes
m_connection.Open();
```

From there, you may send and receive messages just like with any other endpoint.

## Troubleshooting

What can cause a failure in virtual device creation? Assuming the service is installed and working properly, the main thing to check will be to ensure that the unique Id provided is actually unique. The unique Id is used as the differentiator in the SWD Id, without any additional hashing or obfuscation, so it must be unique among all virtual devices currently running. When in doubt, one practice to ensure uniqueness is to use a GUID by formatting as string and removing all non alpha-numeric characters. The unique Id is just large enough to hold that string.

## Sample Code

* [C++ WinUI Sample](https://github.com/microsoft/MIDI/tree/main/samples/cpp-winrt/virtual-device-app-winui)
* [C# WinUI Sample](https://github.com/microsoft/MIDI/tree/main/samples/csharp-net/virtual-device-app-winui)
