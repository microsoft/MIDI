---
layout: kb
title: Understanding Data Translation for MIDI Messages
description: A description of when and where data is translated between MIDI 1.0 data format and MIDI 2.0 UMP, as well as between MIDI 1.0 and MIDI 2.0 protocols.
audience: everyone
categories:
  - Internals
---

In general, Windows MIDI Services translates MIDI messages only when it has to, which is almost always to support a MIDI 1.0 device or a MIDI 1.0 API. Where that translation happens depends on the driver in use.

Inside the service, messages always move around in the UMP format. That gives us one format for scheduling and processing everything. The `Windows.Devices.Midi2*` API and the service behind it both treat every message as UMP, including messages to and from devices that use the MIDI 1.0 byte format.

## A few definitions

### MIDI 1.0 (byte) data format

This is the byte format used by MIDI 1.0. It's also called a "byte stream", because of the way it works over MIDI 1.0 DIN and serial connections.

### MIDI UMP data format

This is the Universal MIDI Packet format. A packet is one to four 32-bit words long, and each one is self-contained. Inside Windows MIDI Services every message is carried and processed as UMP, and translated to the MIDI 1.0 byte format only when something needs it that way.

### MIDI 1.0 protocol

The messages defined in the MIDI 1.0 specifications. Most values are 0 to 127, and a message is one to three bytes long, apart from System Exclusive. These messages can travel in the MIDI 1.0 byte format or in the Universal MIDI Packet format.

### MIDI 2.0 protocol

The messages defined in the MIDI 2.0 UMP specifications, not counting the MIDI 1.0 protocol messages that UMP also carries.

## Where the data format changes

Windows MIDI Services supports both MIDI 1.0 and MIDI 2.0 devices.

| Device | Driver | Windows.Devices.Midi2 | WinMM/WinRT MIDI 1.0 APIs |
| ------------------- | --------------------- | -------------------------- | ------------------------ |
| USB MIDI 1.0 Device | MIDI 2.0 Class Driver | To/From MIDI 1.0 in UMP by driver | To/from MIDI 1.0 byte data format by service |
| USB MIDI 1.0 Device | Older MIDI 1.0 Class Driver | To/From MIDI 1.0 in UMP by service | To/from MIDI 1.0 byte data format by service |
| USB MIDI 1.0 Device | Vendor MIDI 1.0 driver | To/From UMP by service | To/From MIDI 1.0 byte data format by service |
| USB MIDI 2.0 Device | MIDI 2.0 Class Driver | No translation required | To/from MIDI 1.0 byte data format by service |
| Any other MIDI 2.0 Device | (no driver. ex Virtual, Network 2.0) | No translation required | To/from MIDI 1.0 byte data format by service |
| Any other MIDI 1.0 Device | (no driver. ex BLE) | To/From UMP by service | To/From MIDI 1.0 byte data format by service |

### Translation for client APIs

Incoming messages are translated between protocols and data formats only when they need to be, as shown above.

WinMM and WinRT MIDI 1.0 always receive correct MIDI 1.0 channel voice messages, whatever the endpoint sent.

`Windows.Devices.Midi2` gives you the UMP version of what it was given. It does not upscale MIDI 1.0 channel voice messages into MIDI 2.0 channel voice messages. If you need that, there are open source libraries that do it.

### Translation between message type 2 and message type 4

Message type 2 is MIDI 1.0 channel voice, and message type 4 is MIDI 2.0 channel voice.

Windows MIDI Services does not translate messages based on the **protocol a function block declares**. For a native UMP endpoint, send the right protocol yourself: message type 2 for MIDI 1.0 and message type 4 for MIDI 2.0, using what the `MidiEndpointDeviceInformation` class tells you. For an endpoint that is natively MIDI 1.0 byte format, send MIDI 1.0 messages in UMP.

Windows MIDI Services does downscale messages when the **protocol the endpoint negotiated** calls for it. We plan to add upscaling from MIDI 1.0 to MIDI 2.0 for endpoints that negotiate MIDI 2.0 and don't handle MIDI 1.0, but that isn't in the first release.

If a MIDI 1.0 device is connected to the new MIDI 2.0 class driver, Windows MIDI Services **downscales message type 4 to message type 2 before handing it to the driver**. With a MIDI 1.0 device, that driver only handles UMP messages that map directly to the MIDI 1.0 byte format.

### Translation based on the protocol a function block or group terminal declares

As above, Windows MIDI Services doesn't filter or translate messages based on the declared protocol. Use the function block data, or the group terminal block data if that's all there is, to decide which protocol to send. Windows won't stop you from sending a MIDI 2.0 protocol message to a group whose function block says MIDI 1.0.

To learn more about function blocks and other device metadata, see the section on enumerating endpoints.

### Translating Note On with zero velocity to Note Off

Windows MIDI Services does not turn a MIDI 1.0 Note On with zero velocity into a MIDI 1.0 Note Off. Doing that would break the Mackie protocol, and probably others.

When translating between the MIDI 2.0 protocol in UMP and the MIDI 1.0 protocol, the UMP specification says a MIDI 1.0 Note On with zero velocity becomes a MIDI 2.0 Note On with a velocity of 1, and we follow that. Today the only code that does this is in the SDK helper functions, because incoming MIDI 1.0 byte format messages are always translated into the MIDI 1.0 protocol in UMP. We do downscale the MIDI 2.0 protocol in UMP to the MIDI 1.0 byte format when that's needed, as described above.

## Libraries we use

Windows MIDI Services uses publicly available open source libraries for protocol and data format translation.

* [midi2.dev](https://midi2.dev) has several libraries which include translation.
