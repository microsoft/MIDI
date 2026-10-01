---
layout: kb
title: How to control your DAW with Mackie Control in MIDI Glass
audience: everyone
description: Use Windows MIDI Glass as a Mackie Control surface, with faders your DAW moves and buttons your DAW lights, and choose how MIDI Glass talks to each of your devices.
categories:
  - Getting Started
---

Windows MIDI Glass ("MIDI Glass") is the Windows MIDI Services app for building your own touch control surface. Most DAWs can be run from a **Mackie Control** surface: a desk of faders, knobs and buttons that moves and lights up along with the DAW. MIDI Glass can be one. Its faders move when the DAW moves them, and its buttons light up when the DAW says a track is muted, soloed or ready to record.

> **MIDI Glass is a preview app.** Names and settings in this article can change before it ships.

On this page:

- [Mackie Control and HUI](#mackie-control-and-hui)
- [What you need](#what-you-need)
- [Start from the Mackie Control starter](#start-from-the-mackie-control-starter)
- [Set up your DAW](#set-up-your-daw)
- [Using it](#using-it)
- [Build your own](#build-your-own)
- [How MIDI Glass talks to each device](#how-midi-glass-talks-to-each-device)

## Mackie Control and HUI

Mackie made two different control surface protocols, and a DAW has to be told which one it's talking to.

- **Mackie Control**, sometimes called Mackie Control Universal or MCU, is the one MIDI Glass speaks. Most DAWs have a setting for it.
- **HUI** is older, and it's what Pro Tools uses. MIDI Glass doesn't speak HUI yet.

That's why many hardware control surfaces have a switch between the two.

## What you need

MIDI Glass and your DAW talk to each other through a **loopback**. A loopback is a pair of MIDI endpoints, A and B. Whatever goes into one side comes out of the other, in both directions.

Windows MIDI Services comes with one pair, **Default App Loopback (A)** and **Default App Loopback (B)**. MIDI Glass uses side A, and your DAW uses side B for both its input and its output. If you already use that pair for something else, make another one with [Windows MIDI Loopback Setup]({{ site.baseurl }}/tools/midiloopbacksetup/).

## Start from the Mackie Control starter

1. In the MIDI Glass library, select **New layout**.
2. Give it a name. Under **Send to**, pick **Default App Loopback (A)**.
3. Under **Start from**, pick **Mackie Control**.
4. Select **Create**. The layout opens in the editor.
5. Open the **Layout…** menu and select **Save and run**.

The starter has:

| Part | What's in it |
| --- | --- |
| Eight channel strips | A V-Pot knob, **Rec**, **Solo**, **Mute** and **Select** buttons, and a fader for each strip. |
| Master | The master fader. |
| Transport | **Rewind**, **Forward**, **Stop**, **Play**, **Record** and **Cycle**. |
| Banks | **Bank left** and **Bank right** move the eight strips across your tracks eight at a time. **Channel left** and **Channel right** move them one at a time. |
| Cursor | **Up**, **Down**, **Left**, **Right** and **Zoom**. |
| Jog wheel | Moves the playhead. |

## Set up your DAW

Every DAW puts this in a different place, but the steps are the same:

1. Open the DAW's settings for control surfaces or remote devices. Look for words like **Control Surfaces**, **Remote Devices** or **External Devices**.
2. Add a **Mackie Control** surface. Don't let the DAW search for one on its own. MIDI Glass doesn't answer the handshake some DAWs use to find a surface, so add it by hand.
3. Set the surface's MIDI input and its MIDI output to **Default App Loopback (B)**.
4. Make sure side B isn't also turned on as an ordinary MIDI input for recording. If it is, the DAW can record your fader moves as notes and pitch bend.

Then move a fader in MIDI Glass. The DAW's first track volume should move with it.

## Using it

- **Faders** set each track's volume. While your finger is on a fader, the DAW knows you're holding it, which matters for automation. When the DAW moves a fader, for example while it plays back automation, the fader in MIDI Glass moves too.
- **Buttons** light up the way the DAW tells them to, and some blink. For example, a DAW might blink **Record** while it waits to start recording. If Windows is set to show fewer animations, a blinking button stays lit instead.
- **V-Pots and the jog wheel** send how far you turn them, not where they are. Each springs back to the middle when you let go, so you can keep turning in steps.

## Build your own

You can add Mackie Control to any layout, or build a surface of your own from scratch.

1. Open the **Layout…** menu and select **Pages and devices…**. Then select **Outputs** in the list on the left.
2. On the card for your DAW's loopback, next to **Talk to it in**, pick **Mackie Control**.
3. Select a control and open its **Sends** tab. For a row that goes to that device, the **What** list now lists Mackie Control functions, such as **Play**, **Mute 3** or **Fader 1**.

What a control can do depends on what kind of control it is:

| Control | Mackie Control functions |
| --- | --- |
| Button, pad or toggle | Transport, the strip buttons, banks and cursor keys, F1 to F8, views, assign, automation and utility buttons |
| Fader | Fader 1 to 8 and the master fader |
| Knob, wheel or turntable | V-Pot 1 to 8 and the jog wheel |

A function sets its own message, so there's no channel, number or value to fill in. One row does the whole job: a press and a release for a button, or the position and the touch for a fader. The DAW's lights and fader moves come back on their own, with nothing to set on the **Listens** tab.

If you switch a device that already has rows to Mackie Control, a row that already sends a Mackie Control message becomes that function, as long as it fits its control. For example, note 94 on channel 1 on a button becomes **Play**. Any other row says **Pick a function**, and it doesn't send anything until you pick one. Switch the device back, and each function becomes the plain message it stands for.

## How MIDI Glass talks to each device

Every device on the **Outputs** page has its own **Talk to it in** setting. It decides what the controls can send there and how you type their values.

| Setting | Values are typed as | Use it when |
| --- | --- | --- |
| **MIDI 2.0** | Percentages, at the highest resolution. Windows converts them for a MIDI 1.0 device. | You want smooth, fine control. This is the setting for a new device. |
| **MIDI 1.0** | The exact numbers MIDI 1.0 manuals print: 0 to 127, or 0 to 16383 for pitch bend. | You're copying values from a device's manual, or a value is a code, like a pad color, rather than a level. |
| **Mackie Control** | Named functions, as above. | The device is your DAW, set up to use a Mackie Control surface. |

On a control's **Sends** tab, the value fields follow the row's device:

- A note row has **Velocity**, how hard the note plays, and **Release velocity**, the velocity of its note off. A row sent only when the control turns on, or only when it turns off, shows just the one it sends.
- A controller, pitch bend or pressure row has **From** and **To**, the values it sends at each end of the control's travel. A row sent only when the control turns on, turns off, is touched or is let go has a single **Value**.

When you change a device's setting, the values you've already typed keep their meaning. For example, 100 out of 127 on a MIDI 1.0 device becomes 78.7 % on a MIDI 2.0 one.
