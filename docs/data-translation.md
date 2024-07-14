---
layout: page
title: Data Translation
has_children: false
---

# Data Translation

In general, Windows MIDI Services translates MIDI messages only when it absolutely has to (primarily to support MIDI 1.0 devices or MIDI 1.0 APIs). This translation happens in different places, depending upon the driver in use.

Internally, the MIDI service moves messages around in the UMP format. This enables a standard format for message scheduling and processing. In addition, the `Microsoft.Windows.Devices.Midi2*` SDK and the service behind it, both treat all messages as UMP, including ones to/from devices which were MIDI 1.0 byte data format.

## A couple definitions






## Translation Scenarios involving data format changes

Windows MIDI Services supports both MIDI 1.0 and MIDI 2.0 devices.

| Device | Driver | Windows.Devices.Midi2 | Older MIDI 1.0 APIs |
| ------------------- | --------------------- | -------------------------- | ------------------------ |
| USB MIDI 1.0 Device | MIDI 2.0 Class Driver | To/From MIDI 1.0 in UMP by driver | To/from MIDI 1.0 byte data format by service |
| USB MIDI 1.0 Device | Older MIDI 1.0 Class Driver | To/From MIDI 1.0 in UMP by service | To/from MIDI 1.0 byte data format by service |
| USB MIDI 1.0 Device | Vendor MIDI 1.0 driver | To/From UMP by service | To/From MIDI 1.0 byte data format by service |
| USB MIDI 2.0 Device | MIDI 2.0 Class Driver | No translation required | To/from MIDI 1.0 byte data format by service |
| Any other MIDI 2.0 Device | (no driver. ex Virtual, Network 2.0) | No translation required | To/from MIDI 1.0 byte data format by service |
| Any other MIDI 1.0 Device | (no driver. ex BLE) | To/From UMP by service | To/From MIDI 1.0 byte data format by service |

### Translation between Message type 2 (MIDI 1.0 Channel Voice) and Message type 4 (MIDI 2.0 Channel Voice)

Currently, Windows MIDI Services does not translate messages based on negotiated protocol or Function Block declared protocol. Instead, for native UMP endpoints, applications should send the correct protocol messages (message type 2 for MIDI 1.0-compatible and message type 4 for MIDI 2.0-compatible messages) based upon the information provided by the `MidiEndpointDeviceInformation` class. In addition, for native MIDI 1.0 byte data format endpoints, applications should send the appropriate MIDI 1.0 messages in UMP.

If a MIDI 1.0 device is connected to the new MIDI 2.0 Class Driver, Windows MIDI Services *will* downscale Message Type 4 to Message Type 2 before sending to the driver. This is because the driver, when working with a MIDI 1.0 device, only handles UMP messages which can be directly translated to MIDI 1.0 data format.

### Translation based on declared protocol for a Function Block or Group Terminal

As mentioned above, Windows MIDI Services does not filter out or translate messages based on the declared protocol. Applications should use the Function Block (preferred) or Group Terminal Block (backup) data to decide which protocol to use when sending messages. However, Windows will not stop an application from sending a MIDI 2.0 Protocol in UMP message to a Group in a Function Block which specifies MIDI 1.0 Protocol.

To learn more about metadata like Function Blocks, see the section on Enumerating endpoints.

### Translation of Note On with zero velocity to Note Off

Windows MIDI Services does not, by default, translate a MIDI 1.0 Note On with zero velocity to a MIDI 1.0 Note Off message. Doing so would break the Mackie protocol and possibly others.

When translating between MIDI 2.0 Protocol in UMP and MIDI 1.0 protocol (MIDI 1.0 byte data format, or MIDI 1.0 protocol in UMP), Windows MIDI Services follows the MIDI 2.0 UMP specification and translates a MIDI 1.0 Note On with a zero velocity to a MIDI 2.0 Note On with a velocity of 1. However, there is no code, outside of the SDK helper functions, which does this today, as incoming MIDI 1.0 byte data format messages are always translated into MIDI 1.0 protocol in UMP. We do, however, downscale MIDI 2.0 Protocol in UMP to MIDI 1.0 byte data format when required, as per above.

## Resources for translation

Windows MIDI Services makes use of publicly-available open source libraries for protocol and data format translation.

* [midi2.dev](https://midi2.dev) contains a number of libraries which include translation.
