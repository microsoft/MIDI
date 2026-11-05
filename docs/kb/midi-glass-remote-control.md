---
layout: kb
title: Run Windows MIDI Glass from a foot controller
audience: everyone
description: Use a foot controller, such as a Morningstar MC6 Pro, or any MIDI device to turn pages, press controls, swap pad grids, and change the key in Windows MIDI Glass while you play.
categories:
  - Getting Started
---

Windows MIDI Glass is the Windows MIDI Services app for building your own touch control surface. When your hands are busy, a foot controller can run the layout for you. Step on a switch and the page changes, a set of pads swaps for another, or the pads move to the next scale.

This works with any MIDI device that sends a program change, a control change, or a note: a foot controller like the Morningstar MC6 Pro, a pad controller, or your keyboard.

> **Windows MIDI Glass is a preview app.** Names and settings in this article can change before it ships.

On this page:

- [How it works](#how-it-works)
- [Set it up](#set-it-up)
- [What a rule can do](#what-a-rule-can-do)
- [Swap pad grids in one spot](#swap-pad-grids-in-one-spot)
- [Keep the controller's lights in step](#keep-the-controllers-lights-in-step)
- [Run a sequence when a page opens](#run-a-sequence-when-a-page-opens)
- [An example layout](#an-example-layout)
- [Tips and limits](#tips-and-limits)

## How it works

A layout has a list of **rules**. Each rule says: when this message arrives from this device, do this. Rules only work while the layout runs, or in the editor's **Try** mode. In **Edit** mode, the rule's card lights up when its message arrives, so you can check which switch is which, but nothing else happens.

A rule can only do what the layout already holds. It can press a button you built, but it can't send something the layout doesn't.

## Set it up

1. **Add the controller to the layout.** Open the layout in the editor, open **Layout…**, then **Pages and devices…**, and select **Outputs**. Add your foot controller, even though the layout won't send it anything. Rules only hear devices in this list.
2. **Set up the controller.** Program each switch to send a program change, a control change, or a note. Most foot controllers can. A control change that sends 127 when you step on the switch and 0 when you let go works well.
3. **Add a rule.** Select **MIDI remote control** in the list on the left, then **Add a rule**.
4. **Learn the message.** Select **Learn** on the rule, then step on the switch. The rule fills in the device, the kind of message, the channel, and the number.
5. **Pick what it does.** Under **Do this**, pick an action, and the page, control, or sequence it works on.
6. **Try it.** Go back to the editor and switch to **Try**, or run the layout, and step on the switch.

A control change counts only when its value is above 0, so letting go of a switch doesn't run the rule a second time. A note counts only when it starts.

## What a rule can do

| Action | What happens |
| --- | --- |
| **Go to a page** | Shows the page you pick. |
| **Next page**, **Previous page** | Moves one page along. It stops at the last and the first page. |
| **Go to the page the message picks** | Program 0 opens the first page, 1 the second, and so on. A control change uses its value and a note uses its number. Use **Any** for the number, so every program change works. |
| **Press a control** | A button presses and lets go, a toggle flips, and a switch moves to its next position. |
| **Set a control's value** | Moves the control and sends what it sends, as if your finger moved it. Pick **Take it from the message** to follow an expression pedal. |
| **Turn a control on**, **off**, or **on or off** | A control that's off is drawn faint and ignores touch, but it still follows what a device sends, so it's right the moment it comes back on. |
| **Show**, **Hide**, or **Show or hide a control** | A hidden control is gone from the page until a rule shows it. |
| **Show only this one of its group** | Shows the control and hides the other controls in its group. See [Swap pad grids in one spot](#swap-pad-grids-in-one-spot). |
| **Change pad grids** | Does what a grid button does, such as octave up, next scale, or next root. |
| **Set the key's root note**, **Set the scale** | Sets the key of the pad grids, the way a root or scale picker does. With **Take it from the message**, a note picks the root: any C picks C, any D picks D. |
| **Run a sequence** | Runs one of the layout's sequences. |
| **Panic** | Stops every note on every device Windows MIDI Glass is playing. |

A rule can work on one control, or on a whole group of controls.

## Swap pad grids in one spot

Maybe you want square pads for one song and hex pads for the next, in the same place on the screen. You don't need two pages:

1. Put both pad grids in the same spot, the same size.
2. Select both and select **Group** on the toolbar.
3. Select the hex pads, and on the **Behavior** tab turn on **Start hidden**.
4. Add two rules: one that does **Show only this one of its group** to the square pads, and one that does the same to the hex pads.

Now one switch brings in the square pads and the other brings in the hex pads. This works for any stack of controls, not only pad grids.

The editor always shows every control, so you can still edit the one underneath. In **Try** mode, you see what the layout does.

## Keep the controller's lights in step

Under **Tell the controller which page is showing**, turn on **Send the page**. Each time the page changes, the layout sends a program change or a control change to the device you pick, so the bank or the lights on your controller follow the screen. **First page sends** is the number the first page sends. Each page after it sends one more.

It's off unless you turn it on. If the controller just sent the same message to change the page, the layout doesn't send it straight back.

## Run a sequence when a page opens

In **Pages and devices…**, select **Pages**, open a page's **…** menu, and pick a sequence under **When this page opens**. It runs each time you go to that page, the first page included, such as a program change that loads the right sound for the song.

## An example layout

[Download the foot controller example]({{ site.baseurl }}/assets/layouts/midi-glass-foot-controller.midilayout) and copy it into **Documents › MIDI Layouts**. It has three pages and expects six switches and an expression pedal sending control changes on channel 1:

| Switch | Sends | Does |
| --- | --- | --- |
| A | CC 20 | Previous page |
| B | CC 21 | Next page |
| C | CC 22 | Shows the square pads |
| D | CC 23 | Shows the hex pads |
| E | CC 24 | Moves the pads to the next scale |
| F | CC 25 | Turns the **Hold** switch on or off. It starts turned off. |
| Expression pedal | CC 11 | Moves the **Volume** fader on the **Mix** page |

A program change from the controller also picks a page, and the layout sends a program change back each time the page changes. The **Play** page loads a piano sound each time it opens.

The example looks for a device called **MC6 Pro**, and plays **Default App Loopback (A)**, so you can watch what it sends in MIDI Monitor on **Default App Loopback (B)**. If your controller has another name, open the layout in the editor, go to **Outputs**, and point **Foot** at your controller. Every rule follows.

## Tips and limits

- **One message, one thing.** A rule listens for a program change, a control change, or a note, and nothing else. To do two things with one switch, add two rules for the same message, or have the rule run a sequence.
- **Loops are stopped for you.** For a moment after the layout sends a message to a device, the same message coming back from that device is ignored. A controller that sends back everything it hears can't start a loop.
- **Nothing a rule does is saved.** The layout opens the way you built it each time, with every control as its **Start turned off** and **Start hidden** settings say.
- **The layout isn't a MIDI device other apps can see.** Your controller has to be in the layout's device list. A rule can't open another layout or a set list, and can't tap a tempo.
