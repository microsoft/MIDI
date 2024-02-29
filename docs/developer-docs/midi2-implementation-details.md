---
layout: page
title: MIDI 2.0 Implementation Details
parent: Windows MIDI Services
has_children: false
---

# Implementation Details

Specifications can be funny. As much as the MIDI Association, and all of us in it, try to be very specific and crisp on wording, there's often room for interpretation. Most of these we work out among the various OS companies under the umbrella of the MIDI Association. But there are others were an approach may just not make sense on one OS or the other. Here are the ones that are Windows-specific, that you should be aware of as a developer.

Of course, the full source code for Windows MIDI Services, including the USB MIDI 2.0 driver, is available in our repo, so you can review it at any time to better understand how a feature or function works.

## Discovery and Protocol Negotiation

Windows MIDI Services supports only the UMP-based Endpoint Discovery and Protocol Negotiation. We do not implement the deprecated MIDI-CI equivalents.

## JR Timestamps

The hardware manufacturers, AMEI, and the MIDI Association agreed that JR Timestamp handling in the operating systems is not needed at this time (and may not be needed for many years). As a result, we do not have JR Timestamp handling built in.

When (if) it is needed, we will implement clock generation, incoming timestamp generation, and outgoing timestamp/clock message creation in the service itself. Client applications should not send JR timestamps now or in the future.

## Message Translation

Internally in Windows MIDI Services, all messages are UMP messages, whether they come from a MIDI 1.0 byte stream data format device, a UMP-native device or API, or a MIDI 1.0 classic API. This requires Windows to perform message data format translation in some cases.

Windows MIDI Services performs required message data format translation in these cases:

- When a classic (WinMM or WinRT) MIDI 1.0 API accesses a UMP device. In those cases, the UMP messages need to be translated to MIDI 1.0 protocol and MIDI 1.0 byte stream data format when being sent to the classic API. The reverse is also true here. This is done in the Windows Service.
- When a MIDI 1.0 byte stream data format device is connected to the new combined MIDI 1.0/MIDI 2.0 class driver, we will translate between MIDI 1.0 protocol UMP data format and MIDI 1.0 protocol byte stream data format as required to communicate with the device. This is done in the new driver.
- When a MIDI 1.0 driver is used with a MIDI 1.0 device (third-party USB, BLE MIDI 1.0, etc.), we translate data coming from and going to that driver in the Windows service.

Windows MIDI Services does not translate messages in these scenarios:

- When a native UMP device is connected to the new driver, all messages are simply a pass-through to the API. The service does not do any translation between MIDI 1.0 protocol in UMP and MIDI 2.0 protocol in UMP, regardless of the negotiated stream protocol. Applications should instead inspect the properties of the device using our enumeration classes, and constrain sent messages appropriately.
- When a native UMP endpoint is connected to the service through another transport (like Network MIDI 2.0), we do not translate the protocol of any messages.

> If a UMP-native device does not properly participate in discovery and protocol negotiation, we report it as a MIDI 2.0 protocol device.

## UMP Endpoint Names for native MIDI 2.0 UMP format devices

Although we make all the names available through the Enumeration API, we have an order of precedence we use when providing the recommended `Name` property value. In order from most preferred to least, we have:

1. Any user-supplied endpoint name configured through the configuration files (these will be created by the MIDI Settings app in the future)
2. The name supplied through in-protocol Endpoint Name Notification messages
3. The name supplied by the transport plugin in the service. This is typically pulled from a device name supplied by the driver, or other transport-specific sources such as network advertising in the case of Network MIDI 2.0.

When we create MIDI 1.0-compatible "ports" for these endpoints, we'll use the Function Block Names if available and Group Terminal Block names if not.

## UMP Endpoint Names for MIDI 1.0 byte stream format devices

The API also creates UMP endpoints for MIDI 1.0 devices. This happens two ways:

1. If the device is assigned to the USB MIDI 2.0 driver (this is preferred) the driver creates Group Terminal Blocks for each "cable" (a "port" in MIDI 1.0 API speak). In the new driver, we use the `iJack` names, if provided, to name the Group Terminal Blocks. This is the best way to ensure your endpoint and Group Terminal Block names are correct.
2. If the device is assigned a third-party driver or the legacy MIDI 1.0 driver (not preferred in most cases), the service creates the Group Terminal Blocks using the same algorithm. However, because much less information is available to the service from the legacy drivers, the name may not be identical.

The precedence for naming is the same as with MIDI 2.0 devices, with the exception of the Endpoint Name Notification, which doesn't exist in MIDI 1.0.

1. Any user-supplied endpoint name
2. The name supplied through in-protocol Endpoint Name Notification messages
3. The name supplied by the transport plugin in the service. This is typically pulled from a device name supplied by the driver, or other transport-specific sources such as network advertising in the case of Network MIDI 2.0.

## iSerialNumber Really Helps

If your device exposes a unique iSerialNumber, that will really help with retaining name and other information across physical USB connects and disconnects. We do our best to retain the correct information if you plug into the same physical port, but when you change ports, a device without an iSerialNumber essentially becomes a new device. This is not unique to Windows, but it's important enough to mention here. [More info and guidance in this blog post](https://devblogs.microsoft.com/windows-music-dev/the-importance-of-including-a-unique-iserialnumber-in-your-usb-midi-devices/).
