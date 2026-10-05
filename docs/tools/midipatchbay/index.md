---
layout: tools_page
title: Windows MIDI Patchbay
tool: midipatchbay
description: Route MIDI between endpoints on a canvas you draw yourself
icon: /assets/images/midipatchbay.png
categories:
  - General Purpose MIDI Tools
---

> This page covers information about a Windows MIDI Services feature and application that will be released to consumers in November 2026. It's currently available for developers.

Windows MIDI Patchbay connects MIDI endpoints to each other. You drop the devices you care about onto a canvas, draw connections from one device's **Out** to another device's **In**, and from then on everything arriving on that connection point is passed along. Steps along the way can keep some messages out or change them, so one keyboard can play two instruments split at middle C, or a pedal that works backwards can be turned around.

It's the software version of the patchbay in a studio rack: keys into a sound module, a drum machine into your DAW, one controller split across three instruments.

## Quick start

![The main Windows MIDI Patchbay window with four patch tiles, and numbered callouts on the toolbar, a patch tile, the filter buttons, the search box and Sort button, the Appearance and settings and keep on top buttons, and the status bar]({{ site.baseurl }}/assets/images/midipatchbay-quick-start.png)

1. **New patch** starts a blank patch, and **New quick patch** connects one device to another in two steps. **Import patch…** adds a patch file that somebody sent you, **Ask an AI assistant…** helps an AI assistant build one for you, and **Open patch folder** shows your patch files in File Explorer.
2. **Each tile is a patch.** It shows the devices the patch uses, a small map of its canvas, and whether it's routing. Select a tile to open the patch in a window of its own. Turn on the switch to start it routing.
3. **All**, **Routing**, **Not routing**, and **Needs attention** show only some of your patches.
4. **The search box** finds a patch by its name, its description, or the devices on it. **Sort** changes the order of the tiles.
5. **Appearance and settings** changes the theme and the window background, and has settings such as **Run in notification area** and **Start patches automatically**. The pin keeps the window on top of other windows.
6. **The status bar** shows how many patches you have, how many are routing, and how many messages go through each second.

Routing happens only while Windows MIDI Patchbay is running. [Routing only runs while Patchbay is running](#routing-only-runs-while-patchbay-is-running) explains how to keep it going in the background.

## The patch window

![A patch open in a window of its own, with numbered callouts on the Steps and Endpoints panel, a connection, the details panel, the Routing button and Start automatically switch, the toolbar, and the zoom and Test buttons]({{ site.baseurl }}/assets/images/midipatchbay-editor.png)

1. **Steps and Endpoints.** The panel on the left has everything you can add. Drag a device from **Endpoints** onto the canvas. Drag a step from **Steps** onto a connection to put it in the way, or anywhere on the canvas to connect it yourself.
2. **Draw a connection** by dragging from an **Out** point to an **In** point.
3. **The details panel** shows whatever you've selected. For a step, **Edit settings…** changes what it does.
4. **Routing** turns the patch on and off. **Start automatically** starts it each time Windows MIDI Patchbay starts.
5. **The toolbar** has **Undo**, **Redo**, **Cut**, **Copy**, **Paste**, and **Remove**, and buttons to add an endpoint, create a loopback, or arrange the canvas for you.
6. **The zoom buttons** change how much of the canvas you see. **Test** opens a monitor, a keyboard, or a scratch pad pointed at the selected endpoint.

## Your patches

A **patch** is one canvas: the endpoints on it, the steps, the connections between them, and a name. Patches are saved as files in **Documents &rsaquo; MIDI Patchbay**, one file per patch, so you can back one up or copy it to another PC.

The main MIDI Patchbay window shows each patch as a tile, with the devices it uses and whether it's routing. Turn on the switch on a tile to start that patch, and select the tile to open it. Right-click a tile to duplicate the patch, find its file, or delete it. **All**, **Routing**, **Not routing**, and **Needs attention** show only some of your patches, and the search box finds a patch by its name, its description, or the devices on it.

Patch files end in `.midipatch`. To add one that somebody sent you, or one an AI assistant saved in another folder, select **Import patch…** and pick the file. You can also double-click it in File Explorer. The first time you do, Windows asks which app to open it with, so pick MIDI Patchbay. Either way, the patch is copied into your patches. It doesn't route, and it doesn't start automatically, until you turn those on. A file from somewhere else shouldn't connect your devices before you've looked at it.

To have an AI assistant build a patch for you, select **Ask an AI assistant…** next to **Import patch…**. It shows a starting prompt to paste into the AI assistant you use, such as a chat in your web browser. The prompt has the link to [MIDI Patchbay patch files, a guide for AI agents]({{ site.baseurl }}/kb/midi-patchbay-patches-for-agents/), and the names of your MIDI devices and the groups they use, so the patch the assistant makes can find them. Patchbay doesn't send anything itself. If you'd rather not see it, turn off **Ask an AI assistant** in the appearance and settings flyout.

Older versions of Patchbay named patch files `.midipatch.json`. Patchbay renames them to `.midipatch` the next time it starts. If a file with the new name is already there, the old one is left alone.

You can have as many patches as you like, and more than one can be routing at the same time. Each patch opens in a window of its own, and a patch keeps routing after you close its window.

When a patch isn't routing, a warning bar across the top of its window says so. Select **Start routing** on the bar to turn it on, and the bar goes away.

To have a saved patch start routing by itself every time Patchbay starts, turn on **Start automatically** next to the routing button. While it's off, another bar reminds you that you'll need to start the patch yourself each time. You can close that reminder. A temporary patch can't start by itself, so turning the switch on for one asks you to save it first. Turn off the **Start patches automatically** setting to stop every patch from starting by itself.

A patch you don't name is **temporary**: it routes right now and disappears when Patchbay closes. Nothing is written to disk. Give it a name and it sticks around.

**New quick patch** is the fastest way to get going: pick a source, pick a destination, and you have a patch with one connection in it. **New patch** gives you a blank canvas to build on instead.

### Patches from earlier versions

Earlier versions of Patchbay kept the filters, the changes, and the sending speed of a connection on the connection itself. Now each of those is a step on the canvas. When you open a patch made with an earlier version, Patchbay turns them into steps, and the patch routes exactly the way it did. A bar across the top of the patch says so.

Patchbay keeps a copy of the original file in **Documents &rsaquo; MIDI Patchbay &rsaquo; Earlier versions**. Select **Show the original** on the bar to find it. Earlier versions of Patchbay can't open the new files, so keep that copy if you might go back.

In a few rare cases, a note mapping next to a transpose can't be carried over exactly, because the transpose would push the note past the top or bottom of the keyboard. The bar lists any note that's affected.

## Connection points

Each endpoint on the canvas has two columns.

- **In** is what Patchbay sends *to* that device.
- **Out** is what Patchbay receives *from* it.

There's a row for each group the endpoint declares, labeled with the group's number and the name the device gives that group. The name comes from the device's function blocks, or from its group terminal blocks when no function block names the group. Each endpoint grows wide enough to show its longest name. If a name is longer still, point at the row to see all of it. Above them is an **All groups** row: connect that and everything passes through with its group untouched, which is usually what you want when you just mean "send this device to that one".

Connect a specific group to a different specific group and Patchbay rewrites the group as the message goes past. That's how you fold four groups of one device onto one group of another.

A step has one **In** and one **Out**, and both carry every group.

You can draw a connection by dragging from an Out point to an In point, or by clicking the Out point and then clicking the In point. The second way also works from the keyboard. You don't have to land exactly on the point &mdash; get close and the connection snaps to it.

To change where an existing connection goes, drag the end of the cord onto a different point. Drag an endpoint or a step by its title bar to move it out of the way. Select a connection, an endpoint, or a step and press **Delete** to remove it. The first time, Patchbay asks, and offers to stop asking.

The zoom controls above the canvas go from 10% to 400%. Click the zoom percentage and choose **Fit to screen** to see everything on the patch at once, as large as the window allows. The overview in the corner of the canvas shows the whole patch, with a box around the part on screen. Drag the box to move around, or click anywhere in the overview to jump there.

## Undo, copy, and paste

Every change to a patch can be undone. **Undo** (Ctrl+Z) and **Redo** (Ctrl+Y) are on the toolbar of the patch window. Each patch window keeps its own list of changes, back to when you opened it.

To work with more than one endpoint or step at once, hold Ctrl and click each one. Then you can move them together, or **Copy** (Ctrl+C), **Cut** (Ctrl+X), and **Paste** (Ctrl+V) them, in the same patch or in another one. The connections between them come along. **Duplicate** (Ctrl+D) copies and pastes in one go, and Ctrl+A selects everything. Pasted steps are new copies, with settings of their own. A pasted endpoint that's already on the patch isn't added twice: the pasted connections use the one that's there.

## Steps

Steps sit between the endpoints and do something to the messages that pass through them. Drag one from the **Steps** tab on the left, or select a connection and choose **Add a step here**. Drop a step on a connection and the connection is split in two, with the step in the middle. A generator has no **In**, so it's added on its own instead.

There are four kinds:

- **Filters** keep some messages out and let the rest through.
- **Transforms** change messages on the way past.
- The **message throttler** slows messages down for a device that can't keep up.
- **Generators** make messages of their own: MIDI clock, MIDI Time Code, and an LFO. See [Generators](#generators).

Messages go through the steps in the order the connections lead them, and each step only sees what the steps before it let through. When an **Out** leads to more than one place, each one gets its own copy of every message, so a step on one path never changes what another path carries. When more than one connection leads into the same **In**, their messages are merged.

Select a step to see what it does, in a sentence, under **What it does**. **Edit settings…** opens its settings. Nothing changes until you select **Apply**, and **Reset** puts the step back the way it started. Under **Name**, you can give a step a name of its own, such as "Bass side".

**Bypass** lets everything through a step unchanged, so you can hear the patch with and without it. A bypassed generator sends nothing.

Steps can't be connected in a circle, because nothing would ever leave it. Patchbay won't draw a connection that would close one.

### Filter steps

- **Message type filter** is the coarsest switch. **Message types** works at the UMP message type level. **Channel messages** applies to both MIDI 1.0 and MIDI 2.0 channel voice messages, so you can drop program changes or keep only notes. **System messages** is where you stop a device flooding everything downstream with timing clock or active sensing.
- **Channel filter** limits which of the sixteen channels get through. Messages without a channel, such as clock, always get through.
- **Group filter** limits which of the sixteen groups get through.
- **Note filter** picks out a range of notes, one note, or a list of notes, and either lets only those through or keeps them out. Click keys on the keyboard to pick them. For a range, click the lowest key, then hold Shift and click the highest. It applies to note on, note off, poly pressure and the MIDI 2.0 per note messages, so a held note can't be stranded. Everything else goes through.
- **Control change filter** does the same for controllers: a range, one controller, or a list, picked from a grid of numbers. Everything that isn't a control change goes through.
- **Velocity filter** picks out notes by how hard they're played, which is how you send soft playing to one sound and hard playing to another. It only looks at note on. Note off always goes through, so no note is left sounding.
- **Message mask filter** is for anything the others don't cover. It looks at the bits in messages of one size, such as "word 1, bits 14 down to 8", and lets the matching ones through or keeps them out. Messages of other sizes go through.

The note filter and the control change filter can also **Learn**. Turn it on and play the notes, or move the controls, on your device. Learn listens to the devices connected to the step's **In**, through any steps before it, so connect the step first.

![The settings of a note filter named Upper keys. It lets notes C3 (60) to G8 (127) through, and the keyboard shows that range]({{ site.baseurl }}/assets/images/midipatchbay-note-range.png)

To split a keyboard across two instruments, connect the keyboard to two note filters, one for the notes below the split and one for the notes above it, and connect each filter to its instrument.

### Transform steps

Transforms change messages on the way past. Between them they do everything the old Windows MIDI Mapper did, and quite a lot it couldn't.

- **Channel mapper** moves everything on one channel to another. Only messages that carry a channel are affected, and channels you don't list are passed through.
- **Group mapper** moves everything in one group to another, and passes the groups you don't list through.
- **Transpose** shifts every note, including aftertouch and the MIDI 2.0 per note messages. A note pushed past either end is clamped rather than wrapped, so nothing lands an octave out.
- **Note mapper** sends one note as another. Notes you don't list go through unchanged. **Play** sends the new note so you can hear where it lands.
- **Velocity rescaler** reshapes the ramp, linear to curved or curved to linear. It can rescale into a narrower range so a light touch still speaks and a heavy one doesn't max out, or send every note at the same fixed velocity. Only note on messages are touched, and a MIDI 1.0 note on with velocity zero is a note off, so it's always left alone.
- **Aftertouch rescaler** reshapes channel pressure and poly pressure. Input low is how hard you have to press before anything is sent, input high is how hard counts as full, and the output range sets what comes out. Slow rise makes full pressure harder to reach, and fast rise makes it easier. A pressure of zero means you let go of the key, so zero always goes out as zero and no sound is left bent.
- **Control change mapper** moves a controller to a different number. Controllers you don't list are passed through.
- **Control change values** changes what a controller's value does on its way out. See [Shaping controller values](#shaping-controller-values) below.
- **Program and bank mapper** picks a different sound on the destination, for an instrument whose programs aren't laid out the way the music expects. The program numbers are the ones on the wire, 0 to 127, with the General MIDI name beside each one. It can also remap the bank, as controller 0 and controller 32 on MIDI 1.0 and as the bank a MIDI 2.0 program change carries. Most instruments only use the MSB half.
- **Clock divider** lets one MIDI clock pulse in every so many through, so a device that follows it runs at half the tempo, a third, and so on. Start sets the count back, so the first pulse after it always goes through. A song position is divided to match, and everything else goes through untouched.

### Shaping controller values

Each rule in a **Control change values** step names one controller and says what happens to its value. A small graph beside the rule shows the result: the dashed line is the value going in, and the solid line is what comes out. Some everyday fixes:

- **A sustain pedal that works backwards.** Add a rule for CC 64 and check **Invert**.
- **An expression pedal that never reaches the ends.** If it only goes from 10 to 117, set input low to 10 and input high to 117. It then covers the whole range.
- **A mod wheel that's too strong.** Set output high to 60, and the top of the wheel sends 60.
- **A volume pedal that jumps too fast near the bottom.** Choose **Slow rise**, which gives you finer control at the quiet end. **Fast rise** does the opposite.

Invert happens before the curve, so a backwards pedal is put the right way round first and then shaped like any other pedal. If you type the ends of a range high to low, that range runs backwards, so the numbers always mean what they say.

A rule applies to the controller number as it arrives at the step. If a control change mapper before it moves CC 1 to CC 11, the rule for CC 11 is the one that shapes it. MIDI 2.0 controllers are shaped at their full resolution.

Rules work on one message at a time. Some controllers split a value across two controller numbers for extra detail, such as CC 1 with CC 33. Those are best left without a rule, because each half would be shaped on its own.

### Showing values as 0 to 127 or as a percentage

MIDI 2.0 carries velocity in sixteen bits and controller values in thirty-two, so a percentage is what those values really mean, and that's how Patchbay stores them. But plenty of controllers give each of the 128 MIDI 1.0 steps its own meaning, such as a pad color, and typing 0.79% when you mean step 1 is no fun.

So the settings for the velocity rescaler, the aftertouch rescaler, and control change values steps start with a choice: **0 to 127** or **Percentage**. The value is the same either way; only the way you type it changes. A patch saved before this choice existed opens as 0 to 127, because that's all it could have meant. A new step starts as a percentage.

### MIDI 2.0 notes that carry an exact pitch

A MIDI 2.0 note on can say exactly which pitch to play, down to a fraction of a semitone. That makes its note number an address rather than a pitch, and moving one without moving the other would produce a message asking for one note and the pitch of another.

Leave **Bypass exact-pitch notes** clear and the pitch moves with the note, so the transpose and note mapper steps both stay honest. Check it and those notes are passed through untouched, which is what you want when the note number means a drum pad or a key on a controller rather than a pitch.

## Generators

Generators make messages of their own, so they only have an **Out**. A generator runs for as long as its patch is routing: it starts when you start the patch and stops when you stop it. Connect it to everything that should get what it sends, straight or through other steps.

- **MIDI clock** sends 24 timing clock pulses for every beat, at the tempo you set. With **Send Start and Stop messages** on, it sends Start as routing starts and Stop as it stops, so a sequencer or drum machine plays along. It can swing eighth notes or sixteenth notes, up to 75%. Select the step to change its tempo right in the panel on the right.
- **MIDI Time Code** sends quarter frame messages at 24, 25, 29.97 drop frame, or 30 frames per second, counting from the start time you set. It starts from that time each time the patch starts routing. With **Send a full timecode when starting and stopping** on, a device finds its place at once.
- **LFO** sweeps a value up and down in the same shapes as an LFO control in MIDI Glass: sine, triangle, square, a ramp up or down, or one of four kinds of noise. Pick how long one pass takes, in beats at a tempo you set, and how much of the range it covers. It can send a control change, pitch bend, channel pressure, poly pressure on one note, or an RPN or NRPN, on the channel and group you pick. **Return to center when routing stops** sends the value halfway between the two ends as it stops, so a pitch bend that sweeps its whole range ends up back in the middle.

The clock and the time code go out on the group you pick. A connection to one group or port of an endpoint sends them there instead.

**One clock for a whole setup.** Put a MIDI clock step on a patch and connect it to every device that should follow it. A device that should run at half speed gets its clock through a **Clock divider** set to 2. Make a patch like this for each project, each with its own tempo, and start the one you need.

Changing a generator while its patch is routing doesn't start it again. A new tempo, more or less swing, and every change to an LFO take effect right away. Changing the clock's group, what its swing applies to, or **Send Start and Stop messages**, or anything about the time code, starts it again from the top.

A generator doesn't follow a clock that comes in from a device, and messages don't start or stop it. It runs whenever its patch routes. Each generator has its own tempo, so an LFO doesn't follow a clock step: give them both the same tempo.

## Slowing messages down

A USB or network connection can carry MIDI many times faster than the 5-pin DIN cable most MIDI hardware was built around. Some devices can't keep up when a lot of data arrives at once. A synth taking a long SysEx dump, a device with a small buffer, or one with a slow processor can lose messages, play notes late, or stop responding.

Filters are often the first fix. Keeping out messages a device doesn't use, such as clock, active sensing, or aftertouch, leaves more room for the ones it needs. When that isn't enough, put a **Message throttler** step in front of the device and pick its speed:

- **Unlimited** sends messages as fast as they arrive.
- **MIDI 1.0 wire speed** is the speed of a 5-pin DIN MIDI cable, 31,250 bits a second. A new throttler starts here.
- **2×** to **32× MIDI 1.0 wire speed** are that many times faster. 32× is about a megabit a second.

Start with MIDI 1.0 wire speed for a device that was built for a DIN cable, and go faster if it copes. These are the same choices Network MIDI Setup offers for network MIDI devices.

Put the throttler last, right before the device, so its speed is spent only on the messages that actually go there. Speed is counted in the bytes the same messages would take on a MIDI 1.0 cable.

**A single note or knob turn is never held back.** After a quiet moment, a short burst goes out right away: 64 bytes at MIDI 1.0 wire speed, and twice that at twice the speed. Only what comes after that is spaced out. So playing a keyboard feels the same, while a 3,000-byte SysEx dump at MIDI 1.0 wire speed takes about a second, just as it would over a cable.

**Patchbay holds the messages that are waiting** and sends them as fast as the speed allows. It never slows down the device or app that's sending to it. Select the throttler to see how many messages are waiting, under **Activity**. A throttler can hold minutes' worth of messages at MIDI 1.0 wire speed. If even more arrives, the newest messages are dropped, and **Activity** says how many.

Everything connected into one throttler shares its speed. Connect everything that goes to a slow device through one throttler, and the device never gets more than that. Two throttlers in front of the same device each send at their own speed, so together they can send it twice as much. A patch from an earlier version gets a throttler for each connection that had a sending speed, so it works the way it did.

When the routing changes, such as when you edit a step or a connection, Patchbay keeps its connections to devices open, but messages that are still waiting to be sent are dropped. Let a long transfer finish before you change things.

### Waiting for each send to complete

When an app sends MIDI, it can ask Windows to wait until the device's driver has taken each message before the app sends the next one. That keeps a fast app from piling messages up in front of a slow device. Older apps that use the Windows multimedia MIDI API (WinMM) always wait like this.

That waiting stops at a loopback, though. An app sending to a loopback only waits for the loopback, and Patchbay then passes the messages on to the device as fast as they come. To put the waiting back, open the patch's **…** menu and turn on **Wait for send complete**. Patchbay then waits until the device's driver has taken each message before it sends the next one.

It applies to every connection in the patch, because it changes how Patchbay connects to each device. Like a throttler, it never holds up the device or app that's sending to Patchbay. Patchbay holds the messages that are waiting, and **Activity** shows how many there are.

You can use both together. The throttler sets the most it sends in a second, and waiting makes sure the device has taken each message before the next one goes.

## Routing only runs while Patchbay is running

This is the important limitation. Patchbay routes by receiving messages in its own process and sending them back out, so the routes exist only while the app is open. Two settings in the appearance and settings flyout, in the main MIDI Patchbay window, deal with that:

- **Start with Windows** launches Patchbay when you sign in.
- **Run in notification area** means closing or minimizing the main window puts Patchbay in the notification area with the routes still up. Click the icon to bring the window back, or right-click it for the list of patches, to stop everything, or to exit properly.

Both are off unless you turn them on. Patchbay doesn't put itself in the notification area uninvited.

Closing a patch's own window never stops it routing. Closing the main window with **Run in notification area** off closes Patchbay, so Patchbay asks first when a patch is routing.

## Loopbacks

Because Patchbay works inside its own process, it can only route *from* a message source *to* a message destination. It can't reach inside another app. A **loopback** is how you bridge that gap: it's a real MIDI endpoint on the PC that any app can open, so your DAW connects to the loopback and Patchbay feeds the loopback.

**Create loopback** on the toolbar of a patch window, or at the bottom of the **Endpoints** tab, makes one without leaving the canvas, and drops it straight onto the patch. It uses the same Windows MIDI Services feature that [MIDI Loopback Setup]({{ site.baseurl }}/tools/midiloopbacksetup/) does, so a loopback made here is a normal endpoint that every app sees, and it can stick around after Patchbay closes.

> **Tip:** Some apps remember a device by its name. To filter what one of those apps receives from a device without breaking that, give a loopback the device's name. First rename the device with **Customize** in [MIDI Settings]({{ site.baseurl }}/tools/settings/). Older apps see the device's MIDI 1.0 ports rather than the device itself, so rename those too, with **Edit port names**. New port names take effect when the MIDI service restarts. Then create a **MIDI 1.0 basic loopback** with the device's original name, connect the device's **Out** to the loopback's **In**, and put the filter steps you want between them. The app finds the loopback under the name it remembers, and gets only what your filters let through.
>
> This works best for a device the app only listens to. A basic loopback sends whatever it's given straight back out, so anything the app sends to that name comes back to the app instead of reaching the device.

## When a device isn't there

Gear gets unplugged. That's a normal state, not an error.

An endpoint that isn't connected right now is drawn with a dashed outline and a warning badge, its patch's tile says it's waiting for a device, and the connections to it sit idle. Everything else in the patch keeps routing. The moment the device comes back, its connections start carrying messages again without you doing anything.

If the device came back with a different identity &mdash; a USB device with no serial number moved to another port, for example &mdash; Patchbay notices a likely match and offers it. It never binds to a different device on its own.

Selecting an endpoint shows **Match by** in the details panel:

- **Device ID** is the default, and the safest.
- **USB model** matches the USB vendor and product ID, so it finds the same model in any port.
- **Name** is a last resort, because two identical devices look the same.

## Loops

Sending a device's output back to its own input, directly or the long way around, floods every device on the path within a second. Patchbay walks the patch after every change and looks for that.

When the circle is certain &mdash; when every endpoint on it is a loopback, so it's known that what goes in comes back out &mdash; the connection that closes it is left in place but held muted, and the whole circle is drawn in red. The message names the hops in order so you can see which one to remove.

When the circle only closes *if* a piece of hardware echoes what it receives, which Patchbay can't see from outside, it says so and mutes nothing.

Patchbay only knows about the connections it routes itself. A DIN cable between two devices, or another routing app, can close a circle it can't see. That's why the status bar says "no loops detected" rather than "no loops".

Steps can't be connected in a circle at all. Patchbay won't draw a connection that would close one, and a patch file that has one doesn't route until it's fixed.

## Testing a route

Right-click an endpoint, or use **Test** on the toolbar, to open [MIDI Monitor]({{ site.baseurl }}/tools/midi2monitor/), the MIDI keyboard or the [scratch pad]({{ site.baseurl }}/tools/midiscratchpad/) already pointed at that endpoint. Selecting a connection shows a running count of what it has forwarded, and selecting a step shows how much it let through and kept out. That's how you tell "nothing is arriving" apart from "arriving and going nowhere".
