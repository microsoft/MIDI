---
layout: kb
title: Using OSC with Windows MIDI Patchbay
description: How to send and receive MIDI as OSC messages with REAPER, TouchOSC, TouchDesigner, Open Stage Control, and other apps, using an OSC connection in Windows MIDI Patchbay.
audience: everyone
categories:
  - Getting Started
---

Open Sound Control (OSC) is a way for music apps and devices to send each other messages over a network, or between apps on one PC. An **OSC connection** in [Windows MIDI Patchbay]({{ site.baseurl }}/tools/midipatchbay/) turns MIDI into OSC messages and back, so an OSC app can play your synths, and your keyboards can play it.

This article walks through setting up common apps. The [OSC connections section]({{ site.baseurl }}/tools/midipatchbay/#osc-connections) of the Windows MIDI Patchbay page explains every setting.

- [Before you start](#before-you-start)
- [Play REAPER from a keyboard](#play-reaper-from-a-keyboard)
- [Connect TouchOSC on a phone or tablet](#connect-touchosc-on-a-phone-or-tablet)
- [Connect TouchDesigner or another app that uses OSC's MIDI type](#connect-an-app-that-uses-oscs-midi-type)
- [Connect Open Stage Control](#connect-open-stage-control)
- [Use TCP instead of UDP](#use-tcp-instead-of-udp)
- [Open it from your DAW](#open-it-from-your-daw)
- [When nothing happens](#when-nothing-happens)

## Before you start

- **Ports.** An OSC app listens for messages on a port, a number from 1 to 65535, and sends to a port on the other end. The two ends have to agree: what one sends to, the other listens on. Many apps send to 8000 and listen on 9000 unless you change them, so a new OSC connection starts the other way around: it listens on 8000 and sends to 9000.
- **Addresses.** To reach an app on this PC, send to `127.0.0.1`. To reach another computer, phone, or tablet, use its name or its IP address, like `192.168.1.20`, or pick it with **Find on the network** when it announces itself. To find this PC's address, open **Settings**, then **Network & internet**, then select your network and look for **IPv4 address**.
- **Windows Firewall.** Messages from other devices only reach Windows MIDI Patchbay once Windows Firewall lets it in. If a bar at the top of the patch window says other devices can't reach your OSC connections, select the kinds of networks to allow, then **Allow through Windows Firewall**, and say yes when Windows asks for permission. Apps on this PC can always reach it.

## Play REAPER from a keyboard {#play-reaper-from-a-keyboard}

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

Notes, controllers, program changes, channel pressure, poly pressure, and pitch bend all reach REAPER on the channel they were played on. REAPER counts channels from 0, and reads pitch bend as 0 to 16383 with 8192 in the middle, which is what this preset sends. We checked this with REAPER on the same PC as Windows MIDI Patchbay.

To play REAPER on another PC or a Mac, send to that computer's address instead of `127.0.0.1`, and let REAPER through the firewall on that computer.

## Connect TouchOSC on a phone or tablet {#connect-touchosc-on-a-phone-or-tablet}

TouchOSC has no MIDI over OSC of its own. Each control sends an OSC message of its own, like `/1/fader3` with a value from 0 to 1. There are two ways to use them: as they come, with the **TouchOSC** preset, or with addresses you give each control.

There are two versions of TouchOSC:

- **TouchOSC Mk1**, the older version, uses UDP, with one connection: a **Host**, a **Port (outgoing)**, and a **Port (incoming)**. We checked everything in this section with TouchOSC Mk1 on an iPhone.
- **The current TouchOSC** can also use TCP, and can have several connections. Hexler's [TouchOSC manual](https://hexler.net/touchosc/manual) explains its settings. We haven't checked it with Windows MIDI Patchbay yet. For TCP, see [Use TCP instead of UDP](#use-tcp-instead-of-udp).

### Find each other on the network {#find-each-other-on-the-network}

Windows MIDI Patchbay and TouchOSC can find each other, so you don't have to type addresses:

1. In Windows MIDI Patchbay, start the patch with the OSC connection routing. If the Windows Firewall bar shows, select **Allow through Windows Firewall**. The connection can't be found until Windows Firewall lets Windows MIDI Patchbay in.
2. In TouchOSC Mk1, open the settings and tap the **OSC** row. Make sure **Port (incoming)** has a number, like `9000`: TouchOSC only announces itself when it does.
3. Below the settings, TouchOSC lists the OSC apps it finds. The OSC connection is listed by its name and this PC's name, like `Patchbay OSC (STUDIO-PC)`. Tap it, and TouchOSC fills in **Host** and **Port (outgoing)**.
4. In Windows MIDI Patchbay, select the OSC connection, then **Find on the network**. TouchOSC is listed as your device's name and model, followed by `(TouchOSC)`, like `iPhone [iPhone] (TouchOSC)`. Pick it to fill in **Send to** and **Port**.

### Use TouchOSC's layouts as they come

The quickest way, with nothing to change in TouchOSC:

1. In Windows MIDI Patchbay, drag **OSC connection** onto the canvas and choose **TouchOSC**.
2. Leave **Listen on port** at `8000`. In **Send to**, type your phone or tablet's address, with port `9000`, or pick it with **Find on the network**. If its address changes from day to day, select the connection after you add it and check **Send to whoever sent to me last** instead.
3. Select **Add**. Connect the OSC connection's **Out** to your synth's **In**, and start the patch routing.
4. In TouchOSC, set up a connection that sends to this PC's address on port `8000` and listens on port `9000`. Or pick this PC from the list, as in [Find each other on the network](#find-each-other-on-the-network).

The TouchOSC preset is a row of rules: faders `/1/fader1` to `/1/fader5` are controllers 71 to 75, toggles `/1/toggle1` to `/1/toggle4` are controllers 80 to 83, and push buttons `/1/push1` and up play notes from middle C, all on channel 1. The rules work both ways, so a controller the patch sends moves the fader on your phone.

For finer control than MIDI 1.0's 128 steps, select the OSC connection and check **Make MIDI 2.0 messages** on the row. A fader then sets its controller with 32-bit precision. Turn it on only when the synth or app it connects to takes MIDI 2.0.

For other controls, select the OSC connection and move them. **Recently heard** lists what arrives, and **Add a rule for this** adds a rule for each control, or for a whole set of them, like every fader on page 2. To change what a rule does, select the OSC connection and find the rule under **Rows**.

### Or set up each control yourself

You can also give each control an address that matches a format Windows MIDI Patchbay reads. The **Paths** format is the easiest:

| To send | Address | Value |
| --- | --- | --- |
| A note on channel 1 | `/midi/1/note/60` | The velocity. 0, or a button let go, is a note off. |
| A control change on channel 1 | `/midi/1/cc/7` | The value |
| Pitch bend on channel 1 | `/midi/1/pitch` | The bend |

Set up the OSC connection as above, but choose **Paths**.

TouchOSC controls go from 0 to 1 unless you change them. Pick one of these, so the numbers come out right:

- **Change each control's range** to the MIDI numbers: 0 to 127 for a control change, 0 to 16383 for pitch bend, and 0 to the velocity you want for a note button. A button sends the top of its range when you press it and the bottom when you let go, so 0 to 100 plays the note at velocity 100 and stops it when you let go.
- **Or leave the controls at 0 to 1.** Select the row on the OSC connection, and set **Values sent as** to **0 to 1**. Then a fader all the way up is 127, and a button plays at velocity 127.

In TouchOSC, set up a connection that sends to this PC's address on port `8000` and listens on port `9000`, then give each control the address from the table above. In TouchOSC Mk1, the older version, you do this in the free TouchOSC Editor on your PC, from [Hexler's TouchOSC Mk1 page](https://hexler.net/touchosc-mk1#resources): clear the **auto** box under OSC, type the address, and set the **Value Range**. Then select **Sync**, and on the phone go to **Layout**, then **Add**, and pick your PC.

It works the other way too. Connect something to the OSC connection's **In**, and a TouchOSC fader with the same address, such as `/midi/1/cc/11`, moves to follow it.

## Connect an app that uses OSC's MIDI type {#connect-an-app-that-uses-oscs-midi-type}

OSC has a type of its own for MIDI messages. Some apps, like TouchDesigner, can read it. TouchDesigner's documentation lists it as a type its **OSC In DAT** reads. We haven't checked it with TouchDesigner.

1. In Windows MIDI Patchbay, add an OSC connection and choose **MIDI type tag**. Each message goes to `/midi`, with one MIDI message in it.
2. Set **Send to** to the computer the app runs on, and the port it listens on. Set **Listen on port** to the port it sends to.
3. In the app, listen on that port. To play into the patch, the app sends messages of the MIDI type to `/midi` on this PC.

Each message also carries a port number, which matches the row's group: the row on group 1 uses port 0, the row on group 2 uses port 1, and so on.

## Connect Open Stage Control {#connect-open-stage-control}

Open Stage Control's documentation names its MIDI messages `/note`, `/control`, `/program`, `/pitch`, and so on, with the channel first and the numbers after it. The **Open Stage Control** preset reads and writes those names, with channels 1 to 16. We haven't checked it with Open Stage Control.

1. In Windows MIDI Patchbay, add an OSC connection and choose **Open Stage Control**.
2. Set **Send to** to the computer Open Stage Control runs on, and the port it listens on. Set **Listen on port** to the port it sends to.
3. In Open Stage Control, send to this PC on that port, with the message names and the order of numbers its MIDI documentation lists. For example, a control change is `/control` with the channel, the controller number, and the value.

The [OSC connections section]({{ site.baseurl }}/tools/midipatchbay/#rows-and-formats) of the Windows MIDI Patchbay page lists what each preset sends.

## Use TCP instead of UDP {#use-tcp-instead-of-udp}

Almost every OSC app uses UDP, where each message travels on its own. Some apps can also use TCP, which never loses a message, so it suits big SysEx. With TCP, one side waits for the other to connect, and messages go both ways on that connection.

1. Select the OSC connection, and under **Network**, set **Protocol**:
   - **TCP, connect to the other app**, when the other app waits for connections. Set **Send to** and **Port** to where it waits.
   - **TCP, wait for apps to connect**, when the other app connects to this PC. Set **Listen on port**. Up to 8 apps can connect at once, and each gets everything the connection sends.
2. Set **TCP framing** to what the other app uses: **SLIP (OSC 1.1)** for most apps, or **Size first (OSC 1.0)**.

If the connection drops, Windows MIDI Patchbay connects again every 2 seconds. We checked both framings, both ways, with a separate OSC test program on this PC.

## Open it from your DAW {#open-it-from-your-daw}

Your DAW can use an OSC connection like any MIDI device, so an OSC app can play a track, and its controls can follow the DAW, with no loopback in between:

1. Select the OSC connection, and check **Show to other apps as a MIDI device**.
2. Start the patch routing.
3. In your DAW, pick the OSC connection by its name as a MIDI input and output. Each row is a block of its own, on its group.

The device is only there while the patch routes. We checked it with the [MIDI Console]({{ site.baseurl }}/tools/console/), but not yet with a DAW.

## When nothing happens

Select the OSC connection on the canvas. The details panel shows:

- **Whether it's listening.** If another app has the port, it says which one. Pick another port, or close that app. If it says it's listening for apps on this PC only, see the Windows Firewall bar at the top of the patch window.
- **Recently heard.** The last messages that arrived, and whether a row took each one. If messages arrive but no row takes them, check the address the app sends. When it looks like one of the formats, **Add a row for this** adds a row that takes it. When it doesn't, **Add a rule for this** adds a rule for it.
- **Who sent them.** By default, an OSC connection only takes messages from the computer it sends to. A message from anywhere else shows as kept out. Change **Take messages from** to **Anyone** to take them all.
- **Activity.** How many messages came in and went out, and how many couldn't be read or sent.

If nothing shows up in **Recently heard** at all, the messages aren't reaching this PC. Check the port and address in the other app, and check the Windows Firewall bar in the patch window. If two apps can't find each other on the network, type the address and port instead. Some networks don't pass along the announcements apps use to find each other.