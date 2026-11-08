---
layout: tools_page
title: Windows MIDI Monitor
tool: midi2monitor
description: Watch the MIDI messages coming from your instruments and controllers
icon: /assets/images/midi2monitor.png
categories:
  - General Purpose MIDI Tools
---

> This page covers information about a Windows MIDI Services feature and application that will be released to consumers in 2027. It's currently available for developers.

Windows MIDI Monitor shows you the MIDI messages arriving from a keyboard, controller, synth, or any other MIDI device on your PC. It's the tool to reach for when you want to answer questions like "is this knob actually sending anything?", "which channel is my keyboard on?", or "what exactly does my synth send when I press that button?".

Everything appears as it arrives, so you can play a note or move a control and watch it show up straight away.

## Quick start

![The Windows MIDI Monitor window, with numbered callouts on the MIDI Device list, the Group and Channel lists, the Start listening button, the Details column, the clock and active sense buttons, and Save to File]({{ site.baseurl }}/assets/images/midi2monitor-quick-start.png)

1. **MIDI Device** picks the instrument to watch.
2. **Group** and **Channel** narrow the list to one part of the instrument. Leave them on all groups and all channels to see everything.
3. **Start listening** begins the capture. Play a note, move a wheel, or turn a knob. Select **Stop listening** when you're done.
4. **Messages** appear as they arrive, newest at the bottom. The **Details** column says what each one means in plain words.
5. **Clock** and **active sense** messages are hidden to start with, because many instruments send them all the time. These two buttons show them.
6. **Save to File** keeps a copy of everything you captured.

You can open more than one copy of Windows MIDI Monitor at a time if you want to watch two instruments side by side. To watch several instruments, or several groups, in one list, use [advanced mode](#advanced-mode-watching-several-devices-at-once).

MIDI Monitor remembers the device, group and channel you watched last, and picks them again the next time you open it.

![Choosing a MIDI device]({{ site.baseurl }}/assets/images/midi2monitor-devices.png)

The device list shows everything currently connected, along with a picture of the instrument where one is available.

## Reading the list

Each row is one MIDI message.

| Column | What it tells you |
|---|---|
| **#** | A running count of messages received |
| **Message Time** | When the message arrived |
| **Message** | The raw message data, with a colored label naming the message underneath |
| **Group** | Which group the message came in on |
| **Channel** | Which of the 16 MIDI channels the message is for |
| **Details** | The message translated into plain terms, such as the note, velocity, or controller |
| **Since last** | How long it had been since the previous message |

The **Details** column is usually the one you want. Rather than working out that `20904050` means a note, it simply tells you `Note 64 (E3)  Velocity 80`. Note numbers are shown with their musical names, and well-known controllers are named too, so controller 10 appears as `Pan MSB`.

The colored label under each message is a quick visual cue. Notes, controllers, SysEx, and MIDI 2.0 messages each get their own color, so a glance is often enough to see what kind of traffic an instrument is sending.

## Narrowing down what you see

Busy instruments can send a lot. There are a few ways to cut the list down to what matters.

**Group and Channel.** Use the **Group** and **Channel** lists at the top to watch just one part of a device. Choose a group first, and the channel list becomes available. Most MIDI 1.0 gear uses a single group, so you'll usually be choosing a channel.

**Clock and Active Sense.** Many instruments send timing clock and "I'm still here" messages constantly, which drowns out everything else. These are hidden by default. The two small buttons next to the message counts at the bottom left switch them back on if you need to see them.

## Working with messages

Right-click anywhere in the list for the message commands.

![The message right-click menu]({{ site.baseurl }}/assets/images/midi2monitor-message-menu.png)

You can select messages with the mouse in the usual way, then copy them to the clipboard in whichever form is most useful:

- **Copy as UMP words** gives you the raw message data.
- **Copy as binary data** gives you the same bytes that a binary file from **Save to File** holds, written out as hex. Paste them into the hex side of a hex editor.
- **Copy as MIDI 1.0 bytes** gives you the familiar MIDI 1.0 byte form, for messages that have one.
- **Copy as SysEx bytes** gives you just the SysEx contents, handy for pasting into a patch librarian, a forum post, or an email to a manufacturer's support desk.

Commands that don't apply to what you've selected are grayed out, so if **Copy as SysEx bytes** is unavailable, there's no SysEx in your selection.

**Select All SysEx 7** picks out every SysEx message in the capture in one go. If you've just asked a synth to dump a patch and want only that, this saves a lot of scrolling.

**Clear All Entries** empties the list so you can start a fresh observation. You can also press **Ctrl+L**, or select **Clear** next to **Save to File** at the bottom of the window. MIDI Monitor asks before it clears, because clearing can't be undone. If you'd rather it didn't ask, check **Don't ask me again** in that question. To have it ask again, turn on **Ask before clearing all entries** in Settings.

**Ctrl+C** copies the selected messages as UMP words, the same as **Copy as UMP words**.

### Adding comments to a message

Every row has a small speech-bubble button on the left. Select it to attach a comment to that message, such as "this is the one the pedal sends". Rows with a comment show a marker on the button, and hovering over it shows what you wrote. Comments are included when you save the capture to a file, which makes them useful for handing findings to someone else.

## Setting up the view

Right-click any column heading to reach the display options.

![The column heading menu]({{ site.baseurl }}/assets/images/midi2monitor-header-menu.png)

**Edit columns** lets you turn columns on or off and change their order. If you never look at the group number, switch it off and give the space to something you do use.

![Choosing columns]({{ site.baseurl }}/assets/images/midi2monitor-columns.png)

**Time and message display** controls how the message time is shown, and whether the message name appears under the raw data.

![Time and message display options]({{ site.baseurl }}/assets/images/midi2monitor-display-options.png)

Message time can be shown as absolute clock ticks from the time the PC booted up, or as microseconds, milliseconds, or seconds counted from the moment you started listening. Seconds or milliseconds are usually the friendliest choice.

Along the bottom right there's a zoom control for making the table text larger or smaller, which is handy if the monitor is sitting on a second screen across the room.

The pin button next to the window's minimize button keeps MIDI Monitor above your other windows, so it stays visible while you work in your DAW.

## Saving a capture

**Save to File** writes everything currently in the list, including any comments you added and the messages the display filters hid. Pick one of four kinds of file in the save box:

**Text file (.txt)** is for reading, and for sending to somebody who is helping you troubleshoot. It has one line per message with the same columns the window shows, separated by tabs. A capture from advanced mode has a **From** column, with the device and group, in place of **Group**.

**Comma separated values (.csv)** is for a script or a spreadsheet. It has a heading row, the raw Universal MIDI Packet words in hex, and the times as plain numbers of microseconds, so you can chart the gap between messages or search for a particular word. The column names are always in English, whatever language Windows is set to, so a script that reads the file keeps working on any machine. The last column, **Source**, names the device and group a message came from in advanced mode, and is empty in simple mode.

**Standard MIDI File (.mid)** is for a sequencer. The capture becomes a playable file at 120 beats per minute, with one track per group, so you can drop it into a DAW and hear it back. Standard MIDI Files are a MIDI 1.0 format, so MIDI 2.0 messages are converted where MIDI 1.0 has an equivalent and left out where it does not. If you want to study the exact timing, save the comma separated file instead: it carries the real microsecond figures, and a MIDI file rounds them to its own musical grid.

**Binary UMP words (.bin)** is for a hex editor. It holds nothing but the Universal MIDI Packet words, one after another, with no times, comments or headings. Each 32-bit word is stored the way Windows PCs store numbers, lowest byte first (this is called little-endian), so the word `20903C64` shows up in a hex editor as `64 3C 90 20`.

## Advanced mode: watching several devices at once

Simple mode watches one device. Advanced mode watches as many groups, on as many devices, as you like, all in one list. Select the **Advanced mode** button in the title bar, or press **Ctrl+Shift+A**. Select it again to go back to simple mode. MIDI Monitor opens in whichever mode you used last.

### Choosing what to watch

In advanced mode the device, group and channel lists are replaced by a **Sources** list on the left. It shows every device, with its groups under it. Check a group to watch it, or check a device to watch all of its groups. You can change what's checked while MIDI Monitor is listening.

Each checked group gets its own color. The color shows next to the group in **Sources**, and as a bar beside every message from that group.

While MIDI Monitor is listening, a green light shows next to every device and group that has sent something in the last second, whether it's checked or not. That's the quickest way to find which device and group your keyboard is really using. Turn on **Only show endpoints that are sending** at the bottom of the list to hide everything that's quiet. The box at the top of the list finds a device or group by name.

The lights work by listening to every device, not only the ones you checked. On a PC with a lot of MIDI devices that can make listening slow to start. If it does, turn off **Show which endpoints are sending** in Settings, and MIDI Monitor opens only the devices you checked.

The button at the left of the toolbar hides and shows **Sources**. In a narrow window, **Sources** opens over the messages instead of beside them, and the groups you're watching show as small tags in the toolbar. Select the **x** on a tag to stop watching that group, or **+ Add** to open **Sources**.

### Where each message came from

Advanced mode adds a **From** column, with the device name and the group. It takes the place of the **Group** column, since **From** already says the group. You can move or hide **From** with **Edit columns**, like any other column.

### Filtering

**Filter** in the toolbar has the same choices as a filter step in [Windows MIDI Patchbay]({{ site.baseurl }}/tools/midipatchbay/): message types, channels, kinds of channel message, system messages, and a range of notes. Everything starts turned on, which means "show it". Changes take effect as you click.

Each part of the filter that's hiding something shows as a tag next to **Filter**, such as **Channels 1 to 4**. Select the **x** on a tag to undo that part. **Show everything** at the top of the filter undoes all of it.

A filter only hides messages. They're still captured, and they come back when you change the filter. The bottom of the window says how many are hidden. **Save to File** still saves all of them.

The **Clock** and **Active Sensing** choices in the filter are the same as the two buttons at the bottom of the window. Changing one changes the other.

**Fold repeated messages into one row, with a count** is for controllers that send the same message over and over. When a message is exactly the same as the last one shown from the same group, it doesn't get a row of its own. The row it repeats shows a count instead, such as **×234 repeats**, and the count goes up as more arrive. Each one still keeps its own time, and **Save to File** writes every one of them out.

### When another app opens MIDI Monitor

MIDI Settings, Network MIDI Setup, Bluetooth MIDI Setup and MIDI Sequencer can open MIDI Monitor to watch one device. If that happens in advanced mode, MIDI Monitor watches that device and sets your own list of sources aside without changing it. A message at the top says so. Select **Add to my sources** to add that device to your list, or **Switch to my sources** to go back to your list. If you do neither, your list is still there the next time you open MIDI Monitor.

Your filter, columns, zoom and colors stay the same however MIDI Monitor was opened.

## Keyboard shortcuts

| Keys | What they do |
|---|---|
| **Ctrl+L** | Clear the list |
| **Ctrl+C** | Copy the selected messages as UMP words |
| **Ctrl+A** | Select all messages |
| **Ctrl+Shift+A** | Switch between simple and advanced mode |

## Settings

The gear button in the title bar opens the settings.

![The settings panel]({{ site.baseurl }}/assets/images/midi2monitor-settings.png)

**Appearance** sets whether the app follows your Windows light or dark mode, and which window background to use. Mica and Acrylic pick up colors from your desktop; Acrylic is the one that lets what's behind the window show through, which some people like when the monitor sits on top of a DAW.

Tick **Use a custom background color** to pick your own color for the window. It works with all three backgrounds, though each treats it a little differently: Solid uses exactly the color you choose, Mica blends it with your desktop wallpaper, and Acrylic gives the gentlest tint of the three because it is still letting what's behind the window through. If you keep more than one monitor window open at a time, giving each one its own color makes it easy to tell at a glance which instrument you're looking at.

**Messages to keep** sets how many messages are held before the oldest start being discarded. The default of 10,000 is plenty for normal use. Raise it if you're capturing a long session, and bear in mind that a larger number uses more memory.

**Ask before clearing all entries** turns the clear question back on after you've checked **Don't ask me again**.

**Show which endpoints are sending** turns the activity lights in advanced mode on or off. See [Choosing what to watch](#choosing-what-to-watch).

**Reset the columns to their defaults** puts the message list back the way it started.

## Starting from the command line

If you always monitor the same instrument, MIDI Monitor can open and start listening to it straight away. Pass the endpoint device id, and optionally a group and channel:

```
midi2monitor.exe [endpoint device id] [options]

  [endpoint device id]   Optional. The full endpoint device id of the device to
                         monitor.

  --group, -g  number    Optional. The group number, 1 through 16, to monitor.
                         Only valid when an endpoint device id is supplied.

  --channel, -c number   Optional. The channel number, 1 through 16, to monitor.
                         Only valid when a group is also supplied.

  --simple, --advanced   Optional. Open in simple or advanced mode, for this window
                         only. Without either, the mode you used last is used.

  --help, -h, -?         Show this help.
```

For example:

```
midi2monitor.exe "\\?\swd#midisrv#midiu_bloop_basic_def#{e7cce071-3c03-423f-88d3-f1045d02552b}" --group 1 --channel 10
```

You can find the endpoint device id for a device using the [MIDI Console]({{ site.baseurl }}/tools/console/) or the [MIDI Settings app]({{ site.baseurl }}/tools/settings/).

When you supply a device id this way, MIDI Monitor selects that device, applies the group and channel you asked for, and begins listening on its own. That makes it a convenient thing to launch straight from another app when you want to watch a particular instrument.

A window opened this way never changes what you saved: the device MIDI Monitor remembers for simple mode, and your sources in advanced mode, stay as you left them. In advanced mode, the channel is left to the filter, so only the device and group are used.

## App provenance

This app was coded by AI using GitHub Copilot, guided by Pete Brown.
