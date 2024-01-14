# Implementation Details

Specifications can be funny. As much as the MIDI Association, and all of us in it, try to be very specific and crisp on wording, there's often room for interpretation. Most of these we work out among the various OS companies under the umbrella of the MIDI Association. But there are others were an approach may just not make sense on one OS or the other. Here are the ones that are Windows-specific, that you should be aware of as a developer.

Of course, the full source code for Windows MIDI Services, including the USB MIDI 2.0 driver, is available in our repo, so you can review it at any time to better understand how a feature or function works.

## Discovery and Protocol Negotiation

Windows MIDI Services supports only the UMP-based Endpoint Discovery and Protocol Negotiation. We do not implement the deprecated MIDI-CI equivalents.

In addition, declaring the use of JR Timestamps in a USB MIDI 2.0 Group Terminal Block does not enable JR Timestamps in Windows MIDI Services. Instead, these must be negotiated using UMP-based Endpoint Discovery end Protocol Negotiation

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
