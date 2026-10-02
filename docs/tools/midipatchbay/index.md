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

Windows MIDI Patchbay connects MIDI endpoints to each other. You drop the devices you care about onto a canvas, draw connections from one device's **Out** to another device's **In**, and from then on everything arriving on that connection point is passed along.

It's the software version of the patchbay in a studio rack: keys into a sound module, a drum machine into your DAW, one controller split across three instruments.

## Quick start

![The Windows MIDI Patchbay window, with numbered callouts on the patch list, the Add endpoint and Create loopback buttons, an endpoint on the canvas, a connection, the details panel, the Routing and Start automatically controls, and the zoom and Test buttons]({{ site.baseurl }}/assets/images/midipatchbay-quick-start.png)

1. **Patches.** **New quick patch** connects one device to another in two steps. **New empty patch** gives you a blank canvas. Your saved patches are listed underneath.
2. **Add endpoint** puts a device on the canvas. **Create loopback** makes a new loopback, so you can send MIDI to another app.
3. **Each endpoint** has an **In** column and an **Out** column, with a row for each group.
4. **Draw a connection** by dragging from an **Out** point to an **In** point. Select the cord to see what it carries.
5. **The details panel** shows whatever you've selected. For a connection, **Edit filters** and **Edit transforms** change what it passes along.
6. **Routing** turns the patch on and off. **Start automatically** starts it each time Windows MIDI Patchbay starts.
7. **The zoom buttons** change how much of the canvas you see. **Test** opens a monitor, a keyboard, or a scratch pad pointed at the selected endpoint.

Routing happens only while Windows MIDI Patchbay is running. [Routing only runs while Patchbay is running](#routing-only-runs-while-patchbay-is-running) explains how to keep it going in the background.

## What a patch is

A **patch** is one canvas: the endpoints on it, the connections between them, and a name. Patches are saved as files in **Documents &rsaquo; MIDI Patchbay**, one file per patch, so you can back one up or copy it to another PC.

Patch files end in `.midipatch`. To add one that somebody sent you, or one an AI assistant saved in another folder, select **Import patch…** and pick the file. You can also double-click it in File Explorer. The first time you do, Windows asks which app to open it with, so pick MIDI Patchbay. Either way, the patch is copied into your patches. It doesn't route, and it doesn't start automatically, until you turn those on. A file from somewhere else shouldn't connect your devices before you've looked at it.

To have an AI assistant build a patch for you, select **Ask an AI assistant…** under **Import patch…**. It shows a starting prompt to paste into the AI assistant you use, such as a chat in your web browser. The prompt has the link to [MIDI Patchbay patch files, a guide for AI agents]({{ site.baseurl }}/kb/midi-patchbay-patches-for-agents/) and the names of your MIDI devices, so the patch the assistant makes can find them. Patchbay doesn't send anything itself. If you'd rather not see it, turn off **Ask an AI assistant** in the appearance and settings flyout.

Older versions of Patchbay named patch files `.midipatch.json`. Patchbay renames them to `.midipatch` the next time it starts. If a file with the new name is already there, the old one is left alone.

You can have as many patches as you like, and more than one can be routing at the same time. Whether a patch is routing is separate from whether it's the one on screen, so you can look at one patch while three others are working.

When the patch on screen isn't routing, a warning bar across the top says so. Select **Start routing** on the bar to turn it on, and the bar goes away.

To have a saved patch start routing by itself every time Patchbay starts, turn on **Start automatically** next to the routing button. While it's off, another bar reminds you that you'll need to start the patch yourself each time. You can close that reminder. A temporary patch can't start by itself, so turning the switch on for one asks you to save it first. Turn off the **Start patches automatically** setting to stop every patch from starting by itself.

A patch you don't name is **temporary**: it routes right now and disappears when Patchbay closes. Nothing is written to disk. Give it a name and it sticks around.

**New quick patch** is the fastest way to get going: pick a source, pick a destination, and you have a patch with one connection in it. **New empty patch** gives you a blank canvas to build on instead.

## Connection points

Each endpoint on the canvas has two columns.

- **In** is what Patchbay sends *to* that device.
- **Out** is what Patchbay receives *from* it.

There's a row for each group the endpoint declares, labeled with the group's number and the name the device gives that group. The name comes from the device's function blocks, or from its group terminal blocks when no function block names the group. Each endpoint grows wide enough to show its longest name. If a name is longer still, point at the row to see all of it. Above them is an **All groups** row: connect that and everything passes through with its group untouched, which is usually what you want when you just mean "send this device to that one".

Connect a specific group to a different specific group and Patchbay rewrites the group as the message goes past. That's how you fold four groups of one device onto one group of another.

You can draw a connection by dragging from an Out point to an In point, or by clicking the Out point and then clicking the In point. The second way also works from the keyboard. You don't have to land exactly on the point &mdash; get close and the connection snaps to it.

To change where an existing connection goes, drag the end of the cord onto a different point. Drag a node by its title bar to move it out of the way. Select a connection or an endpoint and press **Delete** to remove it; the first time, Patchbay asks, and offers to stop asking.

The zoom controls above the canvas go from 10% to 400%. Click the zoom percentage and choose **Fit to screen** to see everything on the patch at once, as large as the window allows. The overview in the corner of the canvas shows the whole patch, with a box around the part on screen. Drag the box to move around, or click anywhere in the overview to jump there.

## Filters

Each connection can be narrowed so only some of what arrives is passed on. Select the connection and choose **Edit filters**.

![Choosing which messages a connection carries]({{ site.baseurl }}/assets/images/midipatchbay-filters.png)

Everything is allowed until you clear something. A message has to pass every section to be sent on, so the sections work together: clearing a whole message type drops those messages whatever the sections below say.

- **Message types** is the coarsest switch, at the UMP message type level.
- **Channel messages** applies to both MIDI 1.0 and MIDI 2.0 channel voice messages, so you can drop program changes or keep only notes.
- **System messages** is where you stop a device flooding everything downstream with timing clock or active sensing.
- **Channels** limits which of the sixteen channels get through.

A **note range** keeps only notes inside a span, which is how you split a keyboard across two instruments. Click a key for the bottom of the range and shift-click for the top, or type the note numbers. It applies to note on, note off, poly pressure and the MIDI 2.0 per note messages, so a held note can't be stranded.

![Setting a note range on the keyboard]({{ site.baseurl }}/assets/images/midipatchbay-note-range.png)

## Transforms

Transforms change messages on the way past. They run after the filters, on the copy sent to that one destination, so nothing here affects what any other connection carries. Between them they do everything the old Windows MIDI Mapper did, and quite a lot it couldn't.

![Transposing down an octave, with note mapping below it]({{ site.baseurl }}/assets/images/midipatchbay-transforms.png)

- **Channel mapping** moves everything on one channel to another. Only messages that carry a channel are affected, and channels you don't list are passed through.
- **Transpose** shifts every note, including aftertouch and the MIDI 2.0 per note messages. A note pushed past either end is clamped rather than wrapped, so nothing lands an octave out.
- **Note mapping** is the list of exceptions to the transpose: a note listed here goes exactly where you send it, and everything else is transposed as usual. **Play** sends the destination note so you can hear where it lands.
- **Note on velocity** reshapes the ramp, linear to curved or curved to linear. It can rescale into a narrower range so a light touch still speaks and a heavy one doesn't max out, or send every note at the same fixed velocity. Only note on messages are touched, and a MIDI 1.0 note on with velocity zero is a note off, so it's always left alone.
- **Aftertouch** reshapes channel pressure and poly pressure. Input low is how hard you have to press before anything is sent, input high is how hard counts as full, and the output range sets what comes out. Slow rise makes full pressure harder to reach, and fast rise makes it easier. A pressure of zero means you let go of the key, so zero always goes out as zero and no sound is left bent.
- **Control change mapping** moves a controller to a different number. Its value only changes if you add a rule for it under control change values. Controllers you don't list are passed through.
- **Control change values** changes what a controller's value does on its way out. See [Shaping controller values](#shaping-controller-values) below.
- **Program mapping** picks a different sound on the destination, for an instrument whose programs aren't laid out the way the music expects. The numbers are the ones on the wire, 0 to 127, with the General MIDI name beside each one.
- **Bank select** remaps the bank, as controller 0 and controller 32 on MIDI 1.0 and as the bank a MIDI 2.0 program change carries. Most instruments only use the MSB half.

![Moving one controller to another]({{ site.baseurl }}/assets/images/midipatchbay-control-change.png)

### Shaping controller values

Each rule under **Control change values** names one controller and says what happens to its value. A small graph beside the rule shows the result: the dashed line is the value going in, and the solid line is what comes out. Some everyday fixes:

- **A sustain pedal that works backwards.** Add a rule for CC 64 and check **Invert**.
- **An expression pedal that never reaches the ends.** If it only goes from 10 to 117, set input low to 10 and input high to 117. It then covers the whole range.
- **A mod wheel that's too strong.** Set output high to 60, and the top of the wheel sends 60.
- **A volume pedal that jumps too fast near the bottom.** Choose **Slow rise**, which gives you finer control at the quiet end. **Fast rise** does the opposite.

Invert happens before the curve, so a backwards pedal is put the right way round first and then shaped like any other pedal. If you type the ends of a range high to low, that range runs backwards, so the numbers always mean what they say.

A rule applies to the controller number as it leaves, after the control change mapping. So if CC 1 is moved to CC 11, the rule for CC 11 is the one that shapes it. MIDI 2.0 controllers are shaped at their full resolution.

Rules work on one message at a time. Some controllers split a value across two controller numbers for extra detail, such as CC 1 with CC 33. Those are best left without a rule, because each half would be shaped on its own.

### Showing values as 0 to 127 or as a percentage

MIDI 2.0 carries velocity in sixteen bits and controller values in thirty-two, so a percentage is what those values really mean, and that's how Patchbay stores them. But plenty of controllers give each of the 128 MIDI 1.0 steps its own meaning, such as a pad color, and typing 0.79% when you mean step 1 is no fun.

So the transforms dialog starts with a choice: **0 to 127** or **Percentage**. Velocity, aftertouch and controller values all follow it. The value is the same either way; only the way you type it changes. A patch saved before this choice existed opens as 0 to 127, because that's all it could have meant. A new one starts as a percentage.

### MIDI 2.0 notes that carry an exact pitch

A MIDI 2.0 note on can say exactly which pitch to play, down to a fraction of a semitone. That makes its note number an address rather than a pitch, and moving one without moving the other would produce a message asking for one note and the pitch of another.

Leave **Bypass exact-pitch notes** clear and the pitch moves with the note, so transposing and note mapping both stay honest. Check it and those notes are passed through untouched, which is what you want when the note number means a drum pad or a key on a controller rather than a pitch.

Selecting a connection shows what its filters and transforms add up to, so you can see at a glance what a cord is doing without opening either dialog.

![What a connection carries]({{ site.baseurl }}/assets/images/midipatchbay-connection.png)

## Routing only runs while Patchbay is running

This is the important limitation. Patchbay routes by receiving messages in its own process and sending them back out, so the routes exist only while the app is open. Two settings in the appearance and settings flyout deal with that:

- **Start with Windows** launches Patchbay when you sign in.
- **Run in notification area** means closing or minimizing the window puts Patchbay in the notification area with the routes still up. Click the icon to bring the window back, or right-click it for the list of patches, to stop everything, or to exit properly.

Both are off unless you turn them on. Patchbay doesn't put itself in the notification area uninvited.

## Loopbacks

Because Patchbay works inside its own process, it can only route *from* a message source *to* a message destination. It can't reach inside another app. A **loopback** is how you bridge that gap: it's a real MIDI endpoint on the PC that any app can open, so your DAW connects to the loopback and Patchbay feeds the loopback.

**Create loopback** on the toolbar makes one without leaving the canvas, and drops it straight onto the patch. It uses the same Windows MIDI Services feature that [MIDI Loopback Setup]({{ site.baseurl }}/tools/midiloopbacksetup/) does, so a loopback made here is a normal endpoint that every app sees, and it can stick around after Patchbay closes.

> **Tip:** Some apps remember a device by its name. To filter what one of those apps receives from a device without breaking that, give a loopback the device's name. First rename the device with **Customize** in [MIDI Settings]({{ site.baseurl }}/tools/settings/). Older apps see the device's MIDI 1.0 ports rather than the device itself, so rename those too, with **Edit port names**. New port names take effect when the MIDI service restarts. Then create a **MIDI 1.0 basic loopback** with the device's original name, connect the device's **Out** to the loopback's **In**, and set the filters you want on that connection. The app finds the loopback under the name it remembers, and gets only what your filters let through.
>
> This works best for a device the app only listens to. A basic loopback sends whatever it's given straight back out, so anything the app sends to that name comes back to the app instead of reaching the device.

## When a device isn't there

Gear gets unplugged. That's a normal state, not an error.

An endpoint that isn't connected right now is drawn with a dashed outline and a warning badge, its patch gets a caution triangle in the list, and the connections to it sit idle. Everything else in the patch keeps routing. The moment the device comes back, its connections start carrying messages again without you doing anything.

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

## Testing a route

Right-click an endpoint, or use **Test** on the toolbar, to open [MIDI Monitor]({{ site.baseurl }}/tools/midi2monitor/), the MIDI keyboard or the [scratch pad]({{ site.baseurl }}/tools/midiscratchpad/) already pointed at that endpoint. Selecting a connection shows a running count of what it has forwarded, which is how you tell "nothing is arriving" apart from "arriving and going nowhere".
