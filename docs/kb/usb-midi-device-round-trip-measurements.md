---
layout: kb
title: USB MIDI 1.0 device round trip measurements
description: Unofficial round trip time and jitter measurements for a set of USB MIDI 1.0 interfaces, taken on a single PC while tuning the outbound message scheduler
audience: everyone
---

# USB MIDI 1.0 device round trip measurements

## Please read this first

**This is not a benchmark, and it is not an official Microsoft comparison of MIDI products.**

These numbers came out of engineering work on the Windows MIDI Services outbound message
scheduler. We needed to know how much of the delay between asking for a message to be sent and
the message actually arriving somewhere was ours, and how much belonged to the device. Measuring
a pile of interfaces was a means to that end. It was never designed up front as a product
comparison, and it should not be read as one.

Specifically:

- **One PC.** Everything here was measured on a single Intel 285K based desktop with 24 logical
  processors, running a Windows 11 Insider Canary build. A different PC, chipset, USB host
  controller, or Windows build may produce different numbers.
- **One unit of each device**, except for the WCH CH345, where two physically different units were
  measured. A single unit tells you about that unit.
- **Whatever firmware was already on the devices.** Nothing was updated for this exercise. These
  are working devices from a studio, not a controlled sample.
- **The devices were not chosen to be representative.** They are what happened to be on hand.
- **Some devices were deliberately left out.** No inMusic (M-Audio) interfaces were tested. There
  are known issues with inMusic drivers when a device is disconnected, and this exercise involved
  hot-swapping devices a large number of times. We also already had sufficient data for the
  intended purpose of the exercise.
- **No device was tuned, configured, or optimized** for the test beyond connecting a cable.
- These are **unofficial** measurements from a development machine. They carry no warranty, and
  nothing here should be used to make a purchasing decision.

If a vendor sees a number here they believe is wrong, it very likely reflects this specific unit
on this specific PC, and we would be glad to hear about it.

## What was measured

Every device was wired so that its **MIDI OUT was connected back to its own MIDI IN** with a
standard MIDI cable. The test then:

1. Sends a MIDI message from a normal application, through the Windows MIDI Services SDK.
2. The message travels through the service, the driver, USB, and out of the device's MIDI OUT
   socket.
3. It crosses the MIDI cable.
4. It comes back into the device's MIDI IN socket, back over USB, through the driver and the
   service, and is handed to the application.
5. The application notes the time it arrived.

The number reported is the whole of that journey, in microseconds. There are 1000 microseconds in
a millisecond.

100 messages are sent per run, spaced 20 milliseconds apart so that each one is handled on its own,
and each run is repeated several times. The figure quoted is the **median**, the middle value, so
that one unusual message cannot move it.

### Why two different message sizes

The test alternates between a 3 byte MIDI message and a 2 byte MIDI message.

A MIDI cable carries one byte in almost exactly 320 microseconds. That is fixed by the MIDI
standard and cannot vary. So a 3 byte message spends about 960 microseconds on the cable and a 2
byte message about 640. Measuring both and subtracting tells us how much of the round trip was the
cable, and therefore how much was everything else.

This also guards against a measurement trap. Some interfaces use a MIDI feature called *running
status* on their MIDI OUT, which omits a repeated status byte and so puts fewer bytes on the cable.
A test that sent 100 identical messages would measure such a device as faster than it behaves with
real, varied music data. Alternating the message type prevents that. One device in this set, the
Roland UM-ONE, does this, and it measured about 320 microseconds faster before the test was
corrected.

## What the numbers mean, and what they do not

**What the round trip figure includes:** the Windows MIDI Services stack twice (once outbound, once
inbound), the driver twice, USB twice, the device's own processing twice, and one trip along the
MIDI cable.

**What it does not tell you:** how much of that is Windows and how much is the device. This method
cannot separate them. The only part that was isolated is the hand-off from the service to the
application, which measured 20 to 40 microseconds on four different devices.

**A real one-way send is not half this number.** The journey out and the journey back are not
symmetric, and no attempt was made to measure one direction alone.

**Jitter** is reported as the interquartile range: the spread of the middle half of the messages.
It is used in preference to "worst minus best" because a single stray message distorts that badly.
For a musician, jitter matters more than latency, because a constant delay can be compensated for
and an inconsistent one cannot.

**These figures are for MIDI 1.0 devices on 5 pin DIN cables.** They say nothing about USB MIDI 2.0
devices, which were not part of this exercise.

## Results

Median round trip for a 3 byte MIDI message, in microseconds. Lower is faster.

| Device | KS (usbmidi2-acx) | KSA (usbaudio) | KSA (vendor driver) | Jitter (IQR) | Notes |
|---|---|---|---|---|---|
| WCH CH345, unit 1 | 1176 | 1207 | — | 45–65 | (1) (2) |
| WCH CH345, unit 2 | — | 1206 | — | 27–54 | (1) (2) |
| Roland UM-ONE | 1226 | 1263 | 1247 | 33–66 | (3) |
| Generic "USB MIDI cable" | 1226 | 1255 | — | 35–63 | (4) |
| ESI M8U eX | 1236 | 1273 | — | 23–54 | (5) (6) |
| ESI MIDIMATE eX | — | 1250 | — | 29–63 | (5) (7) |
| CME C2MIDI Pro | 1282 | 1315 | — | 36–75 | (8) |
| Blokas MIDIhub | 1389 | 1428 | — | 50–120 | (9) |
| iConnectivity mio 1x1 | — | 1492 | — | 26–61 | |
| FORE / Prodipe / DigitalLife BM1003 | — | 1484–1510 | — | 53–93 | (10) |
| "8-in 9-out" rack MIDI interface | — | — | ~4950 | 988–1002 | (11) (12) |

A dash means that combination was not tested, either because the device is not class compliant or
because no vendor driver was installed.

### Notes

1. **This chipset silently discards messages sent together.** When several messages were handed
   over as a single block, only the first arrived: 1 of 2, 1 of 4, 1 of 8, and 3 of 20. The result
   was identical on both Windows drivers and on both units, three runs each. Spaced 20 milliseconds
   apart, all 100 messages arrived every time. *Inference:* the device appears to have effectively
   no input buffer, so anything arriving while it is transmitting is lost. In practice this would
   mean losing notes from a chord. Nothing is reported to the application when this happens. Note
   that this is the fastest device in the table for single messages, which is exactly why a latency
   figure on its own can be misleading. These devices show up as "USB MIDI Interface" and often have silver cables with a stylized staff on an oval black or semi-transparent electronics enclosure. One was stylized with a zipper between cables, so multiple devices exist.
2. Both units report as `USB2.0-MIDI` with identical USB identifiers, so Windows cannot tell them
   apart, even though the enclosures differ. If you have multiples of these, Windows will see them all as the same device if connected at different times.
3. This device uses running status on its MIDI OUT. See "Why two different message sizes" above.
   It was the only device in this set that did so.
4. An unbranded cable with no manufacturer name anywhere on it, which nonetheless outperformed
   several branded interfaces. It uses a different chipset from the three in note 10. This is a black cable, with both MIDI cables sprouting immediately from the USB plug end, so all the electronics are in the USB plug portion. No markings.
5. **Both ESI units have ports that work as either an input or an output**, chosen automatically
   from how the port is being used, a feature ESI documents. A single cable between two ports
   therefore loops back in *both* directions, which is unusual. *Measured:* reversing the direction
   of a port costs roughly 60 to 93 messages, about 1.2 to 1.9 seconds, during which messages are
   silently lost. Once settled, delivery was complete. *Inference:* the port needs sustained
   traffic, not merely elapsed time, before it will switch, because short bursts of 8 messages did
   not trigger it while a sustained run did.
6. 16 ports. Measured between port 1 and port 9. Because of note 5, the measurement software needed
   the ability to listen on a different port than it sends on.
7. Reports its manufacturer as Ploytec GmbH rather than ESI.
8. USB-C, so this was the only device connected through a different USB hub from the rest. Hub
   differences are therefore not controlled for in this row.
9. A programmable router rather than a plain interface, and the only device here that processes
   messages internally as a matter of course. Measured on port D.
10. **Three different brands, one device.** All three report the same name, the same blank
    manufacturer, and the same USB identifiers, so Windows cannot distinguish them. Their measured
    figures agree within 26 microseconds. One of them has a make and model printed on the case, and
    still reports the generic identity. The range given covers all three.
11. This device is deliberately not named here. It is not class compliant, so it can only run on
    its vendor driver, and it was therefore impossible to determine whether the figures in the table
    are caused by the device or by that driver. This device is no longer available for purchase from most locations, as best as we can tell. The driver is also older at this point.
12. **This device behaves differently from every other one measured.** Its round trip does not
    change with message size at all, where every other device tracks the cable time closely, and
    its jitter is quantized to almost exactly 1 millisecond. *Inference:* something in the device or
    its driver buffers to a periodic tick, so a message waits for the next slot regardless of size.
    A fixed delay of this kind can be compensated for; the 1 millisecond of jitter cannot.

## Summary of findings

- **The device is by far the largest variable.** Excluding the older rack interface, the devices
  measured spanned 1176 to 1510 microseconds, a difference of about 330 microseconds, on the same PC
  with the same cable and the same Windows build. Including it, the spread is roughly fourfold. Price
  and brand did not predict performance: the fastest device in the table is a cheap generic cable, and
  an unbranded lead beat several branded interfaces.

- **The newer KS path (usbmidi2-acx) is slightly faster than KSA (usbaudio).** Measured on six
  device pairings, the same device was faster on KS every time, by between 20 and 39 microseconds.
  The gain is the same for a 2 byte message as for a 3 byte one, which indicates it is a per
  transfer cost rather than a per byte one. In musical terms this difference is not audible; it is
  reported because it was consistent.

- **The two class drivers produce similar jitter.** Compared like for like on the same device, the
  interquartile ranges overlap, and on one device KS was slightly worse. The KS advantage is in
  latency only, not in consistency.

- **Loading the CPU did not meaningfully change either figure.** With all 24 logical processors
  pinned at 100 percent, the median round trip moved by about 200 microseconds and the worst single
  message was 1689 microseconds. No message exceeded the median by more than 2 milliseconds. This
  does not support the occasional claim that Windows adds tens of milliseconds of jitter to USB
  MIDI.

- **The part of the path that could be isolated is small.** The hand-off from the Windows MIDI
  Services service to the application measured 20 to 40 microseconds, consistently across four
  devices. The difference between the two Windows driver paths is 20 to 39 microseconds. The
  remainder of the non-cable time, between roughly 140 and 500 microseconds depending on the device,
  includes both the rest of the operating system path and the device's own processing, and this
  method cannot divide it between them.

- **Measuring a device properly requires more than a stopwatch.** Three separate effects would have
  produced wrong numbers if they had not been found: running status shortening what goes on the
  cable, adaptive ports needing time to change direction, and a device silently dropping messages
  that arrive together. Each is invisible unless specifically tested for.

## Reproducing this on your own PC

The measurement code ships in this repository as part of the MIDI SDK integration tests, with a
script to drive it.

1. Build `src\in-box\Midi2-AppSDK.sln` for x64 Release.
2. Connect a MIDI cable from the device's MIDI OUT to its own MIDI IN.
3. Find the endpoint id with `midi enumerate endpoints --verbose`.
4. Run the script from
   `src\in-box\Test\WinRT Client\Session.integrationtests\measure-device-round-trip.ps1`.

Discover which input port an output port loops back to, which is worth doing first on any device
with more than one port:

```powershell
.\measure-device-round-trip.ps1 -EndpointId '<your endpoint id>' -Scan
```

Measure it:

```powershell
.\measure-device-round-trip.ps1 -EndpointId '<your endpoint id>' -OutGroup 1 -InGroup 1 -Runs 4
```

Test whether the device drops messages that arrive together:

```powershell
.\measure-device-round-trip.ps1 -EndpointId '<your endpoint id>' -Burst 20 -Runs 3
```

For a device with ports that switch between input and output automatically, raise the settling
time so that a port has time to reverse:

```powershell
.\measure-device-round-trip.ps1 -EndpointId '<your endpoint id>' -Scan -ScanSettleMilliseconds 3000
```

The script requires TAEF, which is installed with the Windows Driver Kit. Run
`Get-Help .\measure-device-round-trip.ps1 -Full` for all options.

Results from other PCs and other devices are welcome. Please include your Windows build, your
hardware, and which driver each device was using.
