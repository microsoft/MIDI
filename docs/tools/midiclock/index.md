---
layout: tools_page
title: MIDI Clock
tool: midiclock
description: Send MIDI beat clock to your instruments at a tempo you choose
icon: /assets/images/midiclock.png
categories:
  - General Purpose MIDI Tools
---

> This page covers information about a Windows MIDI Services feature and application that will be released to consumers in November 2026. It's currently available for developers.

MIDI Clock sends MIDI beat clock to your instruments so their arpeggiators, sequencers, delays, and drum machines run at a tempo you set, and run together.

It's the tool for a rig where nothing is the master. A drum machine, a groovebox, and a delay pedal all want a clock, and without one they each drift at their own rate. Start a clock here and they lock to it.

![The MIDI Clock main window]({{ site.baseurl }}/assets/images/midiclock.png)

## Getting started

1. Select **Add clock**.
2. Give it a tempo, choose the endpoint to send to, and pick a group.
3. Select **Save**, then **Start** on the tile.

Each clock is a tile, with its tempo as the headline number. The tile's border picks up your accent color while it's running.

Clocks are saved as soon as you make them, and they come back the next time you open the app. They're saved **for everyone who uses this PC**, not per account, so a clock you set up is there for every user. You can have up to 128 of them.

## Setting up a clock

![The clock settings dialog]({{ site.baseurl }}/assets/images/midiclock-editor.png)

| Field | What it's for |
|---|---|
| **Name** | Optional. The endpoint's name is used when you leave it empty, which is usually enough |
| **Tempo** | 20 to 300 beats per minute. Type it, drag the slider, or tap it in |
| **Send to endpoint** | Which device gets the clock |
| **Group** | One group, or every group the endpoint declares |
| **Send Start and Stop messages** | Whether to send Start when the clock starts and Stop when it stops |

**Tap tempo** is for matching something you're listening to. Tap along with the beat and the tempo follows, rounded to the nearest half a beat per minute so you don't end up with 148.2628. Type a number directly if you need an exact one from a DAW.

Stop tapping and start again at a different tempo and it works out that you've started over, rather than averaging the old taps into the new ones. While it's working that out it holds the number it had rather than showing you a half-measured one.

**Send Start and Stop messages** is the setting to think about. With it on, a device that's waiting for a Start will begin its pattern when you start the clock, and go back to the top when you stop. With it off, only the timing clock goes out, and a device follows the tempo without being told when the bar begins. Some gear wants the first, some gets confused by it, so it's per clock.

**All groups this endpoint declares** is for a MIDI 2.0 device where you don't know, or don't care, which group the sequencer is listening on. A MIDI 1.0 device has one group and the choice doesn't arise.

## Running more than one

The buttons along the bottom are where the app earns its place.

- **Start all** starts every clock.
- **Start selected** starts the ones you've checked on their tiles.
- **Stop all** stops everything.

**Clocks started together begin on the same instant**, even at different tempos and on different devices. That's the point of starting them as a group rather than pressing Start on each tile in turn: pressing them one at a time gets you clocks that are a few hundred milliseconds apart, which is audible.

A clock whose device isn't connected says so on its tile and is left out rather than holding up the others.

## What to expect from it

The settings panel carries a note about this, and it's worth reading before you rely on the clock for anything critical.

![The settings panel with the performance note]({{ site.baseurl }}/assets/images/midiclock-settings.png)

The accuracy of the clock this app generates is limited by your MIDI device, by its connection to the PC, and by any timing offset you've set in [MIDI Settings]({{ site.baseurl }}/tools/settings/). MIDI devices vary a lot in latency and jitter. Most are good; some are five, ten, or more milliseconds out.

**The clock is not synchronized to any audio stream.** If you need MIDI clock locked to audio you're recording or playing back, that's a job for your DAW and a plugin built for it, not for this app.

**Appearance** in the same panel sets whether the app follows your Windows light or dark mode, and which window background to use. The pin button next to the settings button keeps the window above your other windows.

## Starting from the command line

```
midiclock.exe [endpoint device id] [options]

  [endpoint device id]   Optional. The full endpoint device id to send to.

  --start                Optional. Start the clock for the endpoint given, or
                         every saved clock when no endpoint is given.

  --group, -g    number  Optional. The group number, 1 through 16. Only valid
                         when an endpoint device id is supplied.

  --bpm, --tempo number  Optional. The tempo in beats per minute. Only valid
                         when an endpoint device id is supplied.
```

Giving an endpoint that already has a clock updates that clock; giving one that doesn't creates it. That makes `midiclock.exe <id> --bpm 128 --start` a one-line way to get a rig running from a shortcut or a script.

You can find the endpoint device id for a device using the [MIDI Console]({{ site.baseurl }}/tools/console/) or the [MIDI Settings app]({{ site.baseurl }}/tools/settings/).

## Learn more

- [MIDI Monitor]({{ site.baseurl }}/tools/midi2monitor/) to confirm the clock is arriving. Timing clock messages are hidden there by default, so switch them back on with the button at the bottom left
- [MIDI Settings]({{ site.baseurl }}/tools/settings/) for the per-device timing offsets mentioned above
