---
layout: kb
title: MIDI 2.0 Implementation Details
audience: everyone
description: Details about our MIDI 2.0 implementation in Windows MIDI Services
---

Specifications can be funny. As much as the MIDI Association, and all of us in it, try to be very specific and crisp on wording, there's often room for interpretation. Most of these we work out among the various OS companies under the umbrella of the MIDI Association. But there are others were an approach may just not make sense on one OS or the other. Here are the ones that are Windows-specific, that you should be aware of as a developer.

Of course, the full source code for Windows MIDI Services, including the USB MIDI 2.0 driver, is available in our GitHub repo, linked at the bottom of every page, so you can review it at any time to better understand how a feature or function works.

## Discovery and Protocol Negotiation

Windows MIDI Services supports only the UMP-based Endpoint Discovery and Protocol Negotiation. We do not implement the deprecated MIDI-CI equivalents. We also do not wrap or automatically perform MIDI 1.0 SysEx-based discovery.

## JR Timestamps

The hardware manufacturers, AMEI, and the MIDI Association agreed that JR Timestamp handling in the operating systems is not needed at this time (and may not be needed for many years). As a result, we do not have JR Timestamp handling built in. Please do not request or supply JR Timestamps.

When (if) it is needed, we will implement JR clock generation, incoming JR timestamp generation, and outgoing JR timestamp/clock message creation in the service itself. Client applications should not send JR timestamps now or in the future.

## Message Translation

Internally in Windows MIDI Services, all messages are UMP messages, whether they come from a MIDI 1.0 byte data format device, a UMP-native device or API, or a MIDI 1.0 classic API. This requires Windows to perform message data format translation in some cases.

Translation happens as described in [the translation page](data-translation.md)

## UMP Endpoint Names for native MIDI 2.0 UMP format devices

Although we make all the names available through the Enumeration API, we have an order of precedence we use when providing the recommended `Name` property value. In order from most preferred to least, we have:

1. Any user-supplied endpoint name configured through the configuration files (these will be created by the MIDI Settings app in the future)
2. The name supplied through in-protocol Endpoint Name Notification messages
3. The name supplied by the transport plugin in the service. This is typically pulled from a device name supplied by the driver, or other transport-specific sources such as network advertising in the case of Network MIDI 2.0.

When we create MIDI 1.0-compatible ports for MIDI 2.0 endpoints, we use the group numbers and the endpoint name. However, we also have built-in naming algorithms that are different for native MIDI 1.0 devices, including both WinMM-compatible naming, and more modern naming which also uses the iJack/Pin values. We are limited to 32 characters for WinMM ports.

## UMP Endpoint Names for MIDI 1.0 byte stream format devices

The service also creates UMP endpoints for MIDI 1.0 devices. This happens two ways:

1. If the device is assigned to the USB MIDI 2.0 driver (this is preferred) the driver creates Group Terminal Blocks for each jack or "cable" (a "port" in MIDI 1.0 API speak). In the new driver, we use the `iJack` names, if provided, to name the Group Terminal Blocks. This is the best way to ensure your endpoint and Group Terminal Block names are correct.
2. If the device is assigned a third-party driver or the legacy MIDI 1.0 driver, the service creates the Group Terminal Blocks using a similar algorithm. However, because the data available to the service is not exactly the same as that available in the MIDI 2.0 driver, there may be some differences.

The precedence for naming is the same as with MIDI 2.0 devices, with the exception of the Endpoint Name Notification, which doesn't exist in MIDI 1.0.

1. Any user-supplied endpoint name
3. The name supplied by the transport plugin in the service.

How MIDI 1.0 names are picked is set, by endpoint and globally, using the MIDI Settings app. In that, you can override naming algorithms or even provide custom names.

## iSerialNumber Really Helps

If your device exposes a unique iSerialNumber, that will really help with retaining name and other information across physical USB connects and disconnects. We do our best to retain the correct information if you plug into the same physical port, but when you change ports, a device without an iSerialNumber essentially becomes a new device. This is not unique to Windows, but it's important enough to mention here. [More info and guidance in this blog post](https://devblogs.microsoft.com/windows-music-dev/the-importance-of-including-a-unique-iserialnumber-in-your-usb-midi-devices/).
