---
layout: page
title: Data Translation
parent: For Developers
---

# Data Translation

In general, Windows MIDI Services translates MIDI messages only when it has to. This translation happens in two different places, depending upon the driver in use.

Internally, the MIDI service moves messages around in the UMP format. This enables a standard format for message scheduling and processing. In addition, the `Windows.Devices.Midi2` API treats all messages as UMP, including ones to/from devices which were bytestream MIDI 1.0 format.

## Translation Scenarios involving data format changes

Windows MIDI Services supports MIDI 1.0 and MIDI 2.0 devices.

| Device | Driver | Windows.Devices.Midi2 | Older MIDI 1.0 APIs |
| ------------------- | --------------------- | -------------------------- | ------------------------ |
| USB MIDI 1.0 Device | MIDI 2.0 Class Driver | To/From UMP by driver | To/from byte stream by service |
| USB MIDI 1.0 Device | Older MIDI 1.0 Class Driver | To/From UMP by service | To/from byte stream by service |
| USB MIDI 1.0 Device | Vendor MIDI 1.0 driver | To/From UMP by service | To/From byte stream by service |
| USB MIDI 2.0 Device | MIDI 2.0 Class Driver | No translation required | To/from byte stream by service |
| Any other MIDI 2.0 Device | (no driver. ex Virtual, Network 2.0) | No translation required | To/from byte stream by service |
| Any other MIDI 1.0 Device | (no driver. ex BLE) | To/From UMP by service | To/From byte stream by service |

## Translation between Message type 2 (MIDI 1.0 Channel Voice) and Message type 4 (MIDI 2.0 Channel Voice)

Currently, Windows MIDI Services does not translate messages based on negotiated protocol or Function Block declared protocol. Instead, for native UMP endpoints, applications should send the correct protocol messages (message type 2 for MIDI 1.0-compatible and message type 4 for MIDI 2.0-compatible messages) based upon the information provided by the `EndpointDeviceInformation` [and related enumeration types](developer-docs\Windows.Devices.Midi2\enumeration\README.md). In addition, for native bytestream endpoints, applications should send the appropriate MIDI 1.0 messages in UMP.

If a MIDI 1.0 device is connected to the new MIDI 2.0 Class Driver, Windows MIDI Services *will* downscale Message Type 4 to Message Type 2 before sending to the driver. This is because the driver, when working with a MIDI 1.0 device, only handles UMP messages which can be directly translated to MIDI 1.0 data format.

## Resources for translation

Windows MIDI Services makes use of publicly-available open source libraries for protocol and data format translation.

* [midi2.dev](https://midi2.dev) contains a number of libraries which include translation.
