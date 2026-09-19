---
layout: tools_page
title: MIDI Patchbay
tool: midipatchbay
description: Route MIDI between endpoints on a canvas you draw yourself
icon: /assets/images/midipatchbay.png
categories:
  - General Purpose MIDI Tools
---

> This page covers information about a Windows MIDI Services feature and application that will be released to consumers in November 2026. It's currently available for developers.

MIDI Patchbay connects MIDI endpoints to each other. You drop the devices you care about onto a canvas, draw connections from one device's **Out** to another device's **In**, and from then on everything arriving on that connection point is passed along.

It's the software version of the patchbay in a studio rack: keys into a sound module, a drum machine into your DAW, one controller split across three instruments.

## What a patch is

A **patch** is one canvas: the endpoints on it, the connections between them, and a name. Patches are saved as files in **Documents &rsaquo; MIDI Patchbay**, one file per patch, so you can back one up or copy it to another PC.

You can have as many patches as you like, and more than one can be routing at the same time. Whether a patch is routing is separate from whether it's the one on screen, so you can look at one patch while three others are working.

A patch you don't name is **temporary**: it routes right now and disappears when Patchbay closes. Nothing is written to disk. Give it a name and it sticks around.

## Connection points

Each endpoint on the canvas has two columns.

- **In** is what Patchbay sends *to* that device.
- **Out** is what Patchbay receives *from* it.

There's a row for each group the endpoint declares, labeled with the same name that group has as a MIDI 1.0 port, so it matches what you see in every other app. Above them is an **All groups** row: connect that and everything passes through with its group untouched, which is usually what you want when you just mean "send this device to that one".

Connect a specific group to a different specific group and Patchbay rewrites the group as the message goes past. That's how you fold four groups of one device onto one group of another.

You can draw a connection by dragging from an Out point to an In point, or by clicking the Out point and then clicking the In point. The second way also works from the keyboard.

## Routing only runs while Patchbay is running

This is the important limitation. Patchbay routes by receiving messages in its own process and sending them back out, so the routes exist only while the app is open. Two settings in the appearance and settings flyout deal with that:

- **Start when I sign in** launches Patchbay with Windows.
- **Keep running in the notification area** means closing the window leaves the routes up.

Both are off unless you turn them on. Patchbay doesn't put itself in the notification area uninvited.

## Loopbacks

Because Patchbay works inside its own process, it can only route *from* a message source *to* a message destination. It can't reach inside another app. A **loopback** is how you bridge that gap: it's a real MIDI endpoint on the PC that any app can open, so your DAW connects to the loopback and Patchbay feeds the loopback.

**Create loopback** on the toolbar makes one without leaving the canvas, and drops it straight onto the patch. It uses the same Windows MIDI Services feature that [MIDI Loopback Setup]({{ site.baseurl }}/tools/midiloopbacksetup/) does, so a loopback made here is a normal endpoint that every app sees, and it can stick around after Patchbay closes.

## When a device isn't there

Gear gets unplugged. That's a normal state, not an error.

An endpoint that isn't connected right now is drawn with a dashed outline and a warning badge, its patch gets a caution triangle in the list, and the connections to it sit idle. Everything else in the patch keeps routing. The moment the device comes back, its connections start carrying messages again without you doing anything.

If the device came back with a different identity &mdash; a USB device with no serial number moved to another port, for example &mdash; Patchbay notices a likely match and offers it. It never binds to a different device on its own.

Selecting an endpoint shows **Identify this device by** in the details panel:

- **Its exact device ID** is the default, and the safest.
- **Manufacturer, VID and PID** matches the same model in any port.
- **Its name** is a last resort, because two identical devices look the same.

## Loops

Sending a device's output back to its own input, directly or the long way around, floods every device on the path within a second. Patchbay walks the patch after every change and looks for that.

When the circle is certain &mdash; when every endpoint on it is a loopback, so it's known that what goes in comes back out &mdash; the connection that closes it is left in place but held muted, and the whole circle is drawn in red. The message names the hops in order so you can see which one to remove.

When the circle only closes *if* a piece of hardware echoes what it receives, which Patchbay can't see from outside, it says so and mutes nothing.

Patchbay only knows about the connections it routes itself. A DIN cable between two devices, or another routing app, can close a circle it can't see. That's why the status bar says "no loops detected" rather than "no loops".

## Testing a route

Right-click an endpoint, or use **Test** on the toolbar, to open [MIDI Monitor]({{ site.baseurl }}/tools/midi2monitor/), the MIDI keyboard or the [scratch pad]({{ site.baseurl }}/tools/midiscratchpad/) already pointed at that endpoint. Selecting a connection shows a running count of what it has forwarded, which is how you tell "nothing is arriving" apart from "arriving and going nowhere".

## Coming next

Filters and transforms on each connection: only certain channels, only certain message types, only notes in a range, and dropping clock and other system real-time messages so a chatty device stops flooding everything downstream. The details panel already shows where they'll live.
