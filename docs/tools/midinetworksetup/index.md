---
layout: tools_page
title: Network MIDI 2.0 Setup
tool: midinetworksetup
description: Connect this PC to Network MIDI 2.0 devices over your local network
---

> This page covers information about a Windows MIDI Services feature and application that will be released to consumers in November 2026. It's currently available for developers.

Network MIDI 2.0 Setup connects this PC to MIDI devices over your local network, with no traditional MIDI cables
between them. If you have an interface, a synth, or another computer that speaks Network MIDI 2.0 over Ethernet or WiFi,
this is where you set up the connection.

Once a connection is made, the device appears in Windows like any other MIDI device, so your DAW
and other MIDI software can use it straight away.

![The Network MIDI 2.0 Setup main window]({{ site.baseurl }}/assets/images/midinetworksetup.png)

There are two pages, each with a different but related function:

- **Network devices** is for connecting *this PC to something else*, such as an interface or a synth. The remote device is a "host", and this PC is a "client". The connection is initiated from this PC.
- **This PC** is for letting *other devices connect to this PC*, such as a laptop or a phone or external device that
  wants to communicate over MIDI. In this case, this PC is the "host" and the remote device is the "client". The connection is initiated by the remote device.

## Finding and connecting to devices

Devices that advertise themselves on your network appear on the **Network devices** page on their
own. There's nothing to scan or search: as long as the device is switched on and on the same
network (and same subnet), it shows up within a few seconds.

Each device shows its name, where it is on the network, and whether you're connected to it.

To connect, select **Connect**. You'll be asked what to call the MIDI Endpoint for this device in Windows.

![Naming a device as you connect to it]({{ site.baseurl }}/assets/images/midinetworksetup-connect-name.png)

Leave the box empty and Windows uses whatever name the device reports for itself. Fill it in and
that name is what you'll see everywhere in Windows, including in your DAW's device list. This is
worth doing if you have several similar units, or if the device's own name is cryptic.

### The other device may be waiting for you too

This is the step that catches people out. Connecting is a conversation between two devices, and
plenty of devices will not simply accept an incoming connection. On the other end, something may
be waiting for you to approve it:

- a web page served by the device, where you allow the connection
- a companion app or a setting on a computer or phone
- a physical button on the unit to confirm
- a pairing or permission prompt of some other kind

If the connection sits at **Connecting** and doesn't complete, that's the first thing to check.
Go and look at the device, its front panel, or its configuration page, and see whether it's asking
you a question. Windows will keep trying, so once you allow it on the other side the connection
completes on its own.

Devices that let anything connect will simply connect straight away.

### Watching a connection

Once connected, each device shows a graph of round trip time, which is how long a message takes
to get to the device and back.

The graph scrolls from right to left, with the newest measurement on the right, and covers roughly
the last few minutes. Underneath it you'll see the current round trip, how many network packets
have gone each way, and how many had to be resent.

The vertical axis is logarithmic and re-scales itself to whatever the largest value in view is,
which is noted above the graph. That sounds fussy, but it's what lets you see a steady
sub-millisecond connection and an occasional large spike in the same picture.

A flat, low trace is what a healthy wired connection looks like. Regular tall spikes usually mean
the device is on Wi-Fi and its radio is going to sleep between messages; the first message after a
quiet moment then has to wait for it to wake up. That's normal for Wi-Fi, but it's the reason a
wired connection is worth having for anything timing-critical.

For MIDI, you normally want a situation where the round-trip latency is under 5 milliseconds. In general, the lower the better. Most wired networks will get you under a millisecond round trip latency.

> If all your connections show high round-trip network latency, it's worth looking at your network usage, and if you are saturating the same network with other audio or video. Most wired networks have plenty of bandwidth though, so the most common cause of high latency is using a WiFi connection instead of Wired. You may also find you have gaming-focused network accelerator software running, which may actually cause worse performance. 

Select **Disconnect** to end a connection. The device stays in the list and can be reconnected
whenever you like. Disconnecting doesn't block anything.

### Device details

Select **Details** under a device for the identifying information.

![Device details]({{ site.baseurl }}/assets/images/midinetworksetup-details.png)

**Product Instance Id** is the device's own serial-number-like identity. **Addresses** is where it
is on the network. **MIDI Endpoint** is the Windows device id, which is what other MIDI tools and DAWs want
when they ask you to identify a device. The small copy button on the right puts it on the
clipboard.

### Connecting to a device that doesn't advertise itself

Some devices don't announce themselves on the network, or are on a part of the network where those
announcements don't reach. You can still connect by typing the address in.

![Connecting to a device by address]({{ site.baseurl }}/assets/images/midinetworksetup-by-address.png)

Enter the host name or IP address and the port, and optionally the two names: **Name that device
will see** is how this PC introduces itself, and **Name to show in Windows** is what you'll call
the device here.

Windows keeps the entry and keeps trying, so if the device isn't switched on yet it will connect
later when it appears.

> Connecting to devices by using mDNS / advertising is more efficient than by IP. When you connect by IP address or name, Windows must keep checking for a response at that address. But if the device advertizes using mDNS, we can simply wait until the ad appears on the network.

## Letting other devices connect to this PC

The **This PC** page is the other direction: making this computer something other devices can
connect *to*. To do that, this PC needs a host.

![The This PC page]({{ site.baseurl }}/assets/images/midinetworksetup-this-pc.png)

Most people need only one host, and it stays there once created.

### Creating a host

Select **Create host**.

![Creating a host]({{ site.baseurl }}/assets/images/midinetworksetup-create-host.png)

**Name other devices will see** is how this PC appears in the device list on your synth, laptop,
or phone. The **Network service name** is the technical name used to announce this PC on the
network, and it has to be different from every other host on this PC.

**When a device asks to connect** is the one worth thinking about:

- **Ask me first** means nothing connects until you say so. A prompt appears at the top of the
  window when a device asks, and you decide.
- **Let any device connect** means anything on your network that finds this PC can connect
  without asking.

On a home studio network, letting any device connect is convenient. Anywhere you don't control
who's on the network, ask first.

> **There is no password option in this release.** The Network MIDI 2.0 specification defines
> optional password and user authentication, and Windows MIDI Services does not support either
> yet. **Ask me first** is how you control who gets in. See
> [How Network MIDI 2.0 works in Windows]({{ site.baseurl }}/kb/network-midi2-transport/) for what
> that means in practice.

**Advertise this host on the network** is what makes this PC appear in other devices' lists.
Switch it off and devices can still connect, but they'll need the address typed in by hand.

**Also create MIDI 1.0 ports for connected devices** makes connected devices usable from older
software that doesn't understand the new combined MIDI 1.0/MIDI 2.0 API. Most apps today will fall under that category.

### When a device asks to connect

If you chose **Ask me first**, a prompt appears at the top of the window whenever a device wants to
connect. It shows the device's name and where it's connecting from, and it's visible on both pages
so you won't miss it.

You have four answers:

- **Allow once** connects this device now, and asks again next time.
- **Always allow** connects it now and remembers, so it connects on its own in future.
- **Deny** refuses this attempt, but asks again next time.
- **Block** refuses and remembers, so the device is turned away without asking you again.

### Connected devices

Each host lists the devices currently connected to it, with the same round trip graph you get on
the Network devices page.

**Disconnect** ends the connection but doesn't stop the device reconnecting. **Block** ends it and
refuses that device in future.

**Remembered decisions** appears when you've used **Always allow** or **Block**, and lets you undo
those choices. If a device is being turned away and you can't work out why, look here first.

**Stop** takes a host off the network without deleting it, and **Delete** removes it entirely.

## Settings

The gear button in the title bar opens the settings.

![The settings panel]({{ site.baseurl }}/assets/images/midinetworksetup-settings.png)

**Theme** and **Window background** control how the app looks. Mica and Acrylic pick up colors
from your desktop; Acrylic lets what's behind the window show through. Tick **Use a custom
background color** to choose your own.

**Refresh connection details every (seconds)** sets how often the app asks the MIDI service for
connection state, round trip times, and packet counts. Three to five seconds suits most people. A shorter
interval gives a more detailed graph at the cost of asking the service more often. The polling only happens while this app is running.

### Transport settings

Further down the settings panel are the transport settings. These are different from everything
above: they belong to the MIDI service, not to this app, and they apply to **every** Network MIDI
2.0 host and client on this PC. They also persist once changed, whether or not this app is
running.

The defaults suit almost every network. Change them only if you have a reason to.

| Setting | What it does | When it takes effect |
|---|---|---|
| **Most devices allowed at once** | How many remote devices any one host on this PC will accept at the same time | Right away. Devices already connected are not disconnected |
| **How long to wait for your permission** | How long a device asking to connect stays in the waiting list before it is dropped | The next device which asks. Devices already waiting keep the old timeout |
| **How often to retry a device which is not answering** | How often this PC retries a device you connected to by address after it stops answering | Within one retry, so up to the old interval from now |
| **How often to check a quiet connection** | How often a connection with nothing to send checks the other end is still there. Shorter notices a dropped device sooner and sends slightly more traffic | Reaches open connections within one interval |
| **Repeated messages per packet** | How many recently sent messages are repeated in each packet, so a lost packet can be recovered without asking again. Higher copes better with an unreliable network and makes each packet larger | New connections. Reconnect a device for it to apply there |
| **Messages kept for resending** | How many sent messages are held in case the other end asks for them again. Higher recovers from longer gaps and uses more memory per connection | New connections. Reconnect a device for it to apply there |

Each box shows the range it accepts. A value outside that range is corrected rather than
rejected, so if a number changes after you type it, that is why.

The two that matter most in practice are **How often to check a quiet connection**, if you want a
dropped device noticed sooner, and **Repeated messages per packet**, if you are on Wi-Fi or a
busy network and are losing messages.

For the exact defaults, ranges, and the configuration file keys behind these, see
[How Network MIDI 2.0 works in Windows]({{ site.baseurl }}/kb/network-midi2-transport/).

The pin button next to the minimize button keeps the window above your other windows, which is
handy while you're setting a device up.

## When something doesn't connect

A few things to check, roughly in order:

**Look at the other device.** As above, it may be waiting for you to allow the connection there.

**Check they're on the same network and same subnet.** Devices are found by announcing themselves locally, and
those announcements don't usually cross between separate networks, or between a guest network and
your main one. Usually the product manual for the other device will have information about how to ensure the subnet is the same, or how to set the IP address so it matches the network your PC is on.

**Try connecting by address.** If the device works when you type its address but never appears in
the list, the announcements aren't reaching this PC even though the network path is fine.

**Check whether it's blocked.** If you selected **Block** at some point, the device is refused
silently. Look under **Remembered decisions** on the host.

If Network MIDI 2.0 isn't installed or isn't enabled on this PC, the app tells you so when it
starts and the rest of the window stays empty. Nothing here will work until that's sorted out.
