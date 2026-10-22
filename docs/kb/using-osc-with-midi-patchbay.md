---
layout: kb
title: Using OSC with Windows MIDI Patchbay
description: How to send and receive MIDI as OSC messages with REAPER, TouchOSC, and other apps, using an OSC connection in Windows MIDI Patchbay.
audience: everyone
categories:
  - Getting Started
---

Open Sound Control (OSC) is a way for music apps and devices to send each other messages over a network, or between apps on one PC. An **OSC connection** in [Windows MIDI Patchbay]({{ site.baseurl }}/tools/midipatchbay/) turns MIDI into OSC messages and back, so an OSC app can play your synths, and your keyboards can play it.

This article walks through two common setups. The [OSC connections section]({{ site.baseurl }}/tools/midipatchbay/#osc-connections) of the Windows MIDI Patchbay page explains every setting.

## Before you start

- **Ports.** An OSC app listens for messages on a port, a number from 1 to 65535, and sends to a port on the other end. The two ends have to agree: what one sends to, the other listens on. Many apps send to 8000 and listen on 9000 unless you change them, so a new OSC connection starts the other way around: it listens on 8000 and sends to 9000.
- **Addresses.** To reach an app on this PC, send to `127.0.0.1`. To reach another computer, phone, or tablet, use its name or its IP address, like `192.168.1.20`. To find this PC's address, open **Settings**, then **Network & internet**, then select your network and look for **IPv4 address**.
- **Windows Firewall.** Messages from other devices only reach Windows MIDI Patchbay once Windows Firewall lets it in. If a bar at the top of the patch window says other devices can't reach your OSC connections, select the kinds of networks to allow, then **Allow through Windows Firewall**. Apps on this PC can always reach it.

## Play REAPER from a keyboard

REAPER can take MIDI over OSC into its virtual keyboard, which any track can record from. This sends from Windows MIDI Patchbay to REAPER. REAPER doesn't send its virtual keyboard back out as OSC.

In REAPER:

1. Open **Options**, then **Preferences**, then **Control/OSC/web**.
2. Select **Add**, and choose **OSC (Open Sound Control)** as the control surface mode.
3. Set **Mode** to **Local port [receive only]**. **Local listen port** is `8000` already. Leave **Pattern config** as **Default**.
4. Select **OK** on each window. If Windows asks whether to let REAPER through the firewall, you only need to allow it when Windows MIDI Patchbay runs on a different computer.
5. On a track, set the record input to **MIDI**, then **Virtual MIDI Keyboard**, arm the track, and turn on monitoring. To hear notes, put an instrument on the track.

In Windows MIDI Patchbay:

1. Open the patch, open the **Endpoints** tab, and drag **OSC connection** onto the canvas.
2. Choose **REAPER virtual keyboard**. It sends to `127.0.0.1`, port `8000`, and doesn't listen, because REAPER sends nothing back.
3. Select **Play a test note**. REAPER's track meter moves, and if the track has an instrument, you hear middle C.
4. Select **Add**, then connect your keyboard's **Out** to the OSC connection's **In**, and start the patch routing.

Notes, controllers, program changes, channel pressure, poly pressure, and pitch bend all reach REAPER on the channel they were played on. REAPER counts channels from 0, and reads pitch bend as 0 to 16383 with 8192 in the middle, which is what this preset sends.

To play REAPER on another PC or a Mac, send to that computer's address instead of `127.0.0.1`.

## Play your synths from TouchOSC

TouchOSC has no MIDI over OSC of its own. Each control sends the OSC message you set up for it, so you choose messages that match a format Windows MIDI Patchbay reads. The **Paths** format is the easiest:

| To send | Address | Value |
| --- | --- | --- |
| A note on channel 1 | `/midi/1/note/60` | The velocity. 0, or a button let go, is a note off. |
| A control change on channel 1 | `/midi/1/cc/7` | The value |
| Pitch bend on channel 1 | `/midi/1/pitch` | The bend |

In Windows MIDI Patchbay:

1. Drag **OSC connection** onto the canvas and choose **Paths**.
2. Leave **Listen on port** at `8000`. In **Send to**, type your phone or tablet's address, with port `9000`. If its address changes from day to day, select the connection after you add it and check **Send to whoever sent to me last** instead.
3. Select **Add**. Connect the OSC connection's **Out** to your synth's **In**, and start the patch routing.
4. Select the row on the OSC connection, and set **Values sent as** to **0 to 1**. TouchOSC controls go from 0 to 1 unless you scale them, so this turns a fader all the way up into 127.

In TouchOSC, set up a connection that sends to this PC's address on port `8000` and listens on port `9000`, then give each control the address from the table above.

## When nothing happens

Select the OSC connection on the canvas. The details panel shows:

- **Whether it's listening.** If another app has the port, it says which one. Pick another port, or close that app.
- **Recently heard.** The last messages that arrived, and whether a row took each one. If messages arrive but no row takes them, check the address the app sends. When it looks like one of the formats, **Add a row for this** adds a row that takes it.
- **Who sent them.** By default, an OSC connection only takes messages from the computer it sends to. A message from anywhere else shows as kept out. Change **Take messages from** to **Anyone** to take them all.
- **Activity.** How many messages came in and went out, and how many couldn't be read or sent.

If nothing shows up in **Recently heard** at all, the messages aren't reaching this PC. Check the port and address in the other app, and check the Windows Firewall bar in the patch window.
