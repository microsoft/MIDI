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

## The toolbar

Across the top are the other Windows MIDI Services apps. Only the ones actually installed on this PC
appear, so the toolbar reflects what you can really do:

- **Loopback** creates and manages loopback endpoints
- **Bluetooth** connects Bluetooth LE MIDI devices (installed with the Bluetooth MIDI transport)
- **Network** sets up Network MIDI 2.0 (installed with the Network MIDI 2.0 transport)
- **SysEx** sends and receives system exclusive files
- **Scratch Pad** sends hand-written MIDI messages
- **Troubleshoot** checks the health of MIDI on this PC and repairs common problems

On the right, **MIDI Settings** opens the machine-wide settings described below. It is not a separate
app; it opens in this window.

## Endpoints

The main area lists every MIDI endpoint on the PC. Two views:

- **Cards** shows each endpoint as a tile with its picture. Good for picking a device by sight.
- **List** is denser and shows each endpoint's description as well.

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

- the name, description and picture
- the transport it arrived on, and its native data format
- the product instance ID and serial number
- the endpoint device ID, with a button to copy it
- every MIDI 1.0 input and output port belonging to this endpoint, kept live as ports appear and go away
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

The change is applied to the running service immediately and saved to the configuration file so it
survives a restart.

> Naming the individual MIDI 1.0 ports of a USB device is not in this release. Use the MIDI console for
> that in the meantime.

## Global MIDI settings

These are shared by everyone who uses the PC, so they need administrator rights. If the app is not
running elevated it says so and offers to restart itself with the rights it needs.

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
