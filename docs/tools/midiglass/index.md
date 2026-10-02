---
layout: tools_page
title: Windows MIDI Glass
tool: midiglass
description: Build your own on-screen MIDI controller with faders, knobs, pads and keys, and play it with touch, a pen or a mouse
icon: /assets/images/midiglass.png
categories:
  - General Purpose MIDI Tools
---

> This page covers information about a Windows MIDI Services feature and application that will be released to consumers in November 2026. It's currently available for developers.

Windows MIDI Glass ("MIDI Glass") lets you build your own MIDI controller on screen. You put faders, knobs, buttons, pads and keys on a page, choose what each one sends, and then play it with your fingers, a pen or a mouse. It's at its best on a touch screen, or on a second monitor next to your keyboard, but it works on any PC.

Each design is called a **layout**. A layout can have several pages and send to several devices. Its controls can also move and light up when your gear or your DAW sends something back.

## Quick start

![The MIDI Glass library, with numbered callouts on the New layout button, a layout card, a card's device status, the search and sort controls, the Open a file and More options buttons, the Appearance and settings button, and the status bar]({{ site.baseurl }}/assets/images/midiglass-quick-start.png)

MIDI Glass opens to the **library**, where all your layouts are.

1. **New layout** makes a layout. You give it a name, pick the device it sends to, and pick a template to start from.
2. **Each card is a layout.** Click a card to run it. Point at a card to see **Run**, **Edit** and **…** for more options. You can also right-click a card for the same options.
3. **The status chip** on each card says whether the devices that layout sends to are connected right now.
4. **Search, sort and view.** Type to find a layout. Sort by last used, by name or by last changed. Switch between cards and a list.
5. **Open a file…** runs a layout from any folder. **…** (More options) has **Ask an AI assistant…**, imports a layout package, opens the backups folder, and can keep the PC awake while a layout runs.
6. **Appearance and settings** sets the app's light or dark theme and its window background, and turns **Ask an AI assistant** on or off.
7. **The status bar** shows how many layouts you have and where they're saved. The chip on the right says whether the MIDI service is running.

### Make your first layout

1. Select **New layout**.
2. Type a name. Under **Send to**, pick the device you want to play.
3. Pick a **Template**, then select **Create**. The layout opens in the editor.
4. Select **Try** near the top left, and play the page. The **Sending** list under the page shows what goes out.
5. Select **Library** to go back. You don't need to save, because MIDI Glass saves as you go. Click the new card to run your layout.

To play an app on the same PC, such as a DAW, pick **Default App Loopback (A)** under **Send to**. Then set the app to listen to **Default App Loopback (B)**. A loopback passes whatever goes into one side out of the other side.

## The editor

![The MIDI Glass editor, with numbered callouts on the Add pane, a selected fader on the page, the inspector tabs, the Edit and Try switch, the toolbar, the Layout button, and the page tabs and Sending list under the page]({{ site.baseurl }}/assets/images/midiglass-editor.png)

1. **Add** lists every kind of control. Click one, then click the page where you want it. You can also drag one onto the page, or double-click one to drop it in the first open spot. **Outline**, next to **Add**, lists every control on the page.
2. **The page.** Click a control to select it. Drag it to move it, or drag a corner or an edge to resize it. The arrow keys move it one pixel at a time. Hold Shift to move it one grid square at a time.
3. **The inspector** changes the selected control. **Look** sets its color, style, size and label. **Sends** sets what it sends. **Listens** sets what it does when a device sends something back. **Behavior** sets how it moves and where it starts.
4. **Edit and Try.** **Try** plays the page for real without leaving the editor. Ctrl+Enter switches between the two.
5. **The toolbar** has **Undo** and **Redo**, **Snap** and the grid size, the align and arrange buttons, **Repeat**, **Group** and **Lock**.
6. **Layout…** has the settings for the whole layout: its theme and colors, its pages and devices, the page size, a background picture or video, its name, and its keyboard order. **Save and run** is here too.
7. **Under the page** are the page tabs. **+ Page** adds a page. **Sending** lists each message the layout sends while you try it.

MIDI Glass saves your layout a moment after each change. The chip near the top right says **Saved** once it's done. Press Ctrl+S to save right away.

## Starting a new layout

**New layout** asks for three things:

- **Name** is what the card in the library shows. You can change it later.
- **Send to** is the device the controls play. You can add more devices later.
- **Template** is what the new layout starts with.

| Template | What you get |
| --- | --- |
| **Mixer** | Eight faders, eight knobs and eight pads on one page. |
| **DJ deck** | Two decks with filters, three band EQ, cue pads and a crossfader. |
| **Drum pads** | A four by four grid on channel 10, with the lowest note at the bottom left. |
| **Transport** | Play, stop, record and loop, plus eight track faders. |
| **Mackie Control** | Eight channel strips, a master fader, transport and a jog wheel, for a DAW set up to use a Mackie Control surface. See [How to control your DAW with Mackie Control in MIDI Glass]({{ site.baseurl }}/kb/midi-glass-mackie-control/). |
| **Horizontal toolbar** | Eight buttons in a see-through strip that stays in front of your other apps. |
| **Vertical toolbar** | Eight buttons in a see-through column that stays in front of your other apps. |
| **Floating palette** | Sixteen buttons in a see-through square that stays in front of your other apps. |
| **Blank** | An empty page, ready for you to build on. |

The toolbar and palette templates are explained in [How to make a floating toolbar or palette in MIDI Glass]({{ site.baseurl }}/kb/midi-glass-floating-toolbars/).

If no devices show up under **Send to**, plug one in, or make a loopback with [Windows MIDI Loopback Setup]({{ site.baseurl }}/tools/midiloopbacksetup/).

## Adding controls

The **Add** pane groups the controls by what they do.

| Group | Controls |
| --- | --- |
| Buttons | Button, Toggle, Pad, Page tab, Switch |
| Knobs and faders | Knob, Turntable, Fader, Wheel, Ribbon |
| Two axis | XY pad, Joystick |
| Generators | Beat clock, LFO, Steps |
| Keys and pads | Mono keyboard, Note pads, Hex pads |
| Feedback and text | Meter, Lamp, Readout, Stopwatch, Text, Image |
| Grouping | Group, Line |

A few of them need a word of explanation:

- A **Page tab** switches to another page of the layout when you press it.
- A **Switch** has two or more positions, and each position sends its own message.
- An **XY pad** or a **Joystick** sends two values at once: one for across and one for up and down.
- A **Ribbon** is a strip you slide a finger along. It sends one value, like a fader.
- **Generators** play by themselves once they start. **Beat clock** sends MIDI clock at the tempo you set. **LFO** sweeps a value up and down. **Steps** plays a short pattern of notes, one step at a time.
- **Note pads** and **Hex pads** take a finger on each pad, so you can play chords. The **Mono keyboard** plays one key at a time, and you can slide along it.
- **Meter**, **Lamp** and **Readout** show values that arrive from a device. You set that up on their **Listens** tab. A **Stopwatch** counts up from when the layout starts, so you can see how long you've been playing. Tap it to start again from zero.
- A **Group** control draws a frame around controls that belong together, and a **Line** divides one part of a page from another. Neither sends anything. Don't mix up the **Group** control with **Group** on the toolbar, which ties the selected controls together.

Type in **Find a control** to narrow the list. To add a control without the mouse, pick it with the keyboard and select **Add to page**. **Add to page** stays off until you pick a control.

## Arranging controls

- **Select more than one.** Hold Shift or Ctrl and click, or drag a box around them on an empty part of the page. Ctrl+A selects everything on the page that isn't locked.
- **Snap** lines controls up with the grid, with each other and with the page edges. Hold Alt while you drag to turn it off for a moment. Hold Shift while you drag to keep the move straight across or straight down.
- **Align and arrange** line up the selected controls, or spread them out evenly.
- **Gap labels.** Select three or more controls in a row or a column, and the gap between each pair shows as a number under the row or beside the column. When every gap is the same, the numbers are filled in. Click one, type a number and press Enter, and every gap becomes that many pixels. The first control stays where it is.
- **Repeat** makes a row or a column of copies of what's selected. It can count the channel, the note or controller number, or the group up on each copy, and number the labels too, so one channel strip becomes a whole bank.
- **Group**, on the toolbar, makes several controls act as one when you select and move them. Ctrl+G groups, and Ctrl+Shift+G ungroups. Lining up, spreading out and gap labels treat a group as one block too, so a channel strip keeps its shape. Select one group on its own to line up the controls inside it.
- **Lock** stops a control from being picked or moved on the page. That's handy for a panel or a picture behind other controls. A locked control shows a lock on the page and in the **Outline**. To unlock it, select it in the **Outline**, then select **Unlock**.

| Keys | What they do |
| --- | --- |
| Ctrl+Z, Ctrl+Y | Undo, redo |
| Ctrl+C, Ctrl+X, Ctrl+V | Copy, cut, paste |
| Ctrl+D | Duplicate the selection |
| Delete | Delete the selection |
| Arrow keys | Move the selection one pixel |
| Shift+arrow keys | Move the selection one grid square |
| F2 | Rename the selected control |
| Esc | Put away the control you picked in **Add** |
| Ctrl+Enter | Switch between **Edit** and **Try** |
| Ctrl+S | Save now |

## What a control sends

The **Sends** tab has an **Action list**. Each row in it says when to send, what to send, and where to send it.

- **Trigger** is one of these: **Value change**, **On**, **Off**, **Touch** or **Release**. A fader usually sends on **Value change**. A button usually sends one message on **On** and another on **Off**.
- **Type** can be a note, a control change, a program change, pitch bend, channel pressure, a per-note controller, an RPN or NRPN, or system exclusive.
- **To** is one of the layout's devices, with its group and channel.

The fastest way to fill in a row is **MIDI Learn**. Turn on **Learn**, then move a knob or press a key on your hardware, and MIDI Glass copies what it sends. **Learn a bank** fills several controls in a row: touch the knobs on your hardware one after another, and each one fills the next control in keyboard order. MIDI Learn listens to the devices in the layout's device list, so add your hardware there first.

Each device has a **Protocol** setting: **MIDI 2.0**, **MIDI 1.0** or **Mackie Control**. It decides how you type values on the **Sends** tab. [How MIDI Glass talks to each device]({{ site.baseurl }}/kb/midi-glass-mackie-control/#how-midi-glass-talks-to-each-device) explains the choices.

## Controls that listen

On the **Listens** tab, turn on **MIDI follow** and the control moves or lights up when a device sends something, not only when you touch it. Under **Follow mode**, pick one of these:

- **Single message**: the control follows one controller, note or pitch bend, and shows its value. Use it when your DAW should move a fader, the way it moves a motorized fader on a hardware desk.
- **Notes**: the control lights while any note is held.
- **Control changes**: the control blinks whenever a control change arrives.
- **Transport**: the control lights while the other end is playing.
- **Beat**: the control flashes on the beat.
- **Any activity**: the control lights whenever anything arrives, like an activity light.

## Trying a layout as you build it

**Try** turns the page on without leaving the editor. Every control sends for real, so you can check that a fader sends what you expect before you go any further. Ctrl+Enter switches back to **Edit**.

The **Sending** list under the page shows each message as it goes out. **Selected control only** narrows the list to the control you're working on. **Pause** freezes the list so you can read it, and **Clear** empties it. The arrow at the right end folds the list away to give the page more room.

## Pages

A layout can have as many pages as you like. The tabs under the page switch between them in the editor, and **+ Page** adds one. To let people change pages while the layout runs, put a **Page tab** control on each page.

Open **Layout…**, then **Pages and devices…**, to rename pages, change their order, or pin a band to every page. A band is a strip that's always on screen, such as a transport row or a panic button, so you only build it once.

**Layout…**, then **Page size…**, sets how big the page is, in pixels. Pick a size close to the screen the layout will run on.

## Running a layout

To run a layout, click its card in the library, or select **Run** on the card. You can also double-click a layout file in File Explorer. The first time you do, Windows asks which app to open it with, so pick MIDI Glass.

A running layout has its own window. The bar along the top has:

- **Page**, to switch pages.
- **Size on screen**: **Actual size**, **Fit to window**, or a custom size.
- **Always on top**, to keep the window in front of your other apps.
- **Full screen**. In full screen, everything on the bar moves behind one small button in a corner of the screen. It fades after a few seconds so it's out of the way. You can move it to another corner. Press Esc to leave full screen, or F11 to switch in and out.
- **Panic**, which stops every note on every device MIDI Glass is sending to. Ctrl+Shift+P does the same thing. Use it when a note gets stuck.

You can move more than one control at once with more than one finger: two faders, or a fader and a pad.

The bar also says whether the devices the layout sends to are connected. If one is missing, it says **Waiting for** and the device's name. Plug the device back in and the layout picks it up again by itself.

If a layout runs for a long time while nobody touches the PC, such as one that only shows meters or plays a beat clock, Windows may turn the screen off or go to sleep. To stop that, select **…** (More options) in the library and turn on **Keep this PC awake while a layout runs**.

## Devices

A layout keeps its own list of devices. Each one has a name, and the controls send to that name rather than straight to the hardware. So if you move to a different MIDI interface, you only change the device in one place, and every control follows.

Open **Layout…**, then **Pages and devices…**, and select **Outputs** to see the list. You can add a device, change which hardware one points to, and set how MIDI Glass talks to it.

A device that isn't connected is normal, not an error. The library card says how many are missing, and the layout sends to the rest. The status bar in the library says whether the MIDI service is running. If it isn't, nothing can be sent.

## Themes, colors and backgrounds

A **theme** sets how the whole layout looks: the page behind the controls, how each kind of control is drawn, and six colors. Open **Layout…**, then **Theme and colors…**, to pick one. Changing the theme changes every control at once, and you can change it as often as you like.

Each control uses one of the theme's six colors, chosen on its **Look** tab. A control remembers which of the six it uses, not the color itself. So when you change the theme, the colors change along with it and still mean the same thing.

**Layout…**, then **Background image…**, puts a picture or a video behind the controls. You can set how it fits the page and how see-through it is.

[How MIDI Glass themes work]({{ site.baseurl }}/kb/midi-glass-themes/) covers every theme setting, where theme files go, and how to make your own.

## Your layout files

Layouts are saved in **Documents › MIDI Layouts**, one file per layout. The files end in `.midilayout`. A picture or a video you add to a layout is copied next to it, so the layout and its pictures stay together.

- **Back up now**, on a card's **…** menu, saves a copy of the layout in the **Backups** folder next to your layouts.
- **Restore from a backup…** puts a backup back. MIDI Glass backs up the layout as it is now first, so you can change your mind.
- **Package for another PC…** makes one file that holds the layout and its pictures. Copy it to the other PC, and in the library there, select **…** (More options), then **Import a layout package…**.
- **Duplicate** makes a copy to try ideas on, and **Add to favorites** puts the layout in a **Favorites** section at the top of the library.

To have an AI assistant build a layout for you, select **…** (More options), then **Ask an AI assistant…**. It shows a starting prompt to paste into the AI assistant you use, such as a chat in your web browser. The prompt has the link to [MIDI Glass layout files, a guide for AI agents]({{ site.baseurl }}/kb/midi-glass-layouts-for-agents/), the names of your MIDI devices, and the themes you have, so the layout the assistant makes can find your devices. MIDI Glass doesn't send anything itself. If you'd rather not see it, open **Appearance and settings** and turn off **Ask an AI assistant**.

## Keyboard and screen readers

On a running layout, the Tab key moves from control to control, and a screen reader reads each control's name. The **keyboard order** decides the order Tab moves through the controls, and the order a screen reader reads them.

To set it, open **Layout…**, then **Set the keyboard order…**, and click the controls in the order you want. **Keyboard order**, under the page, sets the order from where the controls are on the page. You can also move a control up or down in the **Outline**.

Open **Layout…**, then **Pages and devices…**, and select **Accessibility check** to see whether the colors are easy to tell apart and whether every control has a label a screen reader can say.

## Learn more

- [How MIDI Glass themes work]({{ site.baseurl }}/kb/midi-glass-themes/)
- [How to control your DAW with Mackie Control in MIDI Glass]({{ site.baseurl }}/kb/midi-glass-mackie-control/)
- [How to make a floating toolbar or palette in MIDI Glass]({{ site.baseurl }}/kb/midi-glass-floating-toolbars/)
- [MIDI Glass layout files, a guide for AI agents]({{ site.baseurl }}/kb/midi-glass-layouts-for-agents/)
