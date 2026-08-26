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

### Device names

The name in an advertisement is frequently absent. Many peripherals put the MIDI Service UUID in
the advertisement and their name in the scan response, and since the watcher filters on the
service UUID, only the packet without the name matches. Both an M-Vave SMK25Mini and an Artiphon
Chorda behave this way, while a KORG nanoKEY Studio puts both in one packet.

A device discovered without a name therefore has one resolved from `BluetoothLEDevice`, which
reports what the Bluetooth stack already knows and works for unpaired devices without connecting
to them. That call is queued to the background worker: it is asynchronous, and an advertisement
callback fires several times a second per device. The stack may not have parsed the scan response
by the time the first advertisement arrives, so a few spaced retries are allowed.

**A device is not listed, and cannot be connected, until it can be named.** A Bluetooth address
means nothing to a user, so offering one to pick from is a last resort reserved for a device
whose name cannot be resolved at all. This gate applies to connecting as well as listing, because
an endpoint takes its name from its device: connecting during the resolution window would publish
a MIDI endpoint named after a hexadecimal address, and that name would then persist in the user's
configuration. A connection asked for during the window is not refused, only deferred, and name
resolution starts it as soon as it finishes.

## Connecting

Discovery never connects. Connecting is driven by the `connectDevice` configuration command, or
by a `devices` array in the transport's section of the configuration file, which is how a
previously approved device is reconnected with no app running.

The `midi` console tool exposes all of this:

```
midi bluetooth list
midi bluetooth connect <device id>
midi bluetooth disconnect <device id>
```

Opening a GATT session and creating a device node both take service locks and raise PnP
notifications, so a connect request is queued and drained by a background worker rather than run
on the caller's thread or on a watcher callback. Doing this work on a callback thread is what
deadlocked the Network MIDI 2.0 transport.

Because of that, `connectDevice` returns before the connection has been attempted. The outcome
is recorded against the discovered device and reported by `listAvailableDevices` as
`lastConnectError`, which `midi bluetooth list` prints under the table. Without it, a failed
connect is completely silent.

The worker resolves the address to a `BluetoothLEDevice`, opens the MIDI Service, selects a
Characteristic, takes a `GattSession` with `MaintainConnection(true)`, subscribes to
notifications, then activates the endpoint.

By far the most common failure is a GATT status of `Unreachable`, which simply means the
peripheral is asleep. BLE MIDI devices sleep aggressively, and a device that Windows lists as
paired is very often not currently connectable.

## Remembered devices

Because of that, a single connection attempt is nearly useless: at service start, or at the
moment a user clicks connect, the device usually cannot answer. So connecting records an
intent rather than performing a one-shot action.

A device stays in the wanted set until it is explicitly disconnected, and a connection is
attempted again every time that device advertises or Windows enumerates its GATT service, rate
limited per device. A device only advertises when it is awake and unconnected, which makes an
advertisement the exact moment it becomes connectable. In practice this means a user can ask for
a device that is switched off, turn it on later, and have it connect on its own.

Nothing connects without being asked first. Pairing alone is not treated as consent, because a
paired device is frequently one the user intends to use with a different host.

The intent itself is held in memory, so it does not survive a service restart. The durable form
is a `devices` array in the transport's section of the configuration file, which the service
reads but never writes. `midi bluetooth connect` writes that entry, and `disconnect` removes it,
so a connection made once is re-established on every subsequent service start.

Because a request is remembered rather than performed, `connectDevice` reports whether the device
is actually present. A request for a device which is switched off succeeds and is honored later,
and saying "connecting" in that case would be untrue.

## Presence

Bluetooth LE offers no notion of a device being online, so presence is inferred:

- A device which is connected is present, taken from the live link rather than from the fact that
  a connection object exists.
- Otherwise a device is present if it has advertised recently. A device only advertises while it
  is awake and unconnected.

These are reported as `isPresent` and `lastSeenAgoMilliseconds`, alongside `hasEndpoint`.

`hasEndpoint` and presence are deliberately separate. An endpoint outlives a device going away,
so the combination of an endpoint that exists and a device that is absent is exactly the silent
disconnect that leaves an application holding an endpoint which will never carry data. Reporting
one flag for both would hide it. A signal strength is also cleared once a device is no longer
present, because a reading from several minutes ago reads as current.

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

## Endpoint customization

The transport supports the shared endpoint customization shape, so a user-supplied name,
description, and image survive across service restarts:

```json
{
  "update": [
    {
      "match": { "endpointDeviceInstanceId": "MIDIU_BLEMIDI_..." },
      "customProperties": { "name": "Studio Keyboard", "description": "...", "image": "keys.png" }
    }
  ]
}
```

The match is on the device instance id, which this transport derives from the Bluetooth address
and the device-supplied name, so it is deterministic. `listAvailableDevices` reports that id for
every discovered device, which means a customization can be written for a device that has never
been connected: the id is known as soon as the device is seen. `transportSuppliedEndpointName`
also matches, for callers which prefer that.

Customizations are cached whether or not the endpoint exists yet, and the cache is consulted
before the device node is created, so an endpoint is never published under the wrong name and
then renamed a moment later. A customization for an endpoint which already exists is applied in
place, so renaming a connected device does not require disconnecting it. The custom name becomes
the device node name and not only a MIDI property, so applications which know nothing about MIDI
properties show it too.

The console command is `midi bluetooth customize <device id> [--name] [--description] [--image]`,
with `--clear` to remove it. The image is a bare file name; a path is rejected, because the
service loads images by name from a known folder.

## Peripheral role

Windows can publish itself as a BLE MIDI Peripheral, so a phone or tablet acting as a Central can
connect to the PC. `MidiBlePeripheral` owns a `GattServiceProvider` for the MIDI service UUID and
a single `GattLocalCharacteristic` with `Notify | WriteWithoutResponse | Read`.

The translation, queuing, timestamping, and callback plumbing are the same code as the Central
role: `MidiBleConnection` takes a small link abstraction supplying only the three things which
differ, which are how a packet is written, what the maximum payload is, and whether the link is
up. Everything downstream of that is shared and already exercised by the codec tests.

**Only one characteristic is published at a time**, selected by the configured protocol and
defaulting to BLE MIDI 1.0. The specification allows a Peripheral to implement a single protocol,
and WinRT provides no way to reject the CCCD subscription a Central makes, so offering both
characteristics would let a Central subscribe to the one which is not being served.

**One Central is served at a time**, which matches the single active connection the specification
permits. The first client to subscribe keeps the link until it goes away; a new client resets the
translation state, because anything queued for the previous one is stale.

**A read of the characteristic is answered with an empty payload**, which is the specified
connection handshake and the mirror of what the Central role performs.

### What the endpoint represents

The endpoint is the **remote device which connected**, not this PC, which is the same model the
Network MIDI 2.0 host uses: a host does not publish an endpoint for itself, it publishes one per
connected remote client, named after that client. So the endpoint is created when a Central
subscribes and removed when it goes away, and it carries the remote device's name.

The remote name comes from `BluetoothLEDevice::FromIdAsync` on the subscribed client's session
device id, falling back to the formatted Bluetooth address. The instance id is
`MIDIU_BLEMIDI_PERIPHERAL_<name>_<address>`, kept distinct from the `MIDIU_BLEMIDI_<name>_<address>`
form used for a device this PC connected out to, so the same phone acting in both directions
cannot collide on one device node.

Creating and removing an endpoint calls back into the service, which must never happen on a
Bluetooth callback thread, so `SubscribedClientsChanged` only sets a flag and wakes the background
endpoint creator, which is the one thread allowed to touch device nodes.

The name this PC advertises is separate, and is the PC's Bluetooth name. The GATT service provider
puts that in the advertisement and gives an application no way to override it, so the transport
reports the name rather than accepting one. It is shown by `getPeripheralStatus` but is not the
endpoint name.

### Commands

Commands are `startPeripheral` (argument `protocol`), `stopPeripheral`, and `getPeripheralStatus`.
The console equivalents are `midi bluetooth peripheral start|stop|status`. The configuration file
section is:

```json
"peripheral": { "enabled": true, "protocol": "bleMidi1" }
```

It is off unless the configuration file asks for it, because publishing makes the machine visible
and connectable to anything nearby.

## Device identity, pairing, and the in-box MIDI 1.0 stack

A BLE device may advertise a **public** address, which is stable, or a **resolvable private
address**, which rotates every few minutes for privacy. Dedicated instruments use stable
addresses. General-purpose computers do not: an iPhone connecting to this PC was observed as
`58C641C7B5AD` and later `66646A807FC1`. Anything keyed on the raw address therefore mints a new
identity on every rotation and leaves the previous one behind.

Windows resolves a rotating address back to a single stable identity **once the device is
bonded**, because bonding records an identity address. Measured on a paired iPhone: connected at
`53D91162DC72`, disconnected, left for seventeen minutes, then reconnected, still
`53D91162DC72`. Unbonded, the same phone had previously appeared under a different address
altogether. Note that re-pairing creates a new bond and therefore a new identity address, so an
unpair and re-pair does move the endpoint.

`BluetoothAddressType` reports `random` in both the bonded and unbonded cases, so it cannot be
used to tell an identity address from a rotating one. Whether the device is paired is the signal
that matters.

Pairing was performed **on the remote device**, which offered to pair with this PC. That is the
direction which has been tested, and it is the natural one, since the phone is the side which
scanned for this PC and connected to it. Initiating the pairing from Windows instead has not been
tried, so the guidance to give a user is the direction known to work: pair with this PC from the
phone's own Bluetooth settings or from the MIDI app doing the connecting.

The endpoint is therefore keyed on the address only when the device is bonded. Everything
unbonded shares a single reusable endpoint, `MIDIU_BLEMIDI_PERIPHERAL_UNPAIRED`, whose name
follows whatever is currently connected. That keeps an unbonded phone from leaving a new endpoint
behind every quarter of an hour, at the cost of not being able to tell two unbonded devices apart,
which is not possible anyway.

Whether pairing is a good idea depends on which role Windows is playing, and the two cases pull in
opposite directions.

| | Windows is the Peripheral (a phone connects to this PC) | Windows is the Central (this PC connects to an instrument) |
|---|---|---|
| Does the address rotate? | Yes, phones and computers rotate | No, instruments use stable addresses |
| Is pairing needed for a stable identity? | Yes | No |
| Does pairing invite a conflict? | No, this PC serves the GATT service | **Yes**, see below |
| Advice | Pair, **from the device**, with this PC | Do not pair unless the device requires it |

The conflict in the Central column is the in-box Windows BLE MIDI 1.0 support. It only enumerates
devices **after** they are paired, and it then opens their GATT service, which can come back to
this transport as `GattOpenStatus::SharingViolation`. It also only knows the MIDI 1.0 data I/O
characteristic, so against a BLE MIDI 2.0 device it can claim a service it is unable to drive,
blocking the transport that could have used the UMP characteristic and forcing a MIDI 1.0 fallback
at best. Pairing an instrument is what exposes it to that, and an instrument gains nothing from
pairing because its address was already stable.

The same applies to third-party vendor BLE MIDI drivers, which can hold a device exclusively for
the same reason.

When this transport ships in-box, the BLE portion of the WinRT MIDI 1.0 stack needs a flag to
disable it, so that only one component claims a BLE MIDI device.

### Generic device names

Apple deliberately withholds the user-assigned device name. Since iOS 16, `UIDevice.name` returns
a generic name such as `iPhone` unless the app holds the
`com.apple.developer.device-information.user-assigned-device-name` entitlement, which Apple grants
only on request. An unpaired iPhone therefore appears as `iPhone`; pairing releases the real name.

Because a generic name is both unhelpful and very likely to collide, a name from the small set of
known generic names is worth flagging to the user, along with the suggestion to pair the device.
Endpoint customization is the other way out, and is per-device.

## Not yet implemented

- A WinRT projection (`Windows.Devices.Midi2.Transports.Bluetooth`) and a setup app for
  approving and configuring connections.
- Jitter Reduction Timestamps.
