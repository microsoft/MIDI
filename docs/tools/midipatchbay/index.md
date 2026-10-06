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

![The main Windows MIDI Patchbay window with five patch tiles and a New patch tile, and numbered callouts on the New patch and New quick patch buttons, a patch tile, the filter, the search box with the sort list and view buttons, the Ask an AI assistant and Import patch buttons, the Appearance and settings and keep on top buttons, and the status bar]({{ site.baseurl }}/assets/images/midipatchbay-quick-start.png)

1. **New patch** starts a blank patch, and **New quick patch** connects one device to another in two steps. The **New patch** tile at the end of your patches offers both.
2. **Each tile is a patch.** It shows a map of the patch and says whether its devices are here. Turn on the switch to start it routing. A routing patch has a green edge and shows how many messages go through it each second. Select a tile to open the patch in a window of its own.
3. **All**, **Routing**, **Not routing**, and **Needs attention** show only some of your patches. Each one says how many patches it would show.
4. **The search box** finds a patch by its name, its description, or the devices on it. The list next to it sorts your patches, and the two buttons after that show them as tiles or as a list.
5. **Ask an AI assistant…** helps an AI assistant build a patch for you, and **Import patch…** adds a patch file that somebody sent you. **…** has **Open patch folder**, which shows your patch files in File Explorer.
6. **Appearance and settings** changes the theme and the window background, and has settings such as **Run in notification area** and **Start patches automatically**. The pin keeps the window on top of other windows.
7. **The status bar** shows where your patches are, how many are routing, how many messages go through each second, and whether the MIDI service is running.

Routing happens only while Windows MIDI Patchbay is running. [Routing only runs while Patchbay is running](#routing-only-runs-while-patchbay-is-running) explains how to keep it going in the background.

## The patch window

![A patch open in a window of its own, with numbered callouts on the Steps and Endpoints panel, a connection, the details panel, the Routing button and Start automatically switch, the toolbar, the zoom controls, and the Test button]({{ site.baseurl }}/assets/images/midipatchbay-editor.png)

1. **Steps and Endpoints.** The panel on the left has everything you can add. Drag a device from **Endpoints** onto the canvas. Each one shows the picture set for it in MIDI Settings, or an empty square when it has none. Drag a step from **Steps** onto a connection to put it in the way, or anywhere on the canvas to connect it yourself.
2. **Draw a connection** by dragging from an **Out** point to an **In** point.
3. **The details panel** shows whatever you've selected. For a step, **Edit settings…** changes what it does.
4. **Routing** turns the patch on and off. **Start automatically** starts it each time Windows MIDI Patchbay starts.
5. **The toolbar** has **Undo**, **Redo**, **Cut**, **Copy**, **Paste**, and **Remove**, and buttons to add an endpoint, create a loopback, or arrange the canvas for you.
6. **The zoom controls**, at the bottom right of the canvas, change how much of the canvas you see. **Fit** shows all of it. The overview above them shows the whole patch.
7. **Test** opens a monitor, a keyboard, or a scratch pad pointed at the selected endpoint.

To give the canvas more room, or the panels more, drag the bar between the left panel and the canvas, or between the canvas and the details panel. Each patch window opens with the widths you last used.

## Your patches

A **patch** is one canvas: the endpoints on it, the steps, the connections between them, and a name. Patches are saved as files in **Documents &rsaquo; MIDI Patchbay**, one file per patch, so you can back one up or copy it to another PC.

The main MIDI Patchbay window shows each patch as a tile. A tile has a map of the patch, its name and description, and words that say whether its devices are here, such as **4 devices ready** or **1 device missing**. A missing device is dashed and red on the map. A routing patch has a green edge, and the number in its corner is how many messages go through it each second. The power mark beside the device count means the patch starts automatically, and **Not saved** means the patch is temporary.

Turn on the switch on a tile to start that patch. To open a patch, select its tile, or point at the tile and select **Edit**. Right-click a tile, or select **…** on it, to duplicate the patch, find its file, or delete it.

**All**, **Routing**, **Not routing**, and **Needs attention** show only some of your patches. **Needs attention** is any patch with a missing device, a loop, or a problem that keeps it from routing. The search box finds a patch by its name, its description, or the devices on it. The sort list puts your patches in order by when they last changed or by name. The two buttons beside it switch between tiles and a list. The list fits more patches on the screen, and it also shows how many steps and connections each one has.

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

The zoom controls at the bottom right of the canvas go from 10% to 400%. **Fit** shows everything on the patch at once, as large as the window allows. To type a zoom level, click the percentage. The overview above the zoom controls shows the whole patch, with a box around the part on screen. Drag the box to move around, or click anywhere in the overview to jump there.

## Undo, copy, and paste

Every change to a patch can be undone. **Undo** (Ctrl+Z) and **Redo** (Ctrl+Y) are on the toolbar of the patch window. Each patch window keeps its own list of changes, back to when you opened it.

To work with more than one endpoint or step at once, hold Ctrl and click each one. Then you can move them together, or **Copy** (Ctrl+C), **Cut** (Ctrl+X), and **Paste** (Ctrl+V) them, in the same patch or in another one. The connections between them come along. **Duplicate** (Ctrl+D) copies and pastes in one go, and Ctrl+A selects everything. Pasted steps are new copies, with settings of their own. A pasted endpoint that's already on the patch isn't added twice: the pasted connections use the one that's there.

## Steps

Steps sit between the endpoints and do something to the messages that pass through them. Drag one from the **Steps** tab on the left, or select a connection and choose **Add a step here**. Drop a step on a connection and the connection is split in two, with the step in the middle. A generator or an annotation is never put into a connection, so it's added on its own instead.

There are six kinds:

- **Filters** keep some messages out and let the rest through.
- **Transforms** change messages on the way past.
- The **message throttler** slows messages down for a device that can't keep up.
- **Distribution** steps decide which connection out a message takes, or whether it goes at all. See [Distribution steps](#distribution-steps).
- **MIDI-CI** steps answer MIDI-CI for a MIDI 1.0 device that can't, or keep MIDI-CI away from a device. See [MIDI-CI steps](#midi-ci-steps).
- **Generators** make messages of their own: MIDI clock, MIDI Time Code, and an LFO. See [Generators](#generators).

The **Steps** tab also has **Annotation**, a note on the canvas. It isn't a step. See [Annotations](#annotations).

Messages go through the steps in the order the connections lead them, and each step only sees what the steps before it let through. When an **Out** leads to more than one place, each one gets its own copy of every message, so a step on one path never changes what another path carries. When more than one connection leads into the same **In**, their messages are merged.

Select a step to see what it does, in a sentence, under **What it does**. When a step's settings are short, they're right there in the panel on the right, and a change takes effect as soon as you make it: the channel, group, and velocity filters, transpose, the message throttler, the clock divider, the note distributor, the MIDI-CI responder and filter, MIDI clock, and MIDI Time Code. Double-click one of those, and the panel is ready for you to change it. For the other steps, **Edit settings…**, or a double-click, opens their settings, as large as the patch window allows, so on a big screen a step with a lot of settings fits without scrolling. Nothing changes until you select **Apply**, and **Reset** puts the step back the way it started. Under **Name**, you can give a step a name of its own, such as "Bass side".

**Bypass** lets everything through a step unchanged, so you can hear the patch with and without it. A bypassed generator sends nothing.

Steps can't be connected in a circle, because nothing would ever leave it. Patchbay won't draw a connection that would close one.

### Filter steps

- **Message type filter** is the coarsest switch. **Message types** works at the UMP message type level. **Channel messages** applies to both MIDI 1.0 and MIDI 2.0 channel voice messages, so you can drop program changes or keep only notes. **System messages** is where you stop a device flooding everything downstream with timing clock or active sensing.
- **Channel filter** limits which of the sixteen channels get through. Messages without a channel, such as clock, always get through.
- **Group filter** limits which of the sixteen groups get through.
- **Note filter** picks out a range of notes, one note, or a list of notes, and either lets only those through or keeps them out. Click keys on the keyboard to pick them. For a range, click the lowest key, then hold Shift and click the highest. It applies to note on, note off, poly pressure and the MIDI 2.0 per note messages, so a held note can't be stranded. Everything else goes through.
- **Control change filter** does the same for controllers: a range, one controller, or a list, picked from a grid of numbers. Everything that isn't a control change goes through.
- **Velocity filter** picks out notes by how hard they're played, which is how you send soft playing to one sound and hard playing to another. It only looks at note on. Note off always goes through, so no note is left sounding.
- **(N)RPN filter** picks out RPNs and NRPNs by bank and index, such as RPN 0/0, pitch bend range, and lets only those through or keeps them out. Leave a bank or an index empty to match any. A MIDI 1.0 device sends a parameter as several control changes: the bank and index always go through, and the values that follow are let through or kept out. Everything that isn't an RPN or NRPN goes through.
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
- **(N)RPN transform** sends one RPN or NRPN as another, and can reshape its value with a curve, an output range, and **Invert**. The first row that matches is used. A MIDI 2.0 value is reshaped across its whole range. For a MIDI 1.0 device, the new bank and index are sent once both halves have arrived, and the coarse and fine values are reshaped together.

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

## Distribution steps

These decide where a message goes, rather than changing it.

- **Note distributor** plays several one-note synths as one bigger synth. Connect one synth to its **Out** for each voice you want. Each connection is a voice, in the order you connected them, and each new note goes to one of them. Its note off, poly pressure, and MIDI 2.0 per-note messages follow it there. **Take turns** moves on to the next voice for each note. **First free voice** uses the first one that isn't playing. When every voice is playing, both cut the oldest note short. **Keep the highest notes** and **Keep the lowest notes** only cut a note short for a higher or a lower one. Control changes, channel pressure, and pitch bend go to every voice, or, with their box cleared, only to the voice that played the latest note. All notes off, program changes, and everything else always go to every voice.
- **Gate** lets messages through only between one message and another. Pick what opens it and what closes it: a note on or off, a control change with a value at or above or below a number, a program change, Start, Continue, Stop, or an exact message typed in hex. When the same message opens and closes it, each one turns it the other way, like a footswitch. You can keep the opening and closing messages out or send them on, and choose whether it starts open. Note offs always get through, so no note is left sounding.

To play notes only while a sequencer is playing, connect the sequencer's clock and the keyboard into a gate left as it starts: it opens on Start and closes on Stop.

## MIDI-CI steps

MIDI-CI lets an app ask a device what it is and what it can do. A MIDI 2.0 device answers for itself. A MIDI 1.0 device can't, so Patchbay can answer for it.

- **MIDI-CI responder** answers MIDI-CI for the device it leads to. Put it on the path from the app to the device, usually right after the loopback the app uses. When the app looks for MIDI-CI devices, it finds one with the manufacturer ID, family, model, and software version you give the step. Use the numbers from the device's manual if it has them. The answers go back the way the question came, on the same group. Everything that isn't MIDI-CI, such as notes, goes on to the device as usual. MIDI-CI doesn't, unless you check **Send MIDI-CI on to the device too**, because most MIDI 1.0 devices don't understand it.
- **MIDI-CI filter** keeps MIDI-CI away from a device, or lets only MIDI-CI through. Pick which kinds: discovery and management, profiles, Property Exchange, and Process Inquiry. System exclusive that isn't MIDI-CI goes through when MIDI-CI is kept out, and is kept out when only MIDI-CI is let through.

With **Answer MIDI message reports** on, a responder tells an app which notes are playing, and where each channel's controllers, program, pitch bend, and channel pressure are. It only knows what went through it since the patch started routing, so a change made on the device itself doesn't show.

**Profiles and properties.** A MIDI-CI file tells the responder which profiles the device follows, such as one for drawbar organs, and gives an app properties to read, such as the device's program list. Put the file in the patch folder, then type its name under **MIDI-CI file**, or select **Choose…**. A file from somewhere else is copied into the patch folder first. **Open patch folder** opens the folder in File Explorer. While the patch routes, changes to the file are picked up straight away.

The file is JSON. [MIDI Patchbay patch files, a guide for AI agents]({{ site.baseurl }}/kb/midi-patchbay-patches-for-agents/#the-midi-ci-file) describes it, and an AI assistant can write one for you from the device's manual.

Under **Activity**, a responder shows what its file adds, any problems in the file, its MUID, and the last questions it answered.

Things a responder can't do:

- **It only answers.** It never asks other devices anything.
- **It can't change the device.** When an app turns a profile on or off, the responder says how the profile already is. An app can read properties, but can't change them or ask to hear when they change.
- **It can't answer what comes through a message throttler or from a generator,** because it doesn't know where to send the answer.
- **It stands for one device.** To answer for two devices, put a responder on each path.
- **It stops answering when routing stops.** An app that found it may still list it until the app looks again.

## Generators

Generators make messages of their own. MIDI clock and MIDI Time Code only have an **Out**. An LFO also has an **In**, for a clock to follow. A generator runs for as long as its patch is routing: it starts when you start the patch and stops when you stop it. Connect it to everything that should get what it sends, straight or through other steps.

- **MIDI clock** sends 24 timing clock pulses for every beat, at the tempo you set. With **Send Start and Stop messages** on, it sends Start as routing starts and Stop as it stops, so a sequencer or drum machine plays along. It can swing eighth notes or sixteenth notes, up to 75%. Select the step to change its tempo right in the panel on the right.
- **MIDI Time Code** sends quarter frame messages at 24, 25, 29.97 drop frame, or 30 frames per second, counting from the start time you set. It starts from that time each time the patch starts routing. With **Send a full timecode when starting and stopping** on, a device finds its place at once.
- **LFO** sweeps a value up and down in the same shapes as an LFO control in MIDI Glass: sine, triangle, square, a ramp up or down, or one of four kinds of noise. Pick how long one pass takes, in beats at a tempo you set, and how much of the range it covers. It can send a control change, pitch bend, channel pressure, poly pressure on one note, or an RPN or NRPN, on the channel and group you pick. **Return to center when routing stops** sends the value halfway between the two ends as it stops, so a pitch bend that sweeps its whole range ends up back in the middle.

**Keeping an LFO in step with a clock.** Connect a clock to the LFO's **In**: a device that sends MIDI clock, a MIDI clock step, or a clock divider. The LFO then follows that clock instead of its own tempo. One pass takes that many beats of the clock, a Start from the clock puts the LFO back at the beginning of a pass, and when the clock stops, the LFO stops moving. Only timing clock, Start, Continue, Stop and Song Position reach the LFO. Anything else that comes in stops there. With nothing connected to its **In**, an LFO keeps its own tempo. A muted connection still counts, so muting the clock holds the LFO still.

With **Start and stop with the clock** checked, the LFO waits for a Start or Continue before it moves, and a Stop holds it until the next one. It goes back to the middle on Stop if **Return to center when routing stops** is checked. Without it, the LFO moves with every timing clock, whether the music is playing or not.

The clock and the time code go out on the group you pick. A connection to one group or port of an endpoint sends them there instead.

**One clock for a whole setup.** Put a MIDI clock step on a patch and connect it to every device that should follow it. A device that should run at half speed gets its clock through a **Clock divider** set to 2. An LFO connected to the same clock sweeps in time with it. Make a patch like this for each project, each with its own tempo, and start the one you need.

Changing a generator while its patch is routing doesn't start it again. A new tempo, more or less swing, and every change to an LFO take effect right away. Changing the clock's group, what its swing applies to, or **Send Start and Stop messages**, or anything about the time code, starts it again from the top. So does connecting a clock to an LFO, or taking it away.

MIDI clock and MIDI Time Code don't follow a clock that comes in from a device, and messages don't start or stop them. They run whenever their patch routes.

## Annotations

An annotation is a note on the canvas about the patch, such as which keyboard is which or what a split is for. Drag **Annotation** from the **Steps** tab onto the canvas, and type the text in the details panel on the right. The canvas shows it as you type. Press Enter to start a new line. Lines break only where you press Enter, so a long line stays on one line. An annotation can hold up to 1,000 characters.

The details panel also sets the font, its size, bold, italic, and underline, and the color. The font list has the fonts every Windows PC has. **Show all fonts** lists every font on your PC, but if you share the patch, a PC without that font shows the annotation in the standard font.

An annotation has no **In** or **Out**, so nothing connects to it, and it doesn't change what routes. It isn't counted with the steps in the status bar, and **Auto arrange** leaves it where you put it. To change the text later, double-click the annotation, or right-click it and choose **Edit text**.

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

An endpoint that isn't connected right now is drawn with a dashed outline and a warning badge, its patch's tile says a device is missing, and the connections to it sit idle. Everything else in the patch keeps routing. The moment the device comes back, its connections start carrying messages again without you doing anything.

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
