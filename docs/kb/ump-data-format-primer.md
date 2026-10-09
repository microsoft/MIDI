---
layout: kb
title: A primer for UMP data format
audience: everyone
description: What MIDI messages look like as Universal MIDI Packets (UMP), how they compare with the MIDI 1.0 bytes you know from tools like MIDI-OX and Pocket MIDI, and how Windows translates between the two.
categories:
  - Getting Started
  - Internals
---

If you've spent time with MIDI-OX or a tool like it, you can probably read MIDI 1.0 messages by sight. `90 3C 64` is a note, `B0 07 64` is a volume change, and anything that starts with `F0` is going to take a while. The tools that come with Windows MIDI Services, like [MIDI Monitor]({{ site.baseurl }}/tools/midi2monitor/), show those same messages in a newer form called the Universal MIDI Packet, or UMP. That note from the first example now shows up as `20903C64`.

This article is for you if you know the MIDI 1.0 bytes and want to make sense of what you see now. It shows you how to read a UMP, puts common messages side by side in both forms, explains how System Exclusive changed, and describes what Windows translates for you. It doesn't try to cover the whole UMP specification. There's a link to that at the end.

The good news is that you already know most of it.

## What UMP is

UMP is the data format that arrived with MIDI 2.0. It carries MIDI 1.0 messages as well as MIDI 2.0 messages, so UMP and MIDI 2.0 aren't the same thing. A MIDI 1.0 keyboard's notes can travel as UMP without any change in what they mean.

UMP data is made of 32-bit words. A word is four bytes, which is eight hexadecimal (hex) digits. A packet, which the specification calls a UMP, is one, two, three, or four words long and holds one message. Long messages, like System Exclusive, are split across several packets.

There's no running status in UMP. Every packet carries its own status, even when it's the same as the one before.

Inside Windows MIDI Services, every message is a UMP, including messages to and from MIDI 1.0 devices. That's why MIDI Monitor and [MIDI Console]({{ site.baseurl }}/tools/console/) show UMP words for every device, old or new. This translation makes it simple to move messages around, merge and split streams, and filter/modify individual messages.

## Reading a UMP

Here's that note again: middle C (note 60), velocity 100, on channel 1. In MIDI 1.0 bytes it's `90 3C 64`. As a UMP, it's `20903C64`.

| Hex digits | What they mean |
| --- | --- |
| `2` | The message type. Type 2 is a MIDI 1.0 channel voice message, such as a note or a controller. |
| `0` | The group. This is group 1. |
| `90` | The MIDI 1.0 status byte you already know: Note On (`9`) on channel 1 (`0`) |
| `3C` | Note number 60, middle C |
| `64` | Velocity 100 |

After the first two hex digits come the MIDI 1.0 bytes you already know, in the same order. A message shorter than three bytes is filled out with `00`.

### Groups

For simplicity's sake*, a group is the UMP version of a MIDI 1.0 port, or of a cable in USB MIDI 1.0. Each group has its own 16 channels and its own system messages, and a device can use up to 16 groups. A MIDI interface with four MIDI outputs shows up in older apps as four ports. In Windows MIDI Services, it's one device with four groups.

In the data, groups and channels are counted from `0` to `F`, and apps show them as 1 to 16. So group `0` is group 1, and channel `0` is channel 1, the same way MIDI 1.0 channels have always worked.

To learn how ports and groups line up, see [Understanding How we Map MIDI 1.0 Ports to UMP Endpoints]({{ site.baseurl }}/kb/mapping-midi1-port-concepts/).

\* There's more subtlety to what a Group really is, especially as a way to expand the channel count across a MIDI 2.0 function, but for our purposes here, this fits. The Windows MIDI Services mapping of Cable to Group is not part of the specification -- it's an affordance to bringing classic MIDI 1.0 devices into the UMP format.

### Message types

The first hex digit of every UMP is its message type. It tells you what kind of message it is, and how many words to read.

| Type | Name | Words | Carries |
| --- | --- | --- | --- |
| `0` | Utility | 1 | Small housekeeping messages, such as "no operation" and timestamps. You'll rarely see these. |
| `1` | System Real Time and System Common | 1 | Clock, Start, Continue, Stop, Active Sensing, Reset, MIDI Time Code, Song Position, Song Select, and Tune Request. That's every MIDI 1.0 system message except System Exclusive. |
| `2` | MIDI 1.0 Channel Voice | 1 | Notes, pressure, controllers, program changes, and pitch bend, exactly as MIDI 1.0 defines them |
| `3` | Data, including System Exclusive | 2 | MIDI 1.0 System Exclusive, up to six bytes in each packet |
| `4` | MIDI 2.0 Channel Voice | 2 | The MIDI 2.0 versions of the channel messages, with much higher resolution, plus some new ones |
| `5` | Data | 4 | System Exclusive 8 and Mixed Data Set, for large blocks of data. MIDI 1.0 has nothing like these. |
| `D` | Flex Data | 4 | Tempo, time signature, key signature, chord names, and text such as lyrics |
| `F` | UMP Stream | 4 | Messages a device uses to describe itself, such as its name and its function blocks |

The other types, 6 through C and E, are reserved for the future, and include message types with 3 words.

The second hex digit is the group in every type except `0` and `F`. Those two apply to the whole device, so they don't have a group.

## Common messages

Here are the messages you'll see most often, as MIDI 1.0 bytes and as UMP words. Every example is on channel 1 and group 1, and every value is in hex. The UMP words are what MIDI Monitor shows in its **Message** column.

| Message | MIDI 1.0 bytes | UMP words | UMP message type |
| --- | --- | --- | --- |
| Note On, note 60, velocity 100 | `90 3C 64` | `20903C64` | `2` MIDI 1.0 Channel Voice |
| Note Off, note 60, velocity 64 | `80 3C 40` | `20803C40` | `2` MIDI 1.0 Channel Voice |
| Note On with velocity 0, which means Note Off in MIDI 1.0 | `90 3C 00` | `20903C00` | `2` MIDI 1.0 Channel Voice |
| Poly Pressure (polyphonic aftertouch), note 60, pressure 80 | `A0 3C 50` | `20A03C50` | `2` MIDI 1.0 Channel Voice |
| Control Change, controller 7 (volume), value 100 | `B0 07 64` | `20B00764` | `2` MIDI 1.0 Channel Voice |
| Program Change, program 5 | `C0 05` | `20C00500` | `2` MIDI 1.0 Channel Voice |
| Bank Select MSB 1 and LSB 0, then Program Change 5 | `B0 00 01`<br>`B0 20 00`<br>`C0 05` | `20B00001`<br>`20B02000`<br>`20C00500` | `2` MIDI 1.0 Channel Voice |
| Channel Pressure (aftertouch), pressure 80 | `D0 50` | `20D05000` | `2` MIDI 1.0 Channel Voice |
| Pitch Bend, centered | `E0 00 40` | `20E00040` | `2` MIDI 1.0 Channel Voice |
| RPN 0, 0 (pitch bend range), 12 semitones | `B0 65 00`<br>`B0 64 00`<br>`B0 06 0C`<br>`B0 26 00` | `20B06500`<br>`20B06400`<br>`20B0060C`<br>`20B02600` | `2` MIDI 1.0 Channel Voice |
| NRPN 1, 8, value 64 | `B0 63 01`<br>`B0 62 08`<br>`B0 06 40`<br>`B0 26 00` | `20B06301`<br>`20B06208`<br>`20B00640`<br>`20B02600` | `2` MIDI 1.0 Channel Voice |
| Timing Clock | `F8` | `10F80000` | `1` System Real Time and System Common |
| Start | `FA` | `10FA0000` | `1` System Real Time and System Common |
| Continue | `FB` | `10FB0000` | `1` System Real Time and System Common |
| Stop | `FC` | `10FC0000` | `1` System Real Time and System Common |
| Active Sensing | `FE` | `10FE0000` | `1` System Real Time and System Common |
| System Reset | `FF` | `10FF0000` | `1` System Real Time and System Common |
| MIDI Time Code Quarter Frame | `F1 23` | `10F12300` | `1` System Real Time and System Common |
| Song Position Pointer | `F2 10 02` | `10F21002` | `1` System Real Time and System Common |
| Song Select | `F3 04` | `10F30400` | `1` System Real Time and System Common |
| Tune Request | `F6` | `10F60000` | `1` System Real Time and System Common |
| System Exclusive: General MIDI System On | `F0 7E 7F 09 01 F7` | `30047E7F 09010000` | `3` Data, including System Exclusive |
| System Exclusive: Roland GS Reset | `F0 41 10 42 12 40 00 7F 00 41 F7` | `30164110 42124000`<br>`30337F00 41000000` | `3` Data, including System Exclusive |

A few things to notice:

- **The bytes don't change.** Types 1 and 2 hold the MIDI 1.0 status and data bytes exactly as they are, and System Exclusive keeps every data byte. Nothing is lost going from MIDI 1.0 bytes to UMP and back.
- **Some things still take several messages.** In type 2, Bank Select with Program Change is still three messages, and an RPN or NRPN is still several Control Changes. The MIDI 2.0 protocol, in the next section, turns each of these into a single message.
- **Numbers start at 0 in the data.** Channel `0` is channel 1. Many instruments also show program `00` as program 1, so the program `05` in the table might be called 6 on yours.
- **A Note On with velocity 0 stays a Note On with velocity 0.** In type 2 it means exactly what it means in MIDI 1.0, and Windows doesn't change it into a Note Off. This is important for some meta protocols built on top of MIDI 1.0.

## MIDI 2.0 channel messages

A device that's using the MIDI 2.0 protocol sends its channel messages as type 4 instead of type 2. You'll see these from instruments that are set to MIDI 2.0 mode, and some apps send them too.

Type 4 messages are two words long. The first word looks a lot like type 2: the status and channel are in the same place, so `4090...` is still a Note On on channel 1. The value moves to the second word, where there's room for much more detail. Velocity is 16 bits instead of 7, and controllers, pressure, and pitch bend are all 32 bits.

Here are the same channel messages as type 4:

| Message | UMP words | What's different |
| --- | --- | --- |
| Note On, note 60, velocity 100 | `40903C00 C9240000` | The velocity, `C924`, is 16 bits. The last four digits can carry extra information about the note, such as its exact pitch. |
| Note Off, note 60, velocity 64 | `40803C00 80000000` | Same layout as Note On |
| Poly Pressure, note 60, pressure 80 | `40A03C00 A0820820` | The pressure is 32 bits |
| Control Change, controller 7, value 100 | `40B00700 C9249249` | The value is 32 bits. Controller numbers are still 0 to 127. |
| Bank Select MSB 1 and LSB 0, then Program Change 5 | `40C00001 05000100` | One message instead of three. The `1` at the end of the first word means the bank is included. The second word holds the program (`05`), an unused byte, and then the bank MSB (`01`) and LSB (`00`). |
| Channel Pressure, pressure 80 | `40D00000 A0820820` | The pressure is 32 bits |
| Pitch Bend, centered | `40E00000 80000000` | The value is 32 bits, and centered is `80000000` |
| RPN 0, 0 (pitch bend range), 12 semitones | `40200000 18000000` | One message instead of four, now called a Registered Controller. The first word holds the RPN number (`00 00`), and the second holds the value. |
| NRPN 1, 8, value 64 | `40300108 80000000` | One message instead of four, now called an Assignable Controller. The first word holds the NRPN number (`01 08`). |

Why does velocity 100 become `C924` instead of a round number? When a 7-bit value moves up to 16 or 32 bits, the UMP specification stretches it so the bottom, the middle, and the top of the range still line up. 0 stays 0, 64 becomes `8000` (the exact middle), and 127 becomes `FFFF`. The values in between are spread out smoothly, so most of them don't look round in hex.

These are the MIDI 1.0 messages from the first table, converted with the specification's rules. When Windows sends one of them to a MIDI 1.0 device or app, it turns it back into exactly those MIDI 1.0 bytes.

A few more differences are worth knowing when you read raw data:

- **A Note On with velocity 0 is a real Note On** in the MIDI 2.0 protocol, not a Note Off. It starts a note at the lowest velocity there is.
- **Some messages are new.** MIDI 2.0 adds controllers and pitch bend for each individual note, and Registered and Assignable Controllers that move a value up or down instead of setting it. MIDI 1.0 has nothing that matches them.
- **MIDI Monitor names them for you.** Under each message you'll see a label such as **MIDI 2.0 Note On** or **MIDI 2.0 Registered Controller**, and the **Details** column spells out the values.

## System Exclusive

In MIDI 1.0 bytes, a System Exclusive (SysEx) message is one long run of bytes. It starts with `F0`, ends with `F7`, and can be as long as it needs to be. System Real Time messages like clock are allowed to cut in, but any other status byte ends the message.

UMP carries SysEx in message type 3. It leaves out the `F0` and `F7`, and splits the bytes in between into packets. Each packet is two words long and holds up to six data bytes. The third hex digit says where the packet belongs in the message:

| Third digit | Name | Meaning |
| --- | --- | --- |
| `0` | Complete | The whole message fits in this one packet |
| `1` | Start | The first packet of a longer message |
| `2` | Continue | A packet from the middle. A long message has as many of these as it needs. |
| `3` | End | The last packet |

The fourth hex digit says how many of the packet's six data bytes are used. Unused bytes are `00`.

Here's General MIDI System On, `F0 7E 7F 09 01 F7`, as a single Complete packet, `30047E7F 09010000`:

| Hex digits | Meaning |
| --- | --- |
| `3` | Message type 3, System Exclusive |
| `0` | Group 1 |
| `0` | Complete. The whole message is in this packet. |
| `4` | Four data bytes are used |
| `7E7F` | The first two data bytes |
| `0901` | The next two data bytes, at the start of the second word |
| `0000` | Unused |

A longer message needs more packets. This made-up message uses `7D`, the ID set aside for research and non-commercial use, and has 15 bytes between the `F0` and the `F7`:

`F0 7D 01 02 03 04 05 06 07 08 09 0A 0B 0C 0D 0E F7`

As UMP, it's a Start, a Continue, and an End:

| UMP words | Packet | Bytes used | Data bytes |
| --- | --- | --- | --- |
| `30167D01 02030405` | Start | 6 | `7D 01 02 03 04 05` |
| `30260607 08090A0B` | Continue | 6 | `06 07 08 09 0A 0B` |
| `30330C0D 0E000000` | End | 3 | `0C 0D 0E` |

To turn it back into MIDI 1.0 bytes, write `F0`, then the data bytes from each packet in order, then `F7`. MIDI Monitor can do this for you: select the packets, right-click, and choose **Copy as SysEx bytes**.

A few things are good to know when you read SysEx as UMP:

- **A packet can hold fewer than six bytes.** Six is the most a packet holds, not the number it always holds. A Start or Continue packet with fewer than six bytes doesn't mean the message is over. Only an End packet does that. SysEx from a USB MIDI 1.0 device often arrives a few bytes at a time, so you may see packets with only two or three bytes. An End packet can even have no data bytes at all, when the `F7` was the only thing left.
- **What can cut in hasn't changed.** On the same group, only System Real Time messages, like clock, can go between the packets of a SysEx message. Anything else on that group ends it, as it does in MIDI 1.0. Messages on other groups aren't affected. When Windows converts MIDI 1.0 bytes, a clock byte that cut into a SysEx message becomes its own packet, next to the SysEx packets instead of inside one.
- **The data is still 7-bit.** Every data byte is `00` to `7F`, the same as in MIDI 1.0, which is why MIDI Monitor calls these **SysEx 7-bit** messages. Message type 5 has a System Exclusive 8 message that carries full 8-bit bytes, but it can't be converted to MIDI 1.0, so MIDI 1.0 devices and older apps never see it.

## Automatic translation

You never have to convert any of this yourself. Windows MIDI Services translates between MIDI 1.0 bytes and UMP wherever it's needed, so every device and every app gets messages in a form it understands.

- **MIDI 1.0 devices.** Messages from a MIDI 1.0 device become UMP as they arrive, using types 1, 2, and 3 exactly as shown in the tables above. Each of the device's ports becomes a group. Messages going to the device are turned back into MIDI 1.0 bytes.
- **Apps that use the older Windows MIDI APIs.** Many music apps still use WinMM or WinRT MIDI 1.0, which only understand MIDI 1.0 bytes. Windows converts in both directions, so these apps send and receive the same bytes they always have, SysEx included, and each group shows up to them as a port. If an app sends with running status, Windows fills the status back in before the message becomes UMP.
- **MIDI 2.0 messages on their way to MIDI 1.0.** When a type 4 message goes to a MIDI 1.0 device, or to an app that uses the older APIs, Windows changes it into the matching MIDI 1.0 messages. Values are scaled down to 7 or 14 bits. A Registered or Assignable Controller becomes the RPN or NRPN Control Changes, and a Program Change that includes a bank becomes Bank Select MSB, Bank Select LSB, and Program Change, in that order. If a Note On's velocity would scale down to 0, Windows sends 1 instead, so it isn't mistaken for a Note Off. Windows also changes type 4 into type 2 for a MIDI 2.0 device that has agreed to use the MIDI 1.0 protocol.
- **Messages with no MIDI 1.0 version are left out** when they're headed to a MIDI 1.0 device or an older app. That includes per-note controllers, per-note pitch bend, relative controllers, System Exclusive 8, Mixed Data Set, Flex Data, UMP Stream, and Utility messages.
- **Apps that use the Windows MIDI Services API get messages in the protocol the device used.** Windows doesn't turn type 2 messages into type 4, so a MIDI 1.0 keyboard's notes arrive as type 2, even in an app that prefers MIDI 2.0. Windows also doesn't turn a Note On with velocity 0 into a Note Off, because some devices, such as Mackie Control surfaces, depend on the difference.

That's why MIDI-OX and Pocket MIDI, which both use WinMM, still show you MIDI 1.0 bytes, even from a MIDI 2.0 keyboard, while MIDI Monitor shows you the UMP the keyboard sent.

To see where each conversion happens for each kind of driver, read [Understanding Data Translation for MIDI Messages]({{ site.baseurl }}/kb/data-translation/).

## Trying it yourself

The best way to get comfortable with UMP is to watch it.

- **[MIDI Monitor]({{ site.baseurl }}/tools/midi2monitor/)** shows each message as UMP words, with a plain description in the **Details** column. Select some messages and right-click. **Copy as MIDI 1.0 bytes** gives you type 1 and type 2 messages as the bytes you know, and **Copy as SysEx bytes** joins SysEx packets back into `F0 ... F7` messages.
- **[MIDI Scratch Pad]({{ site.baseurl }}/tools/midiscratchpad/)** lets you type a message as MIDI 1.0 bytes or as UMP words, and send it. Send to **Default App Loopback (A)** while MIDI Monitor watches **Default App Loopback (B)**, and you'll see exactly how your bytes turn into UMP.
- **[MIDI Console]({{ site.baseurl }}/tools/console/)** does the same from a command prompt. `midi endpoint monitor` shows the UMP words as they arrive, and `midi endpoint send-message` sends any UMP you type.

Windows MIDI Services lets more than one app use a device at the same time. That means you can open MIDI-OX, Pocket MIDI, or another MIDI 1.0 tool, on the same device as MIDI Monitor and compare the two side by side.

## Specifications

This article covers the parts you'll run into most. The full details, including every MIDI 2.0 message and the exact translation rules, are in the *Universal MIDI Packet (UMP) Format and MIDI 2.0 Protocol* specification. You'll find it, along with every other MIDI specification, on the MIDI Association's specifications page at [https://midi.org/specs](https://midi.org/specs). The same page also has a summary of the MIDI 1.0 messages, if you'd like a refresher on the bytes.
