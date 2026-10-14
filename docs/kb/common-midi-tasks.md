---
layout: kb
title: "How do I...? Common MIDI tasks, and which tool to use"
audience: everyone
description: Short answers to common questions, like how to clock a rig without a DAW or play a synth on a Mac from a keyboard on your PC, and which tool does each job.
categories:
  - Getting Started
---

Windows MIDI Services comes with a lot of tools, and most jobs can be done more than one way. This page starts from the job instead of the tool. Each answer is short, and points you to the tool page that has the details. Select a question to see its answer.

> The apps on this page will be released to consumers in 2027. For developers and technical customers, you may download the latest preview releases from [GitHub](https://aka.ms/midireleases). Everyone else should wait until the software is delivered with Windows 11. Bluetooth MIDI, Network MIDI 2.0, RTP-MIDI, and the General MIDI synthesizer are previews too, and they're installed separately.

On this page:

- [Getting started](#getting-started)
- [Playing and performing](#playing-and-performing)
- [Connecting apps, devices, and computers](#connecting-apps-devices-and-computers)
- [Routing, filtering, and changing MIDI](#routing-filtering-and-changing-midi)
- [Clock and sync](#clock-and-sync)
- [SysEx, patches, and firmware](#sysex-patches-and-firmware)
- [Watching, testing, and recording](#watching-testing-and-recording)
- [Automating and scripting](#automating-and-scripting)
- [Backing up and moving your setup](#backing-up-and-moving-your-setup)
- [Fixing problems](#fixing-problems)

## Getting started

<details id="see-every-midi-device" markdown="1">
<summary>See every MIDI device on my PC</summary>

Open [Windows MIDI Settings]({{ site.baseurl }}/tools/settings/). It lists every MIDI device on the PC, which Windows calls an endpoint, and keeps the list up to date as you plug things in and take them out. Select one to see its details, including the MIDI 1.0 ports that older apps see.

From a command prompt, `midi enumerate endpoints` in the [MIDI Console]({{ site.baseurl }}/tools/console/#enumerate-midi-services-endpoints) lists the same devices. In PowerShell, use `Get-MidiEndpointDeviceInfo` from the [MIDI PowerShell cmdlets]({{ site.baseurl }}/tools/powershell/).

</details>

<details id="use-a-device-in-two-apps" markdown="1">
<summary>Use one MIDI device in two apps at the same time</summary>

Open it in both apps. There's nothing to set up. Windows MIDI Services lets more than one app use a device at once, so a DAW and Windows MIDI Monitor, or two DAWs, can listen to the same keyboard and send to the same synth.

This is one of the things you give up in Legacy API mode, where a device belongs to one app at a time, the way it did before Windows MIDI Services. See [How to change the API mode]({{ site.baseurl }}/kb/how-to-change-api-mode/).

</details>

<details id="rename-a-device" markdown="1">
<summary>Rename a device, or give it a picture</summary>

In [Windows MIDI Settings]({{ site.baseurl }}/tools/settings/#customizing-an-endpoint), select the device, then **Customize**. The name, description, and picture you choose show up in apps that use Windows MIDI Services, and they come back each time you plug the device in.

Older apps see a device's MIDI 1.0 ports instead, so name those with **Edit port names** in the same dialog. New port names take effect when the MIDI service restarts. To do this from a script, use `midi endpoint customize` in the [MIDI Console]({{ site.baseurl }}/tools/console/#endpoint-customize).

</details>

<details id="play-without-a-hardware-synth" markdown="1">
<summary>Play MIDI without a hardware synth</summary>

Use the General MIDI synthesizer that comes with Windows MIDI Services. It plays through your PC's default audio device, and apps list it as **General MIDI Synth**. [Windows MIDI Keyboard]({{ site.baseurl }}/tools/midikeyboard/), [Windows MIDI Player]({{ site.baseurl }}/tools/midiplayer/), and [Windows MIDI Sequencer]({{ site.baseurl }}/tools/midisequencer/) can all play it.

Turn it on or off, and change how it sounds, under **Global MIDI Settings** in [Windows MIDI Settings]({{ site.baseurl }}/tools/settings/#global-midi-settings), or with `midi synth` in the [MIDI Console]({{ site.baseurl }}/tools/console/#synth). [How the built-in General MIDI synthesizer works]({{ site.baseurl }}/kb/in-box-general-midi-synth/) has the details.

</details>

## Playing and performing

<details id="use-a-tablet-as-a-keyboard" markdown="1">
<summary>Use a Surface or other Windows tablet as a virtual keyboard</summary>

[Windows MIDI Keyboard]({{ site.baseurl }}/tools/midikeyboard/) is the quickest way. You can play several keys at once with your fingers, and where you strike a key sets how hard the note plays: softer near the top, harder near the bottom. Sliding a finger up a key you're holding can send aftertouch, the mod wheel, or a MIDI 2.0 per-note controller.

For a playing surface of your own, build a layout in [Windows MIDI Glass]({{ site.baseurl }}/tools/midiglass/#adding-controls). Note pads and hex pads take a finger on each pad, so you can play chords, and they can stay in one key and scale. Put faders, knobs, and buttons on the same page, and run it full screen.

To play an app on the same tablet, send to **Default App Loopback (A)** and set the app to listen to **Default App Loopback (B)**. To play another computer, connect the two first. See [Connect two Windows PCs](#connect-two-windows-pcs).

</details>

<details id="create-a-dj-remote" markdown="1">
<summary>Create a remote control for a DJ rig</summary>

In [Windows MIDI Glass]({{ site.baseurl }}/tools/midiglass/), select **New layout** and pick the **DJ deck** template. You get two decks with filters, three band EQ, cue pads, and a crossfader. Most DJ apps have a MIDI learn feature, so turn that on in the DJ app and move each control in the layout to map it. Run the layout full screen on a touch screen or a tablet.

If the DJ app is on the same PC, send the layout to **Default App Loopback (A)** and have the DJ app listen to **Default App Loopback (B)**. If it's on another PC, [connect the two computers](#connect-two-windows-pcs) first, and send the layout to that connection.

Want just a few buttons over the DJ app? Make a [floating toolbar]({{ site.baseurl }}/kb/midi-glass-floating-toolbars/). An Xbox controller or a joystick can be a remote too, with the [game controller steps]({{ site.baseurl }}/tools/midipatchbay/#game-controllers) in Windows MIDI Patchbay.

</details>

<details id="control-a-daw-from-a-touch-screen" markdown="1">
<summary>Control my DAW from a touch screen</summary>

[Windows MIDI Glass]({{ site.baseurl }}/tools/midiglass/) can act as a Mackie Control surface, which most DAWs support. Its faders move when the DAW moves them, and its buttons light up for mute, solo, and record. Start a new layout from the **Mackie Control** template, then follow [How to control your DAW with Mackie Control in Windows MIDI Glass]({{ site.baseurl }}/kb/midi-glass-mackie-control/). Pro Tools uses HUI instead, which Windows MIDI Glass doesn't support yet.

For a few buttons that float over the DAW, see [How to make a floating toolbar or palette in Windows MIDI Glass]({{ site.baseurl }}/kb/midi-glass-floating-toolbars/).

</details>

<details id="use-a-game-controller" markdown="1">
<summary>Turn a game controller into a MIDI controller</summary>

[Windows MIDI Patchbay]({{ site.baseurl }}/tools/midipatchbay/#game-controllers) has game controller steps for Xbox controllers, including the Xbox Adaptive Controller, and for joysticks, flight sticks, wheels, and gamepads. A button can play a note, hold the sustain pedal, or step through programs. A stick or trigger can bend the pitch or move a controller. Add a step for each control, connect it to your instrument, and turn on routing.

The steps keep working while another app, such as a game or your DAW, is in front, so you'll want to turn this off when using your controllers for an actual game :).

</details>

<details id="test-a-synth" markdown="1">
<summary>Check that a synth makes a sound, without plugging in a keyboard</summary>

Open [Windows MIDI Keyboard]({{ site.baseurl }}/tools/midikeyboard/), pick the synth on the top row, and click or tap the keys. Turn on **Arp** and **Latch** for a steady pattern you don't have to keep playing, so your hands are free for the synth's knobs.

From a command prompt, `midi endpoint play-notes 60 64 67` in the [MIDI Console]({{ site.baseurl }}/tools/console/#endpoint-play-notes) plays three notes. To send an exact message from the synth's manual, use [Windows MIDI Scratch Pad]({{ site.baseurl }}/tools/midiscratchpad/).

</details>

<details id="stop-a-stuck-note" markdown="1">
<summary>Stop a stuck note</summary>

Select **Panic** next to the device in [Windows MIDI Settings]({{ site.baseurl }}/tools/settings/#endpoints). It sends All Sound Off, Reset All Controllers, and All Notes Off on every channel of the device. [Windows MIDI Keyboard]({{ site.baseurl }}/tools/midikeyboard/) and a running [Windows MIDI Glass]({{ site.baseurl }}/tools/midiglass/#running-a-layout) layout have a **Panic** button too. In Windows MIDI Glass, Ctrl+Shift+P does the same thing.

</details>

<details id="play-a-midi-file" markdown="1">
<summary>Play a MIDI file</summary>

Open it in [Windows MIDI Player]({{ site.baseurl }}/tools/midiplayer/), or drag it onto the window, and choose an instrument under **Play to**. It plays to any MIDI device, a loopback, or the General MIDI synth, and shows the notes as they play. It's also in the **Open with** menu for MIDI files in File Explorer.

To play a file from a script, use `Start-MidiFilePlayback` from the [MIDI PowerShell cmdlets]({{ site.baseurl }}/tools/powershell/#midi-file-cmdlets). To change the music, import the file into [Windows MIDI Sequencer]({{ site.baseurl }}/tools/midisequencer/#importing-and-exporting).

</details>

<details id="make-music-without-a-daw" markdown="1">
<summary>Write a song or a pattern without a DAW</summary>

[Windows MIDI Sequencer]({{ site.baseurl }}/tools/midisequencer/) puts MIDI on a timeline. Draw notes in a piano roll, set steps in a pattern, let a generator make a part, or record what you play. Its clip launcher plays clips and scenes live, the way a groove box does. Each track plays to one device: a hardware synth, an app on a loopback, or the General MIDI synth.

It works with MIDI only, not audio. It's also a very early preview, so save often.

</details>

## Connecting apps, devices, and computers

<details id="connect-daws-on-the-same-pc" markdown="1">
<summary>Communicate between DAWs on the same PC, using MIDI</summary>

Use a loopback, which works like a MIDI cable between apps. Windows MIDI Services comes with one: what one app sends to **Default App Loopback (A)** comes out of **Default App Loopback (B)** in the other app, and the other way around. To make more, with names that say what they're for, use [Windows MIDI Loopback Setup]({{ site.baseurl }}/tools/midiloopbacksetup/).

Loopbacks belong to the PC, not to an app, and saved ones come back after a restart. To keep the two DAWs in time, have one send MIDI clock to its side of the loopback, and set the other to follow the clock on the other side.

Both DAWs can also use your MIDI hardware at the same time, so there's no need to pass a keyboard through one DAW to reach the other.

</details>

<details id="replace-another-loopback-tool" markdown="1">
<summary>Replace loopMIDI or another virtual MIDI cable tool</summary>

[Windows MIDI Loopback Setup]({{ site.baseurl }}/tools/midiloopbacksetup/#moving-over-from-another-loopback-tool) can make a loopback to match each port of the other tool, with the same name, so your apps find them the way they did before. On the **MIDI 1.0 Basic Loopbacks** page, select **Import from another provider...**, and tick the ports to bring over.

When it's done, remove the old ports or uninstall the other tool. Until you do, there are two ports with each name, and your apps can't tell them apart.

</details>

<details id="connect-a-bluetooth-device" markdown="1">
<summary>Connect a Bluetooth MIDI keyboard or controller</summary>

Open [Windows MIDI Bluetooth Setup]({{ site.baseurl }}/tools/midibluetoothsetup/), wait a few seconds for the device to show up, and select **Connect**. It then works in your apps like any other MIDI device, and reconnects on its own when you switch it on again. You can close the app. Most devices don't need pairing, and the app tells you when one does.

If a device doesn't show up, it may be asleep, or still connected to a phone or tablet. See [Common reasons a device may not connect]({{ site.baseurl }}/tools/midibluetoothsetup/#common-reasons-a-device-may-not-connect). From a script, use `midi bluetooth connect` in the [MIDI Console]({{ site.baseurl }}/tools/console/#bluetooth).

</details>

<details id="use-this-pc-as-a-bluetooth-device" markdown="1">
<summary>Use this PC as a Bluetooth MIDI device for a phone, tablet, or another computer</summary>

On the **This PC** page of [Windows MIDI Bluetooth Setup]({{ site.baseurl }}/tools/midibluetoothsetup/#letting-other-devices-connect-to-this-pc), select **Publish**, and leave **Protocol** on **Bluetooth LE MIDI 1.0**. Then connect to this PC from the other device's Bluetooth MIDI settings, or from a music app on it. When it connects, select **Allow once** or **Always allow**, and it shows up as a MIDI device on this PC.

From a command prompt, use `midi bluetooth peripheral start` in the [MIDI Console]({{ site.baseurl }}/tools/console/#bluetooth-peripheral).

</details>

<details id="connect-a-network-device" markdown="1">
<summary>Connect a network MIDI device or interface</summary>

Open [Windows MIDI Network Setup]({{ site.baseurl }}/tools/midinetworksetup/#finding-and-connecting-to-devices). Devices that announce themselves on your network show up on their own. Select **Connect** on the one you want. For one that doesn't, use **Connect to a device by address**. Once it's connected, it works in your apps like any other MIDI device, and it reconnects on its own.

If a connection sits at **Connecting**, look at the other device. It may be waiting for you to allow the connection. When a device offers both Network MIDI 2.0 and RTP-MIDI, use Network MIDI 2.0.

</details>

<details id="connect-a-pc-and-a-mac" markdown="1">
<summary>Communicate between a PC and Mac over MIDI</summary>

Use Network MIDI 2.0, which macOS now supports too. It carries both MIDI 2.0 and MIDI 1.0. On the Mac, it's the **MIDI 2.0** option in **MIDI Network Setup**, in the Audio MIDI Setup app. On this PC, open [Windows MIDI Network Setup]({{ site.baseurl }}/tools/midinetworksetup/). To connect to a host on the Mac, find it on the **Network devices** page and select **Connect**. To have the Mac connect to this PC, select **Create host** on the **This PC** page, then connect to it from the Mac. Either way, the connection shows up as a MIDI device in apps on both computers.

RTP-MIDI is the second choice. It's older and carries only MIDI 1.0, but Macs, iPhones, and iPads have had it for years. On the Mac, it's in the same **MIDI Network Setup** window. On this PC, it's on the **RTP-MIDI** pages of [Windows MIDI Network Setup]({{ site.baseurl }}/tools/midinetworksetup/#rtp-midi), which appear once the RTP-MIDI transport is installed. They work the same way as the Network MIDI 2.0 pages.

No network? A MIDI interface on each computer, with MIDI cables between them, works too.

</details>

<details id="connect-two-windows-pcs" markdown="1">
<summary>Connect two Windows PCs</summary>

Use Network MIDI 2.0. On one PC, open [Windows MIDI Network Setup]({{ site.baseurl }}/tools/midinetworksetup/#letting-other-devices-connect-to-this-pc) and select **Create host** on the **This PC** page. On the other PC, the host shows up on the **Network devices** page. Select **Connect**. Each PC then has a MIDI device for the other, which any app can use. If you set the host to **Ask me first**, allow the connection on that PC. See [Get notified when a remote network MIDI device wants to connect](#get-notified-of-network-connection-requests).

RTP-MIDI works between two PCs too, on the **RTP-MIDI** pages of the same app. Use it when the other PC only has RTP-MIDI.

Two PCs near each other can use Bluetooth instead. Publish one PC on the **This PC** page of [Windows MIDI Bluetooth Setup]({{ site.baseurl }}/tools/midibluetoothsetup/#letting-other-devices-connect-to-this-pc), and connect to it from the other.

If the PCs can't find each other, see [Let other computers connect to my PC over the network](#network-connections-blocked).

</details>

<details id="get-notified-of-network-connection-requests" markdown="1">
<summary>Get notified when a remote network MIDI device wants to connect</summary>

When a Network MIDI 2.0 host on this PC is set to **Ask me first**, a device that asks to connect waits until you allow it. The MIDI Notifications app tells you when one is waiting. Select the notification, and Windows MIDI Network Setup opens on that device, so you can allow or block it.

To turn it on, select **Notifications** in [Windows MIDI Settings]({{ site.baseurl }}/tools/settings/), and turn on **Show MIDI notifications** and **Network MIDI 2.0 connection requests**. **Start the notifications app when I sign in** keeps it running. It doesn't carry any MIDI, so nothing stops working when it's closed.

It only watches Network MIDI 2.0 hosts. For an RTP-MIDI host, open Windows MIDI Network Setup to see who's waiting. See [MIDI Notifications]({{ site.baseurl }}/tools/midinetworksetup/#midi-notifications).

</details>

<details id="play-a-synth-on-another-computer" markdown="1">
<summary>Play a synthesizer on a different PC or a Mac, from a MIDI keyboard attached to my PC</summary>

First connect the two computers over the network. Network MIDI 2.0 is the first choice, for another PC or a Mac, and RTP-MIDI works with both too. See [Connect two Windows PCs](#connect-two-windows-pcs) and [Communicate between a PC and Mac over MIDI](#connect-a-pc-and-a-mac).

Then send the keyboard to that connection. In [Windows MIDI Patchbay]({{ site.baseurl }}/tools/midipatchbay/), select **New quick patch**, pick the keyboard, and then pick the connection. Windows MIDI Patchbay routes only while it's running, so turn on **Run in notification area**. On the other computer, set the synth app to listen to the connection from this PC.

For a quick test, `midi forward` in the [MIDI Console]({{ site.baseurl }}/tools/console/#forward) passes everything from one device to another until you press Esc.

</details>

<details id="keep-ports-while-a-device-is-off" markdown="1">
<summary>Keep a device in my DAW while it's switched off</summary>

Many DAWs drop a device from their setup if its ports are missing when the DAW starts. For Bluetooth and RTP-MIDI devices, set **Keep endpoint when offline** to **Always**, in [Windows MIDI Bluetooth Setup]({{ site.baseurl }}/tools/midibluetoothsetup/#keeping-an-endpoint-when-a-device-goes-offline) or [Windows MIDI Network Setup]({{ site.baseurl }}/tools/midinetworksetup/#keeping-the-endpoint-when-a-device-is-offline). The ports stay, and the device works again as soon as it comes back. Anything sent to it while it's off is lost.

Network MIDI 2.0 devices don't have this setting.

</details>

## Routing, filtering, and changing MIDI

Most of these answers use [Windows MIDI Patchbay]({{ site.baseurl }}/tools/midipatchbay/), where you connect devices and apps by drawing lines between them. It routes only while it's running, so turn on **Run in notification area**, and **Start with Windows** if you want your patches every time. See [Routing only runs while Windows MIDI Patchbay is running]({{ site.baseurl }}/tools/midipatchbay/#routing-only-runs-while-patchbay-is-running).

<details id="split-a-keyboard" markdown="1">
<summary>Create a virtual split keyboard going to two synth apps</summary>

In Windows MIDI Patchbay, connect the keyboard to two **Note filter** steps: one that lets through the notes below the split, and one for the notes above it. Connect each filter to a loopback of its own, and set each synth app to listen to its loopback. **Create loopback**, in the patch window, makes them for you.

If both synth apps can listen to one loopback on different channels, put a **Channel mapper** after each filter, and connect both to that one loopback. See [Filter steps]({{ site.baseurl }}/tools/midipatchbay/#filter-steps).

</details>

<details id="layer-and-merge" markdown="1">
<summary>Play two synths from one keyboard, or two keyboards into one synth</summary>

In Windows MIDI Patchbay, connect the keyboard's **Out** to the **In** of both synths, and each synth gets its own copy of every message. Put steps on either path to change one synth and not the other, such as a **Transpose** an octave down on the bass.

It works the other way too. Connect two keyboards' **Out** to one synth's **In**, and their messages are merged. See [Steps]({{ site.baseurl }}/tools/midipatchbay/#steps).

</details>

<details id="create-a-polysynth" markdown="1">
<summary>Create a virtual polysynth from a rack of monosynths</summary>

Use the **Note distributor** step in Windows MIDI Patchbay. Connect your keyboard to its **In**, and connect its **Out** to each monosynth, one connection for each voice. Each new note goes to one synth, and its note off follows it there. Four modes, such as **Take turns** and **First free voice**, decide which synth plays each note and what happens when they're all playing. Controllers and pitch bend can go to every synth, or only to the one that played the latest note.

If the synths share one MIDI port on different channels, put a **Channel mapper** on each voice's connection to move its notes to that synth's channel. See [Distribution steps]({{ site.baseurl }}/tools/midipatchbay/#distribution-steps).

</details>

<details id="change-messages-on-the-way" markdown="1">
<summary>Transpose, change the channel, or fix a backwards pedal</summary>

Put a transform step from Windows MIDI Patchbay between the device and where it's going:

- **Transpose** shifts every note up or down.
- **Channel mapper** moves messages from one channel to another.
- **Control change values** with **Invert** turns a backwards sustain pedal around. It can also stretch an expression pedal that never reaches the ends to cover the whole range.
- **Velocity rescaler** makes a light touch play louder, or plays every note at the same velocity.
- **Program and bank mapper** picks a different sound on the destination.

See [Transform steps]({{ site.baseurl }}/tools/midipatchbay/#transform-steps).

</details>

<details id="filter-messages-for-an-old-synth" markdown="1">
<summary>Filter out clock and active sense messages going to an old synth</summary>

Put a **Message type filter** from Windows MIDI Patchbay right before the synth, and use **System messages** to keep out timing clock and active sensing. Active sensing is a "still here" message that many keyboards send at least three times a second whenever nothing else is being sent, so it adds up to a lot of traffic. A synth that never receives it works normally.

If the messages come from an app, such as a DAW, have the app send to a loopback, and connect the loopback through the filter to the synth. Check the app's settings first, though. Many DAWs can turn off clock for each port.

To see whether a device sends either one, turn them on in [Windows MIDI Monitor]({{ site.baseurl }}/tools/midi2monitor/#narrowing-down-what-you-see). It hides both at first.

</details>

<details id="control-sending-speed" markdown="1">
<summary>Control how fast messages are sent to devices</summary>

USB and network connections are much faster than the 5-pin MIDI cable that older gear was built for, and some devices lose messages when a lot arrives at once. How you slow things down depends on how the messages get there:

- **Through Windows MIDI Patchbay:** put a **Message throttler** right before the device, and start at **MIDI 1.0 wire speed**. See [Slowing messages down]({{ site.baseurl }}/tools/midipatchbay/#slowing-messages-down).
- **A SysEx file:** in [Windows MIDI SysEx Utility]({{ site.baseurl }}/tools/midisysextool/#sending-a-sysex-file-to-a-device), lower **Messages per transfer** and raise **Delay between transfers**. `midi sysex send-file` in the [MIDI Console]({{ site.baseurl }}/tools/console/#sysex-send-file) has `--message-transfer-count` and `--pause` for the same job.
- **A network device:** choose a **Sending speed** for it in [Windows MIDI Network Setup]({{ site.baseurl }}/tools/midinetworksetup/#choosing-a-sending-speed).
- **A file of messages from the console:** `midi endpoint send-message-file` waits between messages for the number of milliseconds you give with `--pause`.

</details>

## Clock and sync

<details id="clock-a-rig-without-a-daw" markdown="1">
<summary>Clock a bunch of synthesizers and sequencers without using a DAW</summary>

Use [Windows MIDI Clock]({{ site.baseurl }}/tools/midiclock/). Add a clock for each device, set the tempo, and select **Start all**. Clocks started together begin on the same instant. Each one can run at half or double speed, add swing, or start a few milliseconds early for a device that runs late. With **Send Start and Stop messages** on, sequencers and drum machines start and stop along with it.

[Windows MIDI Patchbay]({{ site.baseurl }}/tools/midipatchbay/#generators) can do it too: connect a **MIDI clock** step to every device, with a **Clock divider** in front of any device that should run at half speed. From a command prompt, `midi endpoint send-beat-clock` in the [MIDI Console]({{ site.baseurl }}/tools/console/#endpoint-send-beat-clock) sends clock until you press Esc.

</details>

<details id="send-midi-time-code" markdown="1">
<summary>Send MIDI Time Code</summary>

In [Windows MIDI Clock]({{ site.baseurl }}/tools/midiclock/#midi-time-code), set **What this clock sends** to MIDI Time Code, then pick a frame rate and where it starts. A **MIDI Time Code** step in [Windows MIDI Patchbay]({{ site.baseurl }}/tools/midipatchbay/#generators), and `midi endpoint send-time-code` in the [MIDI Console]({{ site.baseurl }}/tools/console/#endpoint-send-time-code), send it too. [Windows MIDI Sequencer]({{ site.baseurl }}/tools/midisequencer/#clock-sync-and-remote-control) can send time code that follows the song as it plays.

</details>

<details id="follow-another-clock" markdown="1">
<summary>Follow the tempo of my DAW or another device</summary>

Have the DAW send MIDI clock to a loopback, or connect the device that sends it. Then:

- [Windows MIDI Sequencer]({{ site.baseurl }}/tools/midisequencer/#clock-sync-and-remote-control) follows it, from **Clock and sync**.
- In [Windows MIDI Glass]({{ site.baseurl }}/tools/midiglass/#tempo), set **Tempo source** to **Incoming MIDI clock**, and LFO and Steps controls follow its speed.
- In [Windows MIDI Patchbay]({{ site.baseurl }}/tools/midipatchbay/#generators), connect the clock to an **LFO** step's **In**, or pass it through a **Clock divider** to send a slower clock on to another device.

Windows MIDI Clock is a clock source. It sends clock, but it doesn't follow one.

</details>

## SysEx, patches, and firmware

System exclusive messages, usually called SysEx, are how most devices load sounds, back up their settings, and take firmware updates. What's inside them is different for every device, so these tools move the bytes accurately and leave the meaning to the device.

<details id="update-firmware-with-sysex" markdown="1">
<summary>Update the firmware on a device using SysEx</summary>

Read the maker's instructions first. Many devices have to be put in an update mode before they accept firmware, and some makers have an updater app of their own that you should use instead.

To send a `.syx` firmware file yourself, use [Windows MIDI SysEx Utility]({{ site.baseurl }}/tools/midisysextool/#sending-a-sysex-file-to-a-device). Pick the device, select **Send a file**, choose the file, and select **Send**. Close other apps that might send to the device while it updates. If the update fails, lower **Messages per transfer**, raise **Delay between transfers**, and try again.

From a command prompt, `midi sysex send-file` in the [MIDI Console]({{ site.baseurl }}/tools/console/#sysex-send-file) does the same job.

</details>

<details id="save-a-patch-dump" markdown="1">
<summary>Save the incoming patch dump data from a synth</summary>

In [Windows MIDI SysEx Utility]({{ site.baseurl }}/tools/midisysextool/#receiving-a-dump-from-a-device), pick the synth, select **Receive**, then **Start receiving**. Start the dump from the synth's own panel. When it's finished, select **Stop receiving**, then **Save**. The `.syx` file holds exactly what arrived, so you can send it back to the synth later.

From a command prompt, `midi sysex receive-file dump.syx` in the [MIDI Console]({{ site.baseurl }}/tools/console/#sysex-receive-file) waits for the dump and saves it. In PowerShell, use `Receive-MidiSystemExclusive`.

</details>

<details id="send-a-patch-bank" markdown="1">
<summary>Send a patch bank or a backup to a synth</summary>

In [Windows MIDI SysEx Utility]({{ site.baseurl }}/tools/midisysextool/), pick the synth, select **Send a file**, choose the `.syx` file, and select **Send**. Many synths ignore SysEx until you put them in a receive mode, so do that first. To copy a bank from one synth to another, run [two copies]({{ site.baseurl }}/tools/midisysextool/#two-copies-at-once) of the utility, one receiving and one sending.

To send a whole folder of `.syx` files from a script, pipe them to `Send-MidiSystemExclusive` from the [MIDI PowerShell cmdlets]({{ site.baseurl }}/tools/powershell/#system-exclusive-cmdlets).

</details>

## Watching, testing, and recording

<details id="view-incoming-messages" markdown="1">
<summary>View all the messages coming from my MIDI keyboard</summary>

Open [Windows MIDI Monitor]({{ site.baseurl }}/tools/midi2monitor/), pick the keyboard, and select **Start listening**. Each message shows up as it arrives, with what it means in plain words, such as the note and how hard it was played. Clock and active sensing are hidden at first, because many devices send them all the time. **Monitor**, next to a device in [Windows MIDI Settings]({{ site.baseurl }}/tools/settings/), opens it already watching that device.

Your DAW can keep using the keyboard while you watch. From a command prompt, use `midi endpoint monitor` in the [MIDI Console]({{ site.baseurl }}/tools/console/#endpoint-monitor).

</details>

<details id="send-a-message-by-hand" markdown="1">
<summary>Send a message from a device's manual</summary>

Type it into [Windows MIDI Scratch Pad]({{ site.baseurl }}/tools/midiscratchpad/) as hexadecimal bytes, the way the manual prints it, such as `B0 07 64`, and select **Send all**. It checks what you type as you go, and **Insert** builds common messages for you.

From a command prompt, `midi endpoint send-message` in the [MIDI Console]({{ site.baseurl }}/tools/console/#endpoint-send-message) sends a message written as UMP words. [A primer for UMP data format]({{ site.baseurl }}/kb/ump-data-format-primer/) explains how to write one.

</details>

<details id="record-to-a-midi-file" markdown="1">
<summary>Record what I play to a MIDI file</summary>

[Windows MIDI Sequencer]({{ site.baseurl }}/tools/midisequencer/#recording) records into the tracks you arm with **R**, and exports Standard MIDI Files. For a quick capture, [Windows MIDI Monitor]({{ site.baseurl }}/tools/midi2monitor/#saving-a-capture) can save what it caught as a `.mid` file.

From a command prompt, `midi endpoint monitor --capture-to-file take1.mid --capture-format smf` does the same. In PowerShell, use `Receive-MidiMessage -Path take1.mid`.

</details>

## Automating and scripting

<details id="send-settings-before-a-gig" markdown="1">
<summary>Automatically send initialization settings to my synths before a gig</summary>

Pick whichever fits the way you work:

- **One button, in [Windows MIDI Glass]({{ site.baseurl }}/tools/midiglass/#what-a-control-sends).** On a button's **Sends** tab, set **Type** to **A sequence of steps**, select **New**, then **Edit steps…**. Add program changes, controller values, system exclusive, and waits in between, for as many of the layout's devices as you like. A control can also send its starting value when the layout opens.
- **A batch file, with the [MIDI Console]({{ site.baseurl }}/tools/console/).** Send `.syx` files with `midi sysex send-file`, and text files of messages with `midi endpoint send-message-file`. Run the batch file before the show.
- **A script, with the [MIDI PowerShell cmdlets]({{ site.baseurl }}/tools/powershell/).** `Send-MidiMessage` and `Send-MidiSystemExclusive` can set up a whole room of gear in one go.
- **For one synth, [Windows MIDI Keyboard]({{ site.baseurl }}/tools/midikeyboard/#bank-and-program)** can send your bank and program again each time the synth reconnects.

</details>

<details id="automate-midi-setup" markdown="1">
<summary>Automate MIDI setup through scripting</summary>

The [MIDI PowerShell cmdlets]({{ site.baseurl }}/tools/powershell/) and the [MIDI Console]({{ site.baseurl }}/tools/console/) cover most setup jobs:

- **Loopbacks:** `New-MidiLoopback -SaveToConfiguration`, or `midi loopback create --save-to-config`.
- **Network devices:** `Connect-MidiNetworkHost` and `Connect-MidiRtpHost`, with `-SaveToConfiguration` so the connection comes back after a restart.
- **Bluetooth devices:** `midi bluetooth connect`.
- **Device names and pictures:** `midi endpoint customize`.
- **The General MIDI synth:** `Set-MidiSynth`, or `midi synth configure`.

Use these, or the setup apps, rather than editing the Windows MIDI Services configuration file yourself. The file's format can change in any update, and the tools keep it correct.

</details>

<details id="start-everything-automatically" markdown="1">
<summary>Start my MIDI setup automatically</summary>

Much of it already does. Loopbacks you saved, and the Bluetooth and network devices you connected, come back on their own after a restart, with no app running. For the rest:

- **[Windows MIDI Patchbay]({{ site.baseurl }}/tools/midipatchbay/#routing-only-runs-while-patchbay-is-running):** turn on **Start with Windows** and **Run in notification area**, and **Start automatically** on each patch that should route.
- **[Windows MIDI Clock]({{ site.baseurl }}/tools/midiclock/#starting-from-the-command-line):** `midiclock.exe --start` starts every saved clock, from a shortcut or a script.
- **[Windows MIDI Glass]({{ site.baseurl }}/tools/midiglass/#running-a-layout):** `midiglass.exe --run "<layout file>"` opens a layout, ready to play. So does double-clicking the layout file.
- **Other tools**, such as Windows MIDI Monitor and Windows MIDI SysEx Utility, open on the device you choose when you give them its endpoint device ID. Copy the ID from the device's details in [Windows MIDI Settings]({{ site.baseurl }}/tools/settings/#endpoint-details).

</details>

## Backing up and moving your setup

Your device names, port names, loopbacks, network and Bluetooth connections, and transport settings are kept in your Windows MIDI Services configuration. Windows MIDI Settings looks after it, under **Global MIDI Settings**. Those settings are shared by everyone who uses the PC, so they need administrator rights, and the app offers to restart itself with them.

<details id="back-up-my-configuration" markdown="1">
<summary>Back up my configuration</summary>

In [Windows MIDI Settings]({{ site.baseurl }}/tools/settings/#global-midi-settings), select **Global MIDI Settings**. Under **Back up the active configuration**, select **Back up now** to keep a copy on this PC. Select **Save a copy of the active file...** to keep one somewhere else too, such as a USB drive.

To go back to a backup, pick it under **Restore from a backup**, select **Restore**, and then restart the MIDI service. For a copy you saved somewhere else, select **Add a configuration file...** instead. Either way, the configuration it replaces is backed up first, so you can change your mind. Windows MIDI Services also backs up a configuration before the first change to it each day, and those backups are in the same list.

Your patches, layouts, and sequences are ordinary files in your Documents folder, in **MIDI Patches**, **MIDI Layouts**, and **MIDI Sequences**, and your SysEx files are in **SysEx Library**. Back them up along with your other files. Windows MIDI Glass can also back up a single layout with **Back up now**, on the layout's **…** menu.

</details>

<details id="studio-and-stage-configurations" markdown="1">
<summary>Set up separate Studio and Stage configurations</summary>

[Windows MIDI Settings]({{ site.baseurl }}/tools/settings/#global-midi-settings) can keep more than one configuration, and uses one at a time. In **Global MIDI Settings**, type a name such as Stage under **Create a new configuration**, and select **Create**. The new configuration starts empty, and it's the one in use after you select **Restart the MIDI service**. Then set up the loopbacks, connections, and device names the stage needs. It doesn't have the default loopbacks, so make those again with the **Create default loopback** buttons in [Windows MIDI Loopback Setup]({{ site.baseurl }}/tools/midiloopbacksetup/#the-default-loopbacks).

To switch, pick a configuration under **Configuration file**, select **Use this file**, and restart the MIDI service. Restarting disconnects every app using MIDI, so close them first.

Windows MIDI Patchbay patches and Windows MIDI Glass layouts aren't part of a configuration, so make a patch or a layout for each setup, and open the one you need.

</details>

<details id="copy-my-setup-to-another-pc" markdown="1">
<summary>Copy all my endpoint and port customizations, network connections, and more to another PC</summary>

Patches, layouts, and sequences move easily, because they're files:

- **Windows MIDI Patchbay:** copy the `.midipatch` files from **Documents › MIDI Patches**, and use **Import patch…** on the other PC.
- **Windows MIDI Glass:** select **Pack for sharing…** on a layout's **…** menu, and use **Import a pack…** on the other PC.
- **Windows MIDI Sequencer:** copy the `.midisequence` files from **Documents › MIDI Sequences**.

To move the configuration, with your device and port names, loopbacks, and network connections, use [Windows MIDI Settings]({{ site.baseurl }}/tools/settings/#global-midi-settings). On this PC, select **Save a copy of the active file...** in **Global MIDI Settings**, and save it where the other PC can reach it, such as a USB drive. On the other PC, select **Add a configuration file...**, pick the copy, and restart the MIDI service. If that PC already has a configuration with the same file name, it's backed up first, so you can get it back with **Restore from a backup**. Use these buttons rather than copying the file yourself, because where it's kept and what's in it can change in any update.

A device that reports a serial number gets its name and picture back as soon as it's plugged in. Other devices are matched by the port they're plugged into, so on a new PC their settings may not find them. Windows MIDI Settings then shows **Some saved settings have no matching device**. Select **Review them** to move the settings onto the right device.

</details>

## Fixing problems

<details id="device-does-not-show-up" markdown="1">
<summary>Find out why my device doesn't show up</summary>

Start with [Windows MIDI Troubleshooting and Repair]({{ site.baseurl }}/tools/miditroubleshooter/). Its **Transports** page shows whether each kind of connection, such as USB, Bluetooth, or the network, is running. On its **Diagnostics** page, the **midiksinfo** report says why a USB device is missing, or has the wrong number of ports or the wrong names.

For a Bluetooth device, see [Common reasons a device may not connect]({{ site.baseurl }}/tools/midibluetoothsetup/#common-reasons-a-device-may-not-connect). For a network device, see [When something doesn't connect]({{ site.baseurl }}/tools/midinetworksetup/#when-something-doesnt-connect).

</details>

<details id="which-app-is-using-a-device" markdown="1">
<summary>Find out which app is using a device</summary>

The **Active sessions** page of [Windows MIDI Troubleshooting and Repair]({{ site.baseurl }}/tools/miditroubleshooter/#active-sessions) lists every app connected to the MIDI service, and what each one has open. `midi enumerate active-sessions` in the [MIDI Console]({{ site.baseurl }}/tools/console/#enumerate-active-sessions), and `Get-MidiSession` in PowerShell, list the same thing.

Apps can share a device, so one app using it doesn't stop another from using it too.

</details>

<details id="collect-diagnostics" markdown="1">
<summary>Collect what support needs for a bug report</summary>

In [Windows MIDI Troubleshooting and Repair]({{ site.baseurl }}/tools/miditroubleshooter/#diagnostics), run **mididiag** on the **Diagnostics** page, and select **Save as zip**. Attach the zip file to your GitHub issue or email. If you're asked for a trace, use **Capture repro log**: start the capture, make the problem happen, then stop and save.

From a command prompt, `mididiag > mididiag.txt` saves the same report. See [MIDI Diagnostics Report]({{ site.baseurl }}/tools/mididiag/).

</details>

<details id="make-midi-apps-start-faster" markdown="1">
<summary>Make the first MIDI app I open start faster</summary>

The MIDI service starts when the first app asks for it, which adds a few seconds to that app's startup. If you use MIDI often, have the service start with Windows instead. Select **Start with Windows** on the **MIDI service** page of [Windows MIDI Troubleshooting and Repair]({{ site.baseurl }}/tools/miditroubleshooter/#midi-service).

</details>

<details id="go-back-to-the-old-midi-stack" markdown="1">
<summary>Go back to the MIDI that Windows had before</summary>

Change the API mode to **Legacy MIDI** on the **API mode** page of [Windows MIDI Troubleshooting and Repair]({{ site.baseurl }}/tools/miditroubleshooter/#api-mode), then restart the PC. In that mode, you give up sharing a device between apps, the built-in loopbacks, Network MIDI 2.0, Bluetooth LE MIDI, and USB MIDI 2.0 devices. Read [How to change the API mode]({{ site.baseurl }}/kb/how-to-change-api-mode/) before you switch.

</details>

<details id="loopback-stopped-passing-messages" markdown="1">
<summary>Fix a loopback that stopped passing messages</summary>

It may have muted itself. Windows MIDI Services mutes a loopback when MIDI feeds back into it, such as when an app sends what it receives straight back to the same loopback. Left alone, that would go around forever. [Windows MIDI Loopback Setup]({{ site.baseurl }}/tools/midiloopbacksetup/#when-midi-feeds-back-into-a-loopback) shows which loopback was muted and when. Turn off the MIDI thru or echo setting that caused it, then select **Unmute**. See [Why a loopback mutes itself]({{ site.baseurl }}/kb/loopback-feedback-protection/).

</details>

<details id="network-connections-blocked" markdown="1">
<summary>Let other computers connect to my PC over the network</summary>

Windows Firewall is the usual reason they can't. The **Firewall** page of [Windows MIDI Network Setup]({{ site.baseurl }}/tools/midinetworksetup/#windows-firewall) lets the MIDI service through. Devices find each other only on the same network, so check that too. If they still can't see each other, the [MIDI Diagnostics Report]({{ site.baseurl }}/tools/mididiag/#when-other-computers-cant-see-your-pc) checks the rest. See also [Adding Network MIDI 2 to your firewall]({{ site.baseurl }}/kb/network-midi-firewall/).

</details>

<details id="port-names-changed" markdown="1">
<summary>Fix port names in older apps</summary>

Apps that use the older Windows MIDI APIs see a device as MIDI 1.0 ports, and many remember a port by its name. To choose how those ports are named, use **Edit port names** in the **Customize** dialog of [Windows MIDI Settings]({{ site.baseurl }}/tools/settings/#naming-the-midi-10-ports-of-a-device), or set the default for every device in **Global MIDI Settings**. **Compatible with older Windows** keeps the names those apps have always seen.

New names take effect when the MIDI service restarts. Apps that remember ports by name may then need to be pointed at them again.

</details>

<details id="device-lost-its-name" markdown="1">
<summary>Keep a device's name and picture when I plug it into another USB port</summary>

A device that reports a serial number, or another identifier of its own, is recognized in any USB port. One that doesn't is recognized by the port it's plugged into, so moving it can lose the name and picture you gave it. Plug it back into the same port and they come back. The device's details in [Windows MIDI Settings]({{ site.baseurl }}/tools/settings/#endpoint-details) say which kind yours is. See [When saved endpoint settings do not match a device]({{ site.baseurl }}/kb/restoring-endpoint-settings/).

</details>

<details id="switch-class-drivers" markdown="1">
<summary>Switch a USB device to the other MIDI class driver</summary>

Windows has two drivers for class-compliant USB MIDI devices: the new one that handles both MIDI 1.0 and MIDI 2.0, and the older USB Audio driver. If a device doesn't work well on one, try the other. The **Drivers** page of [Windows MIDI Troubleshooting and Repair]({{ site.baseurl }}/tools/miditroubleshooter/#drivers) shows which one each device uses, and switches it for you. Only switch a device you know is a MIDI device. See [How to switch between MIDI class drivers]({{ site.baseurl }}/kb/how-to-switch-between-class-drivers/).

</details>
