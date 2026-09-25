---
layout: kb
title: How the built-in General MIDI synthesizer works
audience: everyone
description: What the built-in General MIDI synthesizer is, how to turn it on and off, how to set it up, and a full MIDI 2.0 and MIDI Capability Inquiry implementation chart.
categories:
  - Transport Details
  - Developer Guidance
---

Windows MIDI Services includes a General MIDI synthesizer that plays through your normal Windows audio device. Send it notes and you hear them. There's no driver to install, no sound card involved, and no extra app to keep running.

It shows up as an ordinary MIDI 2.0 endpoint named **General MIDI Synth**, so any app that can list MIDI devices can find it and play it. Older apps see it as a regular MIDI output port too.

This is a new endpoint written from scratch, not a rename of the older Microsoft GS Wavetable Synth. However, it does still use the original GS/GM Sound set provided by Roland when this synth was first introduced in Windows. Thank you, Roland.

## What it plays

The synthesizer uses the General MIDI sound set that ships with Windows, so the sounds are the ones you already know from years of MIDI files. That sound set holds all 128 General MIDI programs, plus a set of Roland GS variations and nine drum kits. To see the real counts and the kit names on your PC, run `midi synth sound-set` in [MIDI Console]({{ site.baseurl }}/tools/console/).

**It's a General MIDI 1 and GS synthesizer, and it says so.** It doesn't claim General MIDI 2. The sound set doesn't have everything GM2 asks for, and claiming a standard it can't meet would only mislead the apps that check.

Files written for Yamaha XG or for General MIDI 2 still play. They address their extra sounds differently from GS, so there's a **Bank select** setting that tells the synthesizer which convention to read. Leave it on **Follow the sender** and a reset message in the file picks the right one.

## Where it shows up

- **In modern apps**, as a MIDI 2.0 UMP endpoint called General MIDI Synth, with one group and one function block.
- **In older apps**, as one MIDI output port to play, and one MIDI input port that carries its replies back. Names are built the same way Windows builds them for any other endpoint; see [How MIDI 1.0 port names are generated]({{ site.baseurl }}/kb/how-midi1-port-names-are-generated/).
- **More than one app can play it at once.** The synthesizer is multi-client, like every other endpoint in Windows MIDI Services, so your sequencer and a monitoring tool can both have it open.

## Turning it off

Turning the synthesizer off doesn't mute it. **It removes the endpoint completely and hands the audio device back.**

That's on purpose. Some apps open every MIDI port they can find the moment they start, and a few web pages do it too. If "off" only muted the synthesizer, one of those apps could hold its endpoint open, which would hold the Windows audio device open, which would keep it away from a program that wants exclusive use of it. If you're about to record with an audio app in WASAPI exclusive mode or through ASIO, switch the synthesizer off and the audio device cannot be taken by the in-box Synth.

Turn it back on and the endpoint comes back with the same device identifier, so an app that remembered it finds it again.

You can turn it on and off in the **MIDI Settings** app, under global settings, or from MIDI Console with `midi synth enable` and `midi synth disable`. Add `--temporary` to the console command to change it for now without saving it, which undoes itself the next time the service restarts.

## When the synth holds the audio device

Even when the synthesizer is switched on, it doesn't keep the audio device open all the time. It opens one when a note or other channel message arrives, and releases it again after a few seconds of silence with no notes still sounding.

Two things follow from that, and both are deliberate:

- **An app can connect to the synthesizer without taking over the audio device.** Discovery, identity, and MIDI Capability Inquiry are all answered with nothing open.
- **The first note after a long rest opens the audio device**, which takes a few milliseconds. Your settings, programs, and channel state are kept across the rest, so nothing resets.

## Settings

All of these are in the MIDI Settings app under global settings, and in MIDI Console under `midi synth configure`. Apps can read them through the [Windows.Devices.Midi2.Transports.Synth]({{ site.baseurl }}/sdk-reference/Transports/Synth/) namespace. Do not change the user's settings without their explicit permission. We prefer that the MIDI Settings app be the primary way a customer manages the synth settings.

| Setting | What it does |
| --- | --- |
| **Synthesizer mode** | **Modern** plays the sound set without the limits of the older Windows synthesizer. **Compatible** reproduces those limits on purpose: 32 voices, a 22050 Hz render rate, and no effects. Pick Compatible when you're comparing against how a file sounded on an older PC. This is the "retro" setting. |
| **Audio output** | **Shared** is the normal setting and mixes with everything else. **Shared, low latency** asks the audio engine for smaller buffers, which tightens timing at the cost of some CPU. Not all audio devices support this low-latency Windows API. |
| **Bank select** | Which convention to read bank select messages in: follow the sender, Roland GS, Yamaha XG, or General MIDI 2. |
| **Volume trim** | A volume control for the synthesizer alone, from -60 dB to +12 dB. The service runs in a place where Windows can't give it a slider in the Volume Mixer, so this is how you balance it against your other audio. |
| **Reverb and chorus** | Turns the built-in effects on or off. They're always off in Compatible mode, because the synthesizer being reproduced didn't have them. |

Changing the synthesizer mode, the bank select mode, or the effects setting rebuilds the sound engine, which clears every channel's program and controller state. Don't change them in the middle of a song. Volume trim is safe to change while music is playing.

## Getting the program list

The synthesizer answers MIDI Capability Inquiry Property Exchange, so an app can ask it for its instrument list by name rather than shipping its own copy of the General MIDI program list. It reports its melodic instruments and its drum kits as two separate lists, and tells each channel which of the two applies to it. That's how [MIDI Keyboard]({{ site.baseurl }}/tools/midikeyboard/) fills its patch list when you point it at the synthesizer.

If you're writing an app that wants to do the same, start with [How to read a device's patch list]({{ site.baseurl }}/kb/how-to-read-a-device-patch-list/). The exact resources and their shapes are in the chart below. The official MIDI-CI specifications may be found at [the MIDI Association web site](https://midi.org).

---

## MIDI 2.0 implementation chart

The rest of this page is a reference for developers and for anyone comparing the synthesizer against another device. It describes what this endpoint answers on the wire.

### Identity and endpoint

| Item | Value |
| --- | --- |
| Manufacturer | Microsoft |
| Manufacturer SysEx ID | `00 00 41` |
| Device family | 11 (Windows 11) |
| Device family model number | 1 |
| Software revision level | `01 00 00 00` (reported as 1.0.0.0) |
| UMP endpoint name | General MIDI Synth |
| Product instance ID | `GM1` |
| UMP version declared | 1.1 |
| Function blocks | 1, static |
| Groups | 1 |
| Protocols declared | MIDI 1.0 and MIDI 2.0 |
| Jitter reduction timestamps | Neither sent nor requested |
| Stream configuration reply | MIDI 2.0 protocol, no jitter reduction timestamps |
| Multi-client | Yes |
| MIDI 1.0 ports created | One output port and one input port, from one group terminal block in each direction |

### Function block 0

| Item | Value |
| --- | --- |
| Name | General MIDI Synth |
| Active | Yes |
| Direction | Bidirectional |
| UI hint | Receiver |
| First group | Group 1 (index 0) |
| Number of groups | 1 |
| MIDI-CI message version | 1.2 |
| Maximum SysEx8 streams | 0 |

The direction is bidirectional because the block answers MIDI-CI on the same group it receives notes on. The UI hint is Receiver because it's a tone generator, so an app building a list of things to *play from* should leave it out.

### UMP message types

| Type | Name | Supported | Notes |
| --- | --- | --- | --- |
| `0x0` | Utility | No | Counted and discarded, including jitter reduction timestamps |
| `0x1` | System Real Time and System Common | Partial | See System messages below |
| `0x2` | MIDI 1.0 Channel Voice | Yes | |
| `0x3` | Data, 7-bit (SysEx7) | Yes | Reassembled up to 1024 bytes, then parsed |
| `0x4` | MIDI 2.0 Channel Voice | Yes | |
| `0x5` | Data, 8-bit (SysEx8 and Mixed Data Set) | No | |
| `0xD` | Flex Data | No | |
| `0xF` | UMP Stream | Yes | See UMP Stream below |

Messages on any group other than the one this endpoint owns are discarded, as are channel voice messages with no matching handler.

### Channel voice messages

| Message | MIDI 1.0 (`0x2`) | MIDI 2.0 (`0x4`) | Notes |
| --- | --- | --- | --- |
| Note Off | Yes | Yes | |
| Note On | Yes | Yes | In MIDI 1.0, velocity 0 is a Note Off. In MIDI 2.0 it is not, per the specification |
| Note On attribute: Pitch 7.9 | — | Yes | The attribute sets the pitch; the note number still chooses the sample and articulation, so a microtonal scale keeps the right sound for the key |
| Note On attribute: manufacturer specific, profile specific | — | Ignored | The note still sounds at its nominal pitch |
| Poly Pressure | No | No | There is no modulation matrix to route it to |
| Control Change | Yes | Yes | 32-bit resolution in MIDI 2.0. See Control changes below |
| Program Change | Yes | Yes | The MIDI 2.0 form applies its bank when the bank valid bit is set |
| Channel Pressure | No | No | Same reason as Poly Pressure |
| Pitch Bend | Yes | Yes | 32-bit resolution in MIDI 2.0 |
| Per-Note Pitch Bend | — | Yes | |
| Registered Per-Note Controller | — | Partial | Controllers 3 (pitch), 7 (volume), and 10 (pan). Others ignored |
| Assignable Per-Note Controller | — | No | |
| Per-Note Management | — | Yes | Both flags: detach and reset per-note controllers |
| Registered Controller (RPN) | — | Partial | Bank 0, controller 0 only. See Registered parameters below |
| Assignable Controller (NRPN) | — | No | |
| Relative Registered Controller | — | No | |
| Relative Assignable Controller | — | No | |

### Control changes

| CC | Name | Supported |
| --- | --- | --- |
| 0 | Bank Select MSB | Yes |
| 1 | Modulation | Yes |
| 6 | Data Entry MSB | Yes, for the registered parameters below |
| 7 | Channel Volume | Yes |
| 10 | Pan | Yes |
| 11 | Expression | Yes |
| 32 | Bank Select LSB | Yes |
| 64 | Damper (sustain) | Yes |
| 91 | Reverb Send Level | Yes |
| 93 | Chorus Send Level | Yes |
| 100 | RPN LSB | Yes |
| 101 | RPN MSB | Yes |
| 120 | All Sound Off | Yes |
| 121 | Reset All Controllers | Yes |
| 123 | All Notes Off | Yes |

Everything else is ignored, including CC 2, 4, 5, and 12-13, the LSB pairs at 33-63, the pedals at 65-67, the sound controllers at 70-79, the NRPN selects at 98-99, and the channel mode messages at 122 and 124-127.

**Reset All Controllers follows General MIDI 2 section 3.5.2 and the MIDI-CI Default Control Change Mapping profile.** It resets modulation, expression, the pedals, the parameter number selects, and pitch bend. It deliberately does **not** reset channel volume, pan, bank select, program, or the effect send levels, because both specifications list those as not reset.

### Registered parameters

| RPN | Name | Supported |
| --- | --- | --- |
| 0:0 | Pitch Bend Sensitivity | Yes |
| 0:1 | Fine Tuning | Yes |
| 0:2 | Coarse Tuning | Yes |
| Anything else | | No |

A MIDI 2.0 Registered Controller message for bank 0, controller 0 is treated as pitch bend sensitivity, taking whole semitones from the top seven bits of the value.

### System messages

| Message | Supported |
| --- | --- |
| System Reset (`FF`) | Yes, resets every channel |
| Active Sensing (`FE`) | Yes |
| Timing Clock, Start, Continue, Stop | No |
| Song Position Pointer, Song Select, Tune Request, MIDI Time Code | No |

### System exclusive

Sent as SysEx7 in UMP message type `0x3`. `dd` is the device ID byte, which is not checked.

| Message | Bytes | Supported |
| --- | --- | --- |
| Identity Request | `F0 7E dd 06 01 F7` | Yes, answered with an Identity Reply carrying the identity above |
| GM System On | `F0 7E dd 09 01 F7` | Yes, resets and selects Roland GS bank addressing |
| GM2 System On | `F0 7E dd 09 03 F7` | Yes, resets and selects General MIDI 2 bank addressing |
| GM System Off | `F0 7E dd 09 02 F7` | Treated as a system reset |
| Master Volume | `F0 7F dd 04 01 ll mm F7` | Yes |
| Master Fine Tuning | `F0 7F dd 04 03 ll mm F7` | Yes |
| Master Coarse Tuning | `F0 7F dd 04 04 ll mm F7` | Yes |
| Global Parameter Control | `F0 7F dd 04 05 ...` | Partial: reverb (slot 1) and chorus (slot 2), parameters 0 and 1, single-byte widths only |
| GS Reset | `F0 41 dd 42 12 40 00 7F vv ss F7` | Yes, resets and selects Roland GS bank addressing |
| GS Use For Rhythm Part | `F0 41 dd 42 12 40 1p 15 vv ss F7` | Yes. `p` is the GS part number, not the channel: part 0 is channel 10, parts 1-9 are channels 1-9 |
| XG System On | `F0 43 dd 4C 00 00 7E 00 F7` | Yes, resets and selects Yamaha XG bank addressing |
| MIDI Capability Inquiry | `F0 7E dd 0D ...` | Yes, see the MIDI-CI chart below |
| Anything else | | Ignored |

GS Use For Rhythm Part is the only way a file can put a drum kit on a channel other than 10, and without it eight of the nine kits in the sound set can never be heard alongside the tenth.

### UMP Stream

| Message | Supported |
| --- | --- |
| Endpoint Discovery | Yes, honoring the filter bits |
| Endpoint Info Notification | Yes |
| Device Identity Notification | Yes |
| Endpoint Name Notification | Yes |
| Product Instance ID Notification | Yes |
| Stream Configuration Request | Yes, accepted as asked |
| Stream Configuration Notification | Yes |
| Function Block Discovery | Yes, for block 0 or for all blocks |
| Function Block Info Notification | Yes |
| Function Block Name Notification | Yes |
| Start of Clip, End of Clip | No |

Because the function blocks are static, an app can read this once and keep it.

---

## MIDI Capability Inquiry implementation chart

The synthesizer is a MIDI-CI responder. It never starts a conversation; it answers one.

### Discovery

| Item | Value |
| --- | --- |
| MIDI-CI version | 1.2, and it replies in whichever version the initiator asked in |
| Device ID in replies | Function block (`7F`) |
| Capability categories declared | Property Exchange only |
| Profile Configuration | Not declared, not supported |
| Process Inquiry | Not declared, not supported |
| Protocol Negotiation | Not declared. This is an endpoint reached through UMP Stream, where protocol is negotiated there instead |
| Maximum receivable SysEx size declared | 1024 bytes |
| Function block number declared | 0 |

These are the messages it acts on. Anything addressed to another device's MUID is left alone.

| Message | Supported |
| --- | --- |
| `0x70` Discovery | Yes, answered with `0x71` Discovery Reply |
| `0x7E` Invalidate MUID | Yes, in both directions. Aimed at this device it withdraws and replaces the MUID; aimed at another device it drops whatever was being held for that initiator, including any subscription |
| `0x30` Inquiry: Property Exchange Capabilities | Yes, answered with `0x31` |
| `0x34` Inquiry: Get Property Data | Yes, answered with `0x35` |
| `0x38` Subscription | Yes, answered with `0x39` |
| `0x36` Inquiry: Set Property Data | No, answered with a NAK |
| `0x20` Profile Inquiry, `0x22` Set Profile On, `0x23` Set Profile Off, `0x28` Profile Details Inquiry | No, answered with a NAK |
| `0x40` Inquiry: Process Inquiry Capabilities, `0x42` MIDI Message Report | No, answered with a NAK |
| `0x72` Endpoint Inquiry | No, answered with a NAK |
| Profile reports, process inquiry replies, and anything else that is not an inquiry | Ignored |

**It always answers an inquiry.** One it doesn't implement gets a NAK with status `0x01`, "MIDI-CI message not supported", rather than silence, so an initiator doesn't spend a three second timeout on every message. Replies, reports, ACK, NAK, and Invalidate MUID are not NAKed, because nothing is waiting on an answer to those and answering them can start a loop.

**MUID collisions are handled.** A Discovery message whose source MUID matches this device's own MUID is a collision. The synthesizer replies with an Invalidate MUID for the shared value, drops it, and draws a new one.

### Property Exchange

| Item | Value |
| --- | --- |
| Simultaneous requests declared | 1 |
| Property Exchange version reported | 0.0, as Common Rules for Property Exchange 1.0 and 1.1 both require |
| Header and body encoding | ASCII JSON. No `mutualEncoding` is declared, so `Mcoded7` and zlib are not used |
| Chunking | Replies are chunked to fit the initiator's declared maximum SysEx size |
| Subscriptions | Up to 8 at once |

#### Resources

| Resource | Resource ID | Cache | Subscribe | Paginate |
| --- | --- | --- | --- | --- |
| `ResourceList` | — | No | No | No |
| `DeviceInfo` | — | 3600 seconds | No | No |
| `ChannelList` | — | No, it reflects what is set right now | Yes | No |
| `ProgramList` | `melodic` or `drums`, and a resource ID is required | 3600 seconds | No | Yes |

`ResourceList` does not list itself, per Common Rules for Property Exchange section 14.

#### DeviceInfo

Carries the same manufacturer, family, model, and version as the Identity Reply and the Discovery Reply, because Property Exchange requires the three to agree: manufacturer `00 00 41` "Microsoft", family 11 "Windows", model 1 "General MIDI Synth", version `01 00 00 00` "1.0.0.0".

#### ChannelList

One entry per MIDI channel, 16 in all. Each entry carries its title, its channel number, the bank MSB, bank LSB, and program currently selected, the name of the instrument those select, and one link to the program list that applies to that channel: `melodic` for a normal channel, `drums` for the rhythm channel. The link moves with the rhythm channel, so if a file moves drums off channel 10 with the GS message, the link moves too.

This is the one resource that can be subscribed to. When a channel's program or bank changes, every subscriber is sent a `notify`, and answers it with an ordinary Get when it wants the new list.

#### ProgramList

Two lists, told apart by resource ID:

- `melodic` — every melodic instrument in the sound set, including the Roland GS variations.
- `drums` — every drum kit.

Each entry carries its `title`, its `bankPC` as a three-element array of bank MSB, bank LSB, and program, and its `category`. Melodic entries are categorized with the instrument group names from the General MIDI 1 specification: Piano, Chromatic Percussion, Organ, Guitar, Bass, Strings, Ensemble, Brass, Reed, Pipe, Synth Lead, Synth Pad, Synth Effects, Ethnic, Percussive, and Sound Effects. Drum kits are categorized as Drum Kit, which is this device's own name for them, because General MIDI names no group for a kit.

Both lists support pagination through `offset` and `limit` in the request header, and every reply carries `totalCount` whether it was paginated or not.

### Not-implemented MIDI-CI features

- **Profiles.** No profile is published, and no profile message is acted on. The most likely candidate if this changes is the Default Control Change Mapping profile, whose reset behavior the synthesizer already follows.
- **Process Inquiry**, including MIDI Message Report.
- **Set Property Data**, so nothing here can be written over Property Exchange, only read.
- **Any resource beyond the four above**, including `JSONSchema`, `LocalOn`, `CMList`, and proprietary `X-` resources.

---

## Not-implemented MIDI features

- **No SysEx8 and no Mixed Data Set.** The function block declares zero SysEx8 streams.
- **No Flex Data**, so tempo, key signature, and metadata messages are ignored.
- **No poly pressure or channel pressure.** The sample playback engine has no modulation matrix to route them to.
- **No NRPN**, and no per-voice filter, which is why the sound controllers at CC 70-79 aren't implemented.
- **No MIDI 2.0 protocol negotiation over MIDI-CI.** Protocol is settled over UMP Stream, which is where MIDI 2.0 puts it.
- **No custom sound set.** It plays the sound set Windows installs, and nothing else. Nothing a caller supplies is parsed.

## See also

- [WinRT API support for the General MIDI synthesizer]({{ site.baseurl }}/sdk-reference/Transports/Synth/), for reading its state and changing its settings from an app
- [How to read a device's patch list]({{ site.baseurl }}/kb/how-to-read-a-device-patch-list/)
- [MIDI Settings]({{ site.baseurl }}/tools/settings/), which has these settings in its global settings dialog
- [MIDI Console]({{ site.baseurl }}/tools/console/), for `midi synth status`, `sound-set`, `instrument-list`, `enable`, `disable`, and `configure`
- [MIDI File Player]({{ site.baseurl }}/tools/midiplayer/), which plays standard MIDI files straight to it
