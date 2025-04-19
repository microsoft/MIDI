---
layout: doc
title: Windows MIDI Services Overview
---

Windows MIDI Services is the new MIDI API, Service, and SDK in Microsoft Windows. It's delivered in two parts:
- In-box MIDI Service and plugins, the new MIDI 2.0 kernel driver, plus backwards-compatibility support for WinMM and WinRT MIDI 1.0 APIs. You get this when you install Windows.
- An out-of-band shipped SDK Runtime and Tools package. You need to download and install this yourself.
We ship the runtime and tools out-of-band to enable us to move quickly and continue providing new features and capabilities for customers and apps.

## Changes from the past

In the past, the Windows APIs for MIDI would connect a single application directly with the driver for the MIDI device being used. Although this made a lot of sense when first developed around the turn of the century, it introduced a lot of limitations, including the inability for more than one application to use a device without special drivers.

The new approach uses a Windows Service (`midisrv.exe` -- the "MIDI Service") as the mediator between the devices and the applications. Any number of applications can talk to any number of devices, and devices may be connected to by more than one application without any changes to their drivers. This is because each device still has only one client: the MIDI Service.

This gives us the flexibility to have more processing inside the service so that we can support protocol data capture, translation, message scheduling, and more in the service today, and custom routing and message manipulation in the future. It also enables us to seamlessly integrate MIDI 1.0 and MIDI 2.0 because we handle all translation in the service.

### Service Plugins: Transports

The service supports the concept of a Transport plugin. This enables us to support new transports, like Network MIDI 2.0, without having to create kernel device drivers. 

At the time of this writing, we ship the following transports in the box:
- **USB MIDI 1.0/2.0 UMP (KS)** for any USB device connected to the USB MIDI 2.0 UMP driver
- **USB MIDI 1.0 byte format (KSA)** for any MIDI 1.0 device using our in-box USB MIDI 1.0 class driver, or a third-party driver
- **Diagnostics / Testing Loopback and Ping (DIAG)** for use when you are checking the health of the system, or on a call with device or app support
- **Simple loopback (LOOP)** for simply connecting two apps
- **MIDI 2.0 device app (APP)** for app-to-app MIDI with MIDI 2.0 discovery and protocol negotiation

In the future, we have plans for BLE MIDI 1.0, Network MIDI 2.0 (demonstrated at NAMM Show 2025), a virtual patch bay / router, and others.

If Developer Mode is not turned on in Windows Settings, all service plugins must be signed with a valid certificate by an authority that is recognized on the PC.

### Service Plugins: Transforms

Another type of service plugin is the Message Transform. These plugins take in a message, do something with it, and optionally pass it along to the next step. 

One of the biggest ways we use this in the service is for message translation. We translate between MIDI 1.0 byte format and MIDI Universal MIDI Packet (UMP) -- inside the service, all messages are UMP. We also use this to scale up/down values between the high resolution MIDI 2.0 protocol and MIDI 1.0.

Another way we use this is for message scheduling. The outgoing MIDI Message scheduler is implemented as a Transform. It takes incoming messages, looks at the timestamp, and either sends them along immediately, or adds them to an outgoing message queue.

We have plans for making these plugins open and configurable in the future, like we have for Transports. That way we can add more plugins that can do interesting things with messages, much like the old MIDI Mapper. That is not yet enabled.

## New driver and communication mechanism

For the new MIDI 2.0 UMP driver we took a page (ha!) from WaveRT and use a cross-process buffer approach to communicate with the service. This makes data transfer faster than the approach used with MIDI 1.0 drivers.

The new driver supports both MIDI 2.0 devices as well as MIDI 1.0 devices. By default, we do not currently move existing MIDI 1.0 devices to the new driver because we need to be sure of compatibility first. However, a customer is free to do this manually today, or through tools we provide in the future.

Some devices, due to how their USB descriptors are set up, will automatically move to the new UMP driver. This is not a large percentage of devices, but includes ones like the Korg nanoKey and family, and the Conductive Labs MRCC.

### Vendor drivers are usually not needed

If your USB MIDI device is class-compliant (that is, it will work with devices like the iPad with no driver required) then you probably do not need to install a driver on Windows. We encourage you to use the in-box drivers whenever possible to reduce the complexity of your setup.

Devices which are not class-compliant will still need their own drivers. We support those just as we do the in-box MIDI 1.0 class driver.

## Backwards compatibility

In short, to ensure that everything plays together nicely, we have replumbed the existing WinMM MIDI 1.0 and WinRT MIDI 1.0 APIs to talk to the new MIDI Service instead of talking directly to device drivers. This ensures apps using those APIs do not lock ports and instead participate in multi-client use, and also that they can use ports created from new MIDI 2.0 endpoints.

Please see the [knowledge base]({{"/kb/" | relative_url}}) for more information about backwards compatibility. 

## SDK Reference for Software Developers

As mentioned at the top, we ship Windows MIDI Services in two parts. The part that ships in-box supports MIDI 1.0 in-box APIs. The service, transports, and plumbing are all there for MIDI 2.0, however apps will need to use the Windows MIDI Services App SDK to interface with it. The existing WinMM and WinRT MIDI 1.0 APIs cannot support the newer constructs, message scheduling, creating virtual device apps, etc. All of those features are in the new SDK.

- Get an [overview of the SDK here]({{ "/sdk-reference/" | relative_url }}).
- The namespaces and types are [documented here]({{ "/sdk-reference/" | relative_url }}).
