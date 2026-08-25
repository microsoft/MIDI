---
layout: kb
title: Bluetooth Low Energy MIDI Transport Architecture
audience: developers
description: How the Windows MIDI Services Bluetooth LE MIDI transport discovers, connects to, and exchanges MIDI with BLE MIDI 1.0 and BLE MIDI 2.0 devices.
---

> <h4>Preview</h4>
> The Bluetooth LE MIDI transport is preview code. BLE MIDI 2.0 support follows a draft
> specification (MIDI Association TSB item #274) which has not been published, so its details
> may change.

This page describes the design of the in-box Bluetooth Low Energy MIDI transport
(`Midi2.Ble2MidiTransport.dll`, registered as `Midi2BluetoothMidiTransport`). It is aimed at
people working on the transport itself rather than at app developers.

## One transport, two protocols

Both BLE MIDI 1.0 and BLE MIDI 2.0 live in the same GATT service:

| Item | UUID |
|---|---|
| MIDI Service | `03B80E5A-EDE8-4B33-A751-6CE34EC4C700` |
| MIDI 1.0 Characteristic | `7772E5DB-3868-4112-A1A9-F2669D106BF3` |
| UMP Characteristic | `C3B10ECF-88F5-4F7D-BFFA-8AD2C91FBAFE` |

A Peripheral rejects a subscription to a second MIDI Characteristic while one is already active,
and the specification requires a Central which discovers both to subscribe to the UMP
Characteristic. Preferring one over the other is therefore not a policy decision the service can
make independently, and it cannot be split across two transport plugins: `midisrv` does not
coordinate between plugins, so two of them would race to claim the same device. One transport
owns the whole MIDI Service.

`SelectPreferredMidiCharacteristic` reads the UMP Characteristic first and falls back to the
MIDI 1.0 Characteristic.

## Discovery

Two watchers feed a single table of discovered devices, keyed by the device's Bluetooth address
rendered as 12 hexadecimal digits. That address is the only identifier both watchers can supply,
and it is what the configuration commands and the endpoint device instance ids use.

- **`BluetoothLEAdvertisementWatcher`**, filtered on the MIDI Service UUID, in Active scanning
  mode so scan response data is collected too. This is the primary source, because it finds
  devices which have never been paired. Requiring pairing before a device is even visible is one
  of the main complaints about the older Windows BLE MIDI 1.0 support.
- **A GATT `DeviceWatcher`** over `GattDeviceService.GetDeviceSelectorFromUuid`. This only sees
  devices Windows has already enumerated, but it sees them whether or not they are advertising
  right now.

Entries which are neither paired nor connected are dropped from the available list once they
have not advertised for a while.

## Connecting

Discovery never connects. Connecting is driven by the `connectDevice` configuration command, or
by a `devices` array in the transport's section of the configuration file, which is how a
previously approved device is reconnected with no app running.

Opening a GATT session and creating a device node both take service locks and raise PnP
notifications, so a connect request is queued and drained by a background worker rather than run
on the caller's thread or on a watcher callback. Doing this work on a callback thread is what
deadlocked the Network MIDI 2.0 transport.

The worker resolves the address to a `BluetoothLEDevice`, opens the MIDI Service, selects a
Characteristic, takes a `GattSession` with `MaintainConnection(true)`, subscribes to
notifications, then activates the endpoint.

## Endpoint properties

Both kinds of device are presented to the service as UMP endpoints, because the transport does
the translation. The native data format is what tells apps, and the service's own transform
selection, what the device really is.

| | BLE MIDI 1.0 | BLE MIDI 2.0 |
|---|---|---|
| `NativeDataFormat` | ByteStream | UMP |
| `SupportedDataFormats` | UMP | UMP |
| MIDI 2.0 protocol capability | no | yes |
| Endpoint discovery and negotiation | skipped, discovery marked complete | performed |

Marking discovery complete on a BLE MIDI 1.0 endpoint is what lets the service create its
MIDI 1.0 ports immediately instead of waiting for a negotiation that will never be answered.
Declaring ByteStream as the native format is also what makes the service insert its protocol
downscaler, so an app sending MIDI 2.0 protocol messages to a MIDI 1.0 device still works.

## BLE MIDI 1.0 packet format

`midi_ble_midi1_codec.h` is a self-contained implementation of the packet format with no
dependency on Bluetooth, the service or COM, which is what makes it unit testable
(`Midi2.Transport.BleMidi.unittests`).

A packet is a Header Byte (bit 7 set, bit 6 reserved, bits 5-0 `timestampHigh`) followed by
Timestamp Byte and message pairs (bit 7 set, bits 6-0 `timestampLow`). Timestamps are 13 bit
millisecond values.

**Decoding** restores omitted Running Status bytes, so everything downstream sees whole
messages. Running Status is canceled at the end of every packet. System Real-Time messages
cancel neither Running Status nor an in-progress SysEx. A SysEx continuation packet is a header
byte followed immediately by data with no timestamp byte, which is the only signal that
distinguishes it from the start of a message.

**Timestamps** arrive in the sender's clock domain, and correlating the two clocks is explicitly
out of scope for the specification. Since a timestamp may never be scheduled in the future, the
decoder anchors the newest message in a packet to the moment the packet arrived and backdates
the rest by their spacing. That preserves the intra-packet timing the timestamps exist to convey
without needing any clock correlation.

**Encoding** never generates Running Status: transmitting it is optional and every receiver is
required to accept full messages. A complete message other than SysEx is never split across two
packets. Because a packet carries a single `timestampHigh`, a message past a 128 ms boundary
starts a new packet rather than relying on the receiver's single wrap allowance.

## BLE MIDI 2.0 packet format

The ATT notification payload is UMP data with no header or other framing. Words are big-endian,
several UMPs may be concatenated, and fragmentation is prohibited: a UMP which does not fit is
deferred to the next notification. A malformed UMP invalidates the rest of the payload, because
without a valid length there is no way to find the next message boundary.

Jitter Reduction Timestamps are optional in the specification and are not implemented. The
service has no JR clock, and at a 15 ms connection interval the BLE schedule is the dominant
latency bound anyway.

## Sending

Writes go through a per-connection queue drained by a writer thread. The Central always writes
without response. The connection interval gates throughput, so blocking a client's thread on a
BLE write is not acceptable. The queue is bounded and drops its oldest packets rather than
letting a device which has stopped draining exhaust service memory.

## Disconnection and return

`MaintainConnection(true)` asks Windows to re-establish a dropped link, and the transport also
watches `ConnectionStatusChanged`. On the way down, the translation state is reset in both
directions: a link drop can cut a SysEx transfer in half, and BLE MIDI 2.0 says UMP stream state
is not preserved across disconnections. On the way back up, notifications are re-subscribed,
because a device which returns without them looks alive to apps while delivering nothing, and a
BLE MIDI 2.0 endpoint is queued for full discovery and protocol negotiation again.

## Not yet implemented

- Peripheral role. Windows should be able to act as a Peripheral as well as a Central, in the
  way it can already be both host and client for Network MIDI 2.0.
- A WinRT projection (`Windows.Devices.Midi2.Transports.Bluetooth`) and a setup app for
  approving and configuring connections.
- Jitter Reduction Timestamps.
