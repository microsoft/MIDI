---
layout: tools_page
title: Bluetooth MIDI Setup
tool: midibluetoothsetup
description: Connect this PC to Bluetooth LE MIDI devices, and let other devices connect to this PC
---

> Bluetooth MIDI is a preview feature for Windows MIDI Services. It's installed separately from the
> service, API, and tools, and it is not yet part of a consumer release.

Bluetooth MIDI Setup connects this PC to wireless MIDI devices: keyboards, controllers, and
instruments which speak Bluetooth LE MIDI. Once connected, the device appears in Windows like any
other MIDI device, so your DAW and other MIDI software can use it straight away.

It also works the other way around. This PC can publish itself so a phone, a tablet, or another
computer can connect to it.

![The Bluetooth MIDI Setup main window]({{ site.baseurl }}/assets/images/midibluetoothsetup.png)

There are three pages:

- **Bluetooth devices** is for connecting *this PC to something else*. This PC is the "central" and
  the device is the "peripheral", which is how nearly all Bluetooth MIDI gear expects to work.
- **This PC** is for letting *something else connect to this PC*. Here this PC is the peripheral. For example, you may want to use a tablet PC as a wireless controller.
- **Transport settings** holds the defaults which apply to every Bluetooth MIDI device.

## Finding devices

Devices appear on the **Bluetooth devices** page on their own. There is nothing to scan or search.

Bluetooth MIDI devices are found by listening for the advertisements they broadcast, and that has
consequences worth knowing about, because it explains most of the times a device you own is not in
the list:

- A device which is **switched off or asleep** is not advertising, so it cannot be found. Many
  battery-powered controllers sleep aggressively to save power.
- A device which is **already connected to something else**, such as a phone or an iPad, stops
  advertising. Bluetooth LE MIDI peripherals talk to one central at a time, so you have to
  disconnect it there first.
- Some devices only advertise for a short window after being switched on, and then go quiet.
- Some devices requiring pairing in Windows Settings before they become available.

Each device shows its name and what Windows currently knows about it:

- **Connected** - the device is connected now and its MIDI endpoint exists.
- **Last heard from *n* minutes ago** - the device has been seen, but is not connected.
- **Paired, but not heard from since the service started** - Windows remembers it from a previous session, but it has not advertised since the MIDI service started.

## Connecting

**Connect** connects to the device and creates its MIDI endpoint. Windows also remembers the device,
so when it comes back it is reconnected for you without you having to open this app again.

**Disconnect** drops the connection but keeps remembering the device.

**Forget** removes it from the remembered list entirely, so Windows stops trying to reconnect. Use
this for a device you have sold or no longer use.

In the majority of cases, you do not necessarily have to pair a Bluetooth MIDI device in Windows Settings first. If the device is advertising, this app can usually connect to it directly.

## Device details

**Details** on any device opens what Windows knows about it.

![The details for a connected device]({{ site.baseurl }}/assets/images/midibluetoothsetup-details.png)

- the Bluetooth address, and which Bluetooth MIDI protocol the connection is using
- the endpoint device ID, with a button to copy it
- the connection interval which was negotiated, which is the main thing determining latency and how
  often messages can be delivered
- how many messages and how many Bluetooth packets have gone in and out since the connection was
  made, which is the quickest way to tell whether a device is actually sending anything
- **Rename** gives the endpoint a name of your own, the same customization the MIDI Settings app offers

## Keeping an endpoint when a device goes offline

**Keep endpoint when offline** is the setting most worth understanding, and it is here because
Bluetooth LE devices come and go constantly. They sleep, they go out of range, and their batteries die.

The question it answers is what should happen to the MIDI endpoint when that happens:

- **Always** keeps the endpoint. Applications hold on to their MIDI ports, and when the device comes
  back it simply starts working again. Nothing has to be reopened or reselected, but MIDI data may be lost in the interim period.
- **Remove immediately** removes the endpoint as soon as the device goes offline, so its MIDI ports
  disappear.
- **After 30 seconds** or **After 5 minutes** waits, then removes it. This rides out a brief dropout
  while still removing an endpoint that is genuinely gone.
- **Use default** defers to the transport setting.

No one answer is right for everyone, which is why it is a setting. Keeping the endpoint is more
convenient, especially for live performance. Removing it matters because applications written against WinMM or WinRT MIDI 1.0 have no way to ask Windows whether a device is present: the port disappearing is the only signal they get
that the device is gone. If those applications matter to you, a device which vanishes silently while
its port stays open is worse than one whose port goes away.

You can set this per device, or set the default for every device on the **Transport settings** page.

![The transport settings page]({{ site.baseurl }}/assets/images/midibluetoothsetup-settings.png)

## Letting other devices connect to this PC

On the **This PC** page, **Publish** advertises this PC as a Bluetooth MIDI peripheral so a phone,
tablet, or another computer can connect to it.

![Publishing this PC so other devices can connect to it]({{ site.baseurl }}/assets/images/midibluetoothsetup-this-pc.png)

The name a remote device sees is this PC's Bluetooth name. That is a Windows setting rather than a
MIDI one, so it is changed in Windows Settings, not here.

The MIDI endpoint on this side represents *the device which connected*, so it exists only while
something is connected. Until then the page says nothing is connected, and that is normal.

**Protocol** chooses which Bluetooth MIDI protocol to advertise. Only one is published at a time:

- **Bluetooth LE MIDI 1.0** is what to use. Every Bluetooth MIDI device and app in the world speaks it.
- **Bluetooth LE MIDI 2.0** implements the draft standard for testing and development.
  Almost nothing supports it yet, and while it is selected a MIDI 1.0 device cannot connect. This feature is not yet available in Windows MIDI Services, but will be after the specification is finalized.

## Troubleshooting

### Common reasons a device may not connect

**It is asleep or not advertising.** This is by far the most common cause. Switch the device off and
on again, and look at the list within the following few seconds. Many battery-powered controllers
sleep aggressively to save power.

**It is connected to something else.** A tablet or phone sitting on the same desk will happily hold
on to the device and keep it off the air. Bluetooth LE MIDI peripherals talk to one central at a
time, so you have to disconnect it there first.

**It needs to be paired.** Most Bluetooth MIDI devices can be connected to without pairing, but some
require it. Windows will usually prompt you when that is the case, so watch for a notification after
pressing **Connect**.

**Its batteries are low.** A device low on power may advertise but fail to hold a connection.

**Windows is remembering something stale.** If a device previously connected and now will not, use
**Forget**, then connect again.

If Bluetooth MIDI is not installed or not enabled on this PC, the app tells you so when it starts,
and the rest of the window stays empty. Nothing here works until that is sorted out.

### Connects, but does not transmit data

Sometimes a device connects, shows as **Connected**, and then sends nothing at all. Open **Details**
and look at the message and packet counts, which tell you which half of the problem you have:

- **Both are zero.** Nothing is arriving from the device. Work through the causes below.
- **Packets are increasing but messages are not.** The device is transmitting, but this transport
  cannot decode what it is sending. That points to a problem on our side rather than anything you
  can fix, so please report it along with the make and model of the device.

**It may still be paired with another device.** This is the most common cause, and it happens even
though the connection itself succeeds: a device which is still paired with a phone or tablet will
accept a connection from this PC and then quietly send nothing. Unpair it from the other device,
remove the pairing from this PC, and then pair it with this PC again.

**It may be in use by the older Bluetooth MIDI support in Windows.** Windows still includes its
original WinRT MIDI 1.0 Bluetooth MIDI stack, and for a device which is *paired* with this PC, that
stack can claim the device first. In Device Manager, under **Software devices**, look for the
**MIDI Bluetooth In** and **MIDI Bluetooth Out** entries for the device, and uninstall them.

> Claiming by the older stack applies only during the preview period, while the previous Bluetooth
> MIDI support is still enabled. It will not be a concern when Bluetooth MIDI ships in a regular
> Windows release.
