---
layout: tools_page
title: MIDI Settings
tool: settings
description: See every MIDI endpoint on this PC, customize them, and launch the rest of the MIDI tools
---

> This page covers information about a Windows MIDI Services feature and application that will be released to consumers in November 2026. It's currently available for developers.

MIDI Settings is the front door to MIDI on this PC. It shows every MIDI endpoint Windows knows about,
lets you name them and give them a picture, and launches the rest of the MIDI tools.

Most of what you do day to day works as a standard user. Only the machine-wide settings behind the
**MIDI Settings** button on the toolbar need administrator rights, and the app tells you when that is
the case rather than demanding them up front.

![The MIDI Settings main window, showing endpoints as cards]({{ site.baseurl }}/assets/images/midisettings.png)

## The toolbar

Across the top are the other Windows MIDI Services apps. Only the ones actually installed on this PC
appear, so the toolbar reflects what you can really do:

- **Loopback Setup** creates and manages loopback endpoints
- **Bluetooth Setup** connects Bluetooth LE MIDI devices (installed with the Bluetooth MIDI transport)
- **Network Setup** sets up Network MIDI 2.0 (installed with the Network MIDI 2.0 transport)
- **SysEx Send/Receive** sends and receives system exclusive files
- **Monitor** watches the messages flowing through an endpoint
- **Scratch Pad** sends hand-written MIDI messages
- **Troubleshoot and Fix** checks the health of MIDI on this PC and repairs common problems

Last on the toolbar, **Global MIDI Settings** opens the machine-wide settings described below. It is
not a separate app; it opens in this window.

## Endpoints

The main area lists every MIDI endpoint on the PC. Two views:

- **Cards** shows each endpoint as a tile with its picture. Good for picking a device by sight.
- **List** is denser and shows each endpoint's description as well.

![The same endpoints in list view]({{ site.baseurl }}/assets/images/midisettings-list.png)

The drop-down on the left filters by transport, so you can look at just the Bluetooth devices, or just
the loopbacks. Your choice of view and filter is remembered.

Each endpoint has two buttons:

- **Monitor** opens MIDI Monitor on that endpoint. If MIDI Monitor is not installed, the MIDI console
  is used instead.
- **Panic** sends All Sound Off, Reset All Controllers and All Notes Off on every channel of every group
  the endpoint declares. This is the one to reach for when a note is stuck on.

If Windows MIDI Services has never been set up on this PC, a banner appears above the list inviting you
to create a configuration file. Without one, anything you customize is lost when the service restarts.

The list keeps itself up to date as devices come and go, and it recovers on its own if the MIDI service
is restarted underneath it.

## Endpoint details

Selecting an endpoint opens its details:

![The details for a single endpoint]({{ site.baseurl }}/assets/images/midisettings-details.png)

- the name, description and picture
- the transport it arrived on, and its native data format
- the product instance ID and serial number
- the endpoint device ID, with a button to copy it
- every MIDI 1.0 input/source and output/destination port belonging to this endpoint, kept live as
  ports appear and go away
- whether the device reports an identifier of its own

That last one matters more than it looks. A device that reports a serial number or a product instance ID
can be recognized wherever you plug it in, so the name and picture you give it follow it around. A device
that reports neither is matched by the port it is plugged into, and moving it may lose your customization.

Everything else Windows knows about an endpoint - function blocks, group terminal blocks, declared
protocol, stream configuration - is available from the MIDI console. It is too much detail for this app.

## Customizing an endpoint

**Customize** lets you set the name and description the endpoint shows everywhere in Windows, and attach
a picture to it. Pictures are copied into a shared folder, so every MIDI app shows the same artwork for
the same device.

![Giving an endpoint a name, description and picture]({{ site.baseurl }}/assets/images/midisettings-customize.png)

The change is applied to the running service immediately and saved to the configuration file so it
survives a restart.

## Naming the MIDI 1.0 ports of a device

A USB device attached through Kernel Streaming gets a MIDI 1.0 port for each of its groups, and those
are what older applications see through WinMM and WinRT MIDI 1.0. **Edit port names**, in the customize
dialog, is where you name them. It only appears for devices which have those ports.

![Naming the individual MIDI 1.0 ports of a device]({{ site.baseurl }}/assets/images/midisettings-port-names.png)

Inputs and outputs are listed separately, in group order. For each port you see the name it has now,
the two names Windows can generate for it, and a box for a name of your own.

**Name style for this device** overrides the machine-wide default for this one device:

- **Use the global default** follows the setting in Global MIDI settings, and tells you which style
  that currently resolves to
- **Compatible with older Windows** keeps the name older applications have always seen
- **New style, based on the endpoint and block names** is clearer, especially on a multi-port interface

Seeing both generated names side by side is the point of the grid: you can tell what switching style
would do to each port before you commit to it.

A name of your own overrides whichever style is selected, and is capped at 31 characters because that
is all WinMM stores. Leave the box empty to use the generated name.

> Renaming takes effect when the MIDI service restarts, and applications which remember ports by name
> will need to be pointed at them again. There is a button to restart the service in Global MIDI settings.

## Global MIDI settings

These are shared by everyone who uses the PC, so they need administrator rights. If the app is not
running elevated it says so and offers to restart itself with the rights it needs.

![The global MIDI settings, shared by everyone who uses the PC]({{ site.baseurl }}/assets/images/midisettings-global.png)

**Configuration file** - your endpoint names, pictures and transport setup are stored in a configuration
file under `%ALLUSERSPROFILE%\Microsoft\MIDI`. Only one is active at a time. You can switch between
files, create a new one, and save a copy somewhere else for backup or to move to another PC.

**Default MIDI 1.0 port naming** - how ports are named for older applications which use WinMM or WinRT
MIDI 1.0. *Compatible with older Windows* keeps the names those applications have always seen.
*New style* names ports from the endpoint and block names, which is clearer but will make applications
that remember port names by string ask you to pick the device again.

**MIDI service** - changing the configuration file or the port naming takes effect when the service
restarts, and there's a button here to do that.

## Appearance

The palette button in the title bar sets the theme, the window material and a custom window color, and
the pin next to it keeps the window on top. These are the same settings every Windows MIDI Services tool
has, and they are remembered per app.
