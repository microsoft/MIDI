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
- **If the audio device can't be opened**, for example because another app has it in exclusive mode, notes are dropped rather than saved up, so they don't all sound at once when the device comes back. Everything else, such as program changes, controllers, and MIDI Capability Inquiry, still gets through. The synthesizer tries the device again every second while notes keep arriving, and stops trying a few seconds after they stop.

## Settings

All of these are in the MIDI Settings app under global settings, and in MIDI Console under `midi synth configure`. Apps can read them through the [Windows.Devices.Midi2.Transports.Synth]({{ site.baseurl }}/sdk-reference/Transports/Synth/) namespace. Do not change the user's settings without their explicit permission. We prefer that the MIDI Settings app be the primary way a customer manages the synth settings.

| Setting | What it does |
| --- | --- |
| **Synthesizer mode** | **Modern** plays the sound set without the limits of the older Windows synthesizer. **Compatible** reproduces those limits on purpose: 32 voices, a 22050 Hz render rate, and no effects. Pick Compatible when you're comparing against how a file sounded on an older PC. This is the "retro" setting. |
| **Audio output** | **Shared** is the normal setting and mixes with everything else. **Shared, low latency** asks the audio engine for smaller buffers, which tightens timing at the cost of some CPU. Not all audio devices support this low-latency Windows API. **Exclusive** takes the audio device for the synthesizer alone, for the shortest delay. See [Exclusive mode](#exclusive-mode) below. |
| **Bank select** | Which convention to read bank select messages in: follow the sender, Roland GS, Yamaha XG, or General MIDI 2. |
| **Volume trim** | A volume control for the synthesizer alone, from -60 dB to +12 dB. The service runs in a place where Windows can't give it a slider in the Volume Mixer, so this is how you balance it against your other audio. |
| **Reverb and chorus** | Turns the built-in effects on or off. They're always off in Compatible mode, because the synthesizer being reproduced didn't have them. |

Changing the synthesizer mode, the audio output, or the reverb and chorus setting restarts the audio, which stops any notes that are sounding. Each channel keeps its program, controllers, and tuning. When **Bank select** is set to follow the sender, the synthesizer also keeps reading bank select the way the sender's last System On message chose. A new **Bank select** setting takes effect right away without a restart, but it can change the instrument each channel plays, so don't change it in the middle of a song. Volume trim is safe to change while music is playing.

### Exclusive mode

In shared mode, the synthesizer's sound goes through the Windows audio mixer along with every other app's sound. That mixing takes time, so there's a delay between a note arriving and you hearing it. Exclusive mode skips the mixer. The synthesizer talks to the audio device directly, with much smaller buffers, so you hear each note sooner. That matters most when you play the synthesizer live from a keyboard.

The cost is that the audio device belongs to the synthesizer while it plays. Other apps can't play sound through that device until the synthesizer lets it go, a few seconds after the last note stops sounding. If you listen to music or join calls on the same device, use one of the shared settings.

Sometimes the synthesizer can't take the device. Another app may already be using it on its own, or Windows may be set not to allow it. That's the **Allow applications to take exclusive control of this device** box in the device's advanced properties in Sound settings. When that happens, the synthesizer plays in shared mode so you still hear it, and it tries exclusive mode again the next time it opens the device. The setting still reads **Exclusive**.

The synthesizer keeps the sample rate the device is already set to, so nothing that takes its clock from the device is disturbed.

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
| Note On attribute: Pitch 7.9 | — | Yes | The attribute sets the exact pitch, from note 0 up to just under note 128. The note number then only names the note for Note Off and the per-note messages, so a sender can reuse note numbers freely, as the specification's note rotation does. On a melodic instrument the pitch picks the sample, so the note sounds the same whatever its number. On a drum kit the note number still picks the drum, and the pitch tunes it |
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
| Registered Controller (RPN) | — | Partial | Bank 0, controllers 0, 1, and 2. See Registered parameters below |
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
| 38 | Data Entry LSB | Yes, for the registered parameters below |
| 64 | Damper (sustain) | Yes |
| 91 | Reverb Send Level | Yes |
| 93 | Chorus Send Level | Yes |
| 100 | RPN LSB | Yes |
| 101 | RPN MSB | Yes |
| 120 | All Sound Off | Yes |
| 121 | Reset All Controllers | Yes |
| 123 | All Notes Off | Yes |

Everything else is ignored, including CC 2, 4, 5, and 12-13, the other LSB pairs at 33-63, the pedals at 65-67, the sound controllers at 70-79, the NRPN selects at 98-99, and the channel mode messages at 122 and 124-127.

**Reset All Controllers follows General MIDI 2 section 3.5.2 and the MIDI-CI Default Control Change Mapping profile.** It resets modulation, expression, the pedals, the parameter number selects, and pitch bend. It deliberately does **not** reset channel volume, pan, bank select, program, or the effect send levels, because both specifications list those as not reset.

### Registered parameters

| RPN | Name | Supported |
| --- | --- | --- |
| 0:0 | Pitch Bend Sensitivity | Yes |
| 0:1 | Fine Tuning | Yes |
| 0:2 | Coarse Tuning | Yes |
| Anything else | | No |

A MIDI 2.0 Registered Controller message for bank 0, controller 0 is treated as pitch bend sensitivity. It takes semitones from the top seven bits of the value and cents from the next seven, as M2-104-UM describes, and ignores the rest. Over MIDI 1.0, the data entry MSB (CC 6) sets the semitones and the data entry LSB (CC 38) sets the cents.

Controller 1 sets fine tuning from the whole 32-bit value. `0x80000000` is in tune, `0x00000000` is 100 cents flat, and `0xFFFFFFFF` is 100 cents sharp. Controller 2 sets coarse tuning from the top seven bits, in semitones, where 64 is in tune. The specification leaves the other bits undefined, so they're ignored. Over MIDI 1.0, fine tuning takes both data entry bytes as one 14-bit value, so it moves in steps of about 0.012 cents. As the MIDI 1.0 specification says, a data entry MSB on its own sets the LSB back to zero, and an LSB on its own adjusts the value that's already there.

Fine and coarse tuning change notes that are already playing too. They add to a note's Pitch 7.9 attribute rather than replacing it, as the specification says.

**Drum channels ignore modulation, the damper pedal, pitch bend, pitch bend sensitivity, and fine and coarse tuning, as General MIDI 2 recommends in sections 3.3, 3.4, and 3.6.** So none of them changes how a drum sounds or how long it rings. A drum channel is any channel that's playing a drum kit, such as channel 10, a channel given drums with the GS Use For Rhythm Part message, or a channel where bank select chose a kit under Yamaha XG or General MIDI 2 addressing. A drum channel doesn't store any of these values when it's sent them, with one exception: lifting the damper pedal always works, so a melodic note the pedal was holding when the channel switched to a kit can't get stuck. A channel that was set up before it switched to a kit keeps those values, because a program change doesn't reset them, but its drums don't use them. They work again if the channel switches back to a melodic program. A melodic note that's still playing when its channel switches to a kit keeps them.

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
| GM System Off | `F0 7E dd 09 02 F7` | Ignored. General MIDI 2 section 4.9.3 says a device that only does General MIDI ignores it, and this synth has no other mode |
| Master Volume | `F0 7F dd 04 01 ll mm F7` | Yes |
| Master Fine Tuning | `F0 7F dd 04 03 ll mm F7` | Yes, except on drum channels, as General MIDI 2 recommends |
| Master Coarse Tuning | `F0 7F dd 04 04 ll mm F7` | Yes, except on drum channels, as General MIDI 2 recommends |
| Global Parameter Control | `F0 7F dd 04 05 ...` | Partial: reverb (slot 1) and chorus (slot 2), parameters 0 and 1, single-byte widths only |
| GS Reset | `F0 41 dd 42 12 40 00 7F vv ss F7` | Yes, resets and selects Roland GS bank addressing |
| GS Use For Rhythm Part | `F0 41 dd 42 12 40 1p 15 vv ss F7` | Yes. `p` is the GS part number, not the channel: part 0 is channel 10, parts 1-9 are channels 1-9 |
| XG System On | `F0 43 dd 4C 00 00 7E 00 F7` | Yes, resets and selects Yamaha XG bank addressing |
| MIDI Capability Inquiry | `F0 7E dd 0D ...` | Yes, see the MIDI-CI chart below |
| Anything else | | Ignored |

Every reset above (GM System On, GM2 System On, GS Reset, XG System On and System Reset) puts all 16 channels back to their starting point: program 1 (piano), bank 0, volume 100, expression 127, pan center, no modulation, damper up, pitch bend centered with a range of 2 semitones, no fine or coarse tuning, no registered parameter selected, reverb send 40 and chorus send 0, with drums on channel 10 only. Master volume and master tuning go back to normal too. Notes that are sounding fade out over a few milliseconds rather than being cut off, so the reset doesn't click.

The synth keeps these settings between applications. Closing an app doesn't reset it, so a file that never picks its sounds plays with whatever the last app left. To start from a known state, send one of these resets first. Windows MIDI Player has a **GM Reset** button, and a setting to send it before each file.

With Roland GS addressing, GS Use For Rhythm Part is the only way a file can put a drum kit on a channel other than 10, and without it eight of the nine kits in the sound set can never be heard alongside the tenth. With Yamaha XG or General MIDI 2 addressing, a bank select can choose a kit on any channel too.

Under General MIDI 2 addressing, it works the other way as well. Bank select MSB 121 with a program change makes any channel melodic, channel 10 included, as General MIDI 2 sections 2.4 and 3.3.1 describe, and MSB 120 makes it a drum channel again. The channel's `ChannelList` entry and its links follow.

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
| Device ID in replies | Function block (`7F`). MIDI Message Report replies use the device ID they were asked with |
| Capability categories declared | Property Exchange and Process Inquiry |
| Profile Configuration | Not declared, not supported |
| Process Inquiry | Declared. MIDI Message Report is supported |
| Protocol Negotiation | Not declared. This is an endpoint reached through UMP Stream, where protocol is negotiated there instead |
| Maximum receivable SysEx size declared | 1024 bytes |
| Function block number declared | 0 |

These are the messages it acts on. Anything addressed to another device's MUID is left alone.

| Message | Supported |
| --- | --- |
| `0x70` Discovery | Yes, answered with `0x71` Discovery Reply |
| `0x7E` Invalidate MUID | Yes, in both directions. Aimed at this device it withdraws and replaces the MUID; aimed at another device it drops whatever was being held for that initiator, including any subscription |
| `0x72` Inquiry: Endpoint | Yes. Status `0x00` (product instance ID) is answered with `0x73` carrying `GM1`, the same ID as the Product Instance ID Notification. Any other status is answered with a NAK |
| `0x30` Inquiry: Property Exchange Capabilities | Yes, answered with `0x31` |
| `0x34` Inquiry: Get Property Data | Yes, answered with `0x35` |
| `0x38` Subscription | Yes, answered with `0x39` |
| `0x36` Inquiry: Set Property Data | No, answered with a NAK |
| `0x20` Profile Inquiry, `0x22` Set Profile On, `0x23` Set Profile Off, `0x28` Profile Details Inquiry | No, answered with a NAK |
| `0x40` Inquiry: Process Inquiry Capabilities | Yes, answered with `0x41`, which declares MIDI Message Report |
| `0x42` Inquiry: MIDI Message Report | Yes, answered with `0x43`, then the messages that report the current values, then `0x44`. See [Process Inquiry](#process-inquiry) |
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

Text a person reads, such as the channel titles, the link titles, the controller names, and the names in `DeviceInfo`, comes from the synthesizer's string resources, so it can be translated. Patch names and category names are never translated.

#### Resources

| Resource | Resource ID | Cache | Subscribe | Paginate |
| --- | --- | --- | --- | --- |
| `ResourceList` | — | No | No | No |
| `DeviceInfo` | — | 3600 seconds | No | No |
| `ChannelList` | — | No, it reflects what is set right now | Yes | No |
| `ProgramList` | `melodic` or `drums`, and a resource ID is required | 3600 seconds | No | Yes |
| `ChCtrlList` | `channel` or `drums`, and a resource ID is required | 3600 seconds | No | No |

`ResourceList` does not list itself, per Common Rules for Property Exchange section 14. Its entries also leave out any setting that's already the default for that resource. For example, `ProgramList` and `ChCtrlList` both need a resource ID, but their own specifications already say so, so the list doesn't repeat it.

#### DeviceInfo

Carries the same manufacturer, family, model, and version as the Identity Reply and the Discovery Reply, because Property Exchange requires the three to agree: manufacturer `00 00 41` "Microsoft", family 11 "Windows", model 1 "General MIDI Synth", version `01 00 00 00` "1.0.0.0".

#### ChannelList

One entry per MIDI channel, 16 in all. Each entry carries its title, its channel number, the bank MSB, bank LSB, and program currently selected, and the name of the instrument that plays. That name follows the bank addressing setting, so it's right for Yamaha XG and General MIDI 2 bank numbers too, and it's the capital tone when the sound set doesn't have the variation a bank asks for, because that's what plays. It also carries two links: one to the program list that applies to that channel, `melodic` for a melodic channel or `drums` for a drum channel, and one to the controller list that applies, `channel` or `drums`.

The title says what the channel plays: "Percussion" for a drum channel and "Melodic" for every other channel. A drum channel is any channel that's playing a drum kit, so the title and both links follow the drums when a file puts a kit on another channel, with the GS message or with bank select.

This is the one resource that can be subscribed to. When a channel's program or bank changes, every subscriber is sent a `notify`, and answers it with an ordinary Get when it wants the new list.

#### ProgramList

Two lists, told apart by resource ID:

- `melodic` — every melodic instrument in the sound set, including the Roland GS variations.
- `drums` — every drum kit.

Each entry carries its `title`, its `bankPC` as a three-element array of bank MSB, bank LSB, and program, and its `category`. Melodic entries are categorized with the instrument group names from the General MIDI 1 specification: Piano, Chromatic Percussion, Organ, Guitar, Bass, Strings, Ensemble, Brass, Reed, Pipe, Synth Lead, Synth Pad, Synth Effects, Ethnic, Percussive, and Sound Effects. Drum kits are categorized as Drum Kit, which is this device's own name for them, because General MIDI names no group for a kit.

Both lists support pagination through `offset` and `limit` in the request header, and every reply carries `totalCount` whether it was paginated or not.

#### ChCtrlList

Two lists, one for melodic channels and one for drum channels. The list with resource ID `channel` has 15 controllers, most useful first: volume, modulation, pitch bend, sustain pedal, pan, expression, reverb send, chorus send, per-note pitch bend, pitch bend sensitivity, fine tuning, coarse tuning, and per-note volume, pan, and pitch. The list with resource ID `drums` leaves out what drum channels ignore: modulation, pitch bend, the sustain pedal, pitch bend sensitivity, and fine and coarse tuning. That leaves 9 controllers. Each channel's `ChannelList` entry links to the list that applies to it.

Every entry says `"transmit":"none"`, because the synthesizer only receives controllers. Each default is the value a reset returns to, written as a 32-bit MIDI 2.0 value. For example, the default volume of 100 is `3374617161`.

### Process Inquiry

The synthesizer supports MIDI Message Report. An app that connects after a song has started can use it to find out what each channel is set to, without having sent those messages itself.

| Item | Value |
| --- | --- |
| Features declared | MIDI Message Report |
| Device ID | One channel (`00` to `0F`), the group (`7E`), or the function block (`7F`). The group and the function block cover the same 16 channels |
| Message data control | `00` sends only the reply and the end, `01` sends only values that differ from the power-up state, and `7F` sends everything |
| System messages | None |
| Channel controller messages | Pitch bend, control change, registered controllers, and program change |
| Note data messages | None |

The reply lists only the kinds of message the synthesizer can report, whatever the inquiry asked for. The report itself is sent as MIDI 2.0 channel voice messages, because this endpoint always uses the MIDI 2.0 protocol. Each channel is reported in full before the next one starts, in this order: pitch bend, controllers 1, 7, 10, 11, 64, 91, and 93, pitch bend sensitivity, fine tuning, and coarse tuning (registered controllers 0, 0 to 0, 2), and then the program with its bank. A value that was sent as a 32-bit MIDI 2.0 value comes back exactly. A value that was sent as a 7-bit MIDI 1.0 value comes back scaled up to 32 bits, the same way a MIDI 1.0 to MIDI 2.0 translator would do it. Pitch bend sensitivity comes back as semitones in the top seven bits and cents in the next seven. Coarse tuning comes back as whole semitones in the top seven bits, because that's all this synthesizer keeps of it. A full report on all 16 channels is 198 packets.

An inquiry with a reserved message data control value, or one that's cut off before its bitmaps, gets a NAK with status `0x41`, "message was malformed". One addressed to a reserved device ID gets a NAK with status `0x03`, "channel, group, or function block not in use".

### Not-implemented MIDI-CI features

- **Profiles.** No profile is published, and no profile message is acted on. The most likely candidate if this changes is the Default Control Change Mapping profile, whose reset behavior the synthesizer already follows.
- **Set Property Data**, so nothing here can be written over Property Exchange, only read.
- **Any resource beyond the five above**, including `JSONSchema`, `LocalOn`, `CMList`, and proprietary `X-` resources.

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
