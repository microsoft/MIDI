---
layout: tools_page
title: Windows MIDI Settings
tool: settings
description: See every MIDI endpoint on this PC, customize them, and launch the rest of the MIDI tools
icon: /assets/images/midisettings.png
categories:
  - Core Tools
---

> This page covers information about a Windows MIDI Services feature and application that will be released to consumers in 2027. It's currently available for developers.

Windows MIDI Settings is the front door to MIDI on this PC. It shows every MIDI endpoint Windows knows about, lets you name them and give them a picture, and launches the rest of the MIDI tools.

Most of what you do day to day works as a standard user. Changing the machine-wide settings behind **Configurations** and **Global MIDI Settings** on the toolbar needs administrator rights. The app tells you when that is the case, rather than demanding them up front.

## Quick start

The main window lists every MIDI endpoint on this PC, with the other MIDI apps across the top.

![The Windows MIDI Settings main window, with numbered callouts on the app toolbar, the Configurations, Global MIDI Settings and Notifications buttons, the transport list, the card and list view buttons, an endpoint card, and its Monitor and Panic buttons]({{ site.baseurl }}/assets/images/midisettings-quick-start.png)

1. **The toolbar** opens the other Windows MIDI Services apps. Only the apps installed on this PC appear here.
2. **Configurations** switches, backs up, shares, and imports your whole MIDI setup. **Global MIDI Settings** holds the settings shared by everyone who uses this PC, so it needs administrator rights. **Notifications** chooses what Windows MIDI Services tells you about.
3. **The transport list**, set to **All transports** here, narrows the list to one kind of connection, such as Bluetooth devices or loopbacks.
4. **Card view** and **List view** switch between pictures and a denser list.
5. **Select an endpoint** to see its details. That's also where you give it a name, a description, and a picture of your own.
6. **Monitor** watches the messages the endpoint sends. **Panic** silences a note that's stuck on.

## The toolbar

Across the top are the other Windows MIDI Services apps. Only the ones actually installed on this PC appear, so the toolbar reflects what you can really do:

- **Loopback Setup** creates and manages loopback endpoints
- **Bluetooth Setup** connects Bluetooth LE MIDI devices
- **Network Setup** sets up Network MIDI 2.0
- **Windows MIDI Patchbay** connects endpoints to each other
- **SysEx Send/Receive** sends and receives system exclusive files
- **Monitor** watches the messages flowing through an endpoint
- **Scratch Pad** sends hand-written MIDI messages
- **Virtual Keyboard** plays notes from your mouse or computer keyboard
- **MIDI Clock** sends and watches MIDI timing
- **Windows MIDI Glass** makes touch control surfaces
- **Troubleshoot and Fix** checks the health of MIDI on this PC and repairs common problems

**Configurations**, **Global MIDI Settings**, and **Notifications** sit at the right end of the toolbar and stay there. Configurations opens a window of its own, described in [Configurations](#configurations) below. Global MIDI Settings opens the machine-wide settings, and Notifications chooses when Windows MIDI Services tells you something. None of the three is a separate app.

If the window is too narrow for all of the tools, the ones that do not fit move into a **...** button at the end of the row. Click it to reach them. Right-click anywhere on the toolbar and turn off **Show button labels** to get a compact row of icons instead, which fits many more tools in the same space. Point at an icon to see its name. Your choice is remembered.

## Endpoints

The main area lists every MIDI endpoint on the PC. Two views:

- **Cards** shows each endpoint as a tile with its picture. Good for picking a device by sight.
- **List** is denser and shows each endpoint's description as well.

![The same endpoints in list view]({{ site.baseurl }}/assets/images/midisettings-list.png)

The drop-down on the left filters by transport, so you can look at just the Bluetooth devices, or just the loopbacks. Your choice of view and filter is remembered.

Each endpoint has two buttons:

- **Monitor** opens MIDI Monitor on that endpoint. If MIDI Monitor is not installed, the MIDI console is used instead.
- **Panic** sends All Sound Off, Reset All Controllers and All Notes Off on every channel of every group the endpoint declares. This is the one to reach for when a note is stuck on.

If Windows MIDI Services has never been set up on this PC, a banner appears above the list. Without a configuration, anything you customize is lost when the service restarts. Select **Open Configurations**, make one with **New configuration**, and then select **Use this one** and restart the MIDI service.

The list keeps itself up to date as devices come and go, and it recovers on its own if the MIDI service is restarted underneath it.

## Endpoint details

Selecting an endpoint opens its details:

![The details for a single endpoint]({{ site.baseurl }}/assets/images/midisettings-details.png)

- the name, description and picture
- the transport it arrived on, and its native data format
- the product instance ID and serial number
- the endpoint device ID, with a button to copy it
- every MIDI 1.0 input/source and output/destination port belonging to this endpoint, kept live as ports appear and go away
- whether the device reports an identifier of its own

That last one matters more than it looks. A device that reports a serial number or a product instance ID can be recognized wherever you plug it in, so the name and picture you give it follow it around. A device that reports neither is matched by the port it is plugged into, and moving it may lose your customization.

Everything else Windows knows about an endpoint - function blocks, group terminal blocks, declared protocol, stream configuration - is available from the MIDI console. It is too much detail for this app.

## Customizing an endpoint

**Customize** lets you set the name and description the endpoint shows everywhere in Windows, and attach a picture to it. Pictures are copied into a shared folder, so every MIDI app shows the same artwork for the same device.

![Giving an endpoint a name, description and picture]({{ site.baseurl }}/assets/images/midisettings-customize.png)

The change is applied to the running service immediately and saved to the configuration file so it survives a restart.

## Naming the MIDI 1.0 ports of a device

A USB device attached through Kernel Streaming gets a MIDI 1.0 port for each of its groups, and those are what older applications see through WinMM and WinRT MIDI 1.0. **Edit port names**, in the customize dialog, is where you name them. It only appears for devices which have those ports.

![Naming the individual MIDI 1.0 ports of a device]({{ site.baseurl }}/assets/images/midisettings-port-names.png)

Inputs and outputs are listed separately, in group order. For each port you see the name it has now, the two names Windows can generate for it, and a box for a name of your own.

**Name style for this device** overrides the machine-wide default for this one device:

- **Use the global default** follows the setting in Global MIDI settings, and tells you which style that currently resolves to
- **Compatible with older Windows** keeps the name older applications have always seen
- **New style, based on the endpoint and block names** is clearer, especially on a multi-port interface

Seeing both generated names side by side is the point of the grid: you can tell what switching style would do to each port before you commit to it.

A name of your own overrides whichever style is selected, and is capped at 31 characters because that is all WinMM stores. Leave the box empty to use the generated name.

> Renaming takes effect when the MIDI service restarts, and applications which remember ports by name
> will need to be pointed at them again. There is a button to restart the service in Global MIDI settings.

## Configurations

Your device and port names, pictures, loopbacks, network and Bluetooth connections, and transport settings are kept in a configuration. You can keep more than one, such as one for the studio and one for the stage, but only one is in use at a time.

**Configurations**, at the right end of the toolbar, opens a window of its own with four pages: **Configurations**, **Backups**, **Share**, and **Import**. A configuration is shared by everyone who uses this PC, so changing one needs administrator rights. Without them, you can still look around and share, and **Reopen as administrator** brings you back to the same page with the rights you need.

Use this window rather than copying configuration files yourself. Where they're kept and what's in them can change in any update.

### Switching configurations

The **Configurations** page shows the configuration in use at the top, with **Back up now** and **Share...** beside it. The others on this PC are under **Also on this PC**.

![The Configurations page, with the configuration in use at the top and the others on this PC below it]({{ site.baseurl }}/assets/images/midisettings-configurations.png)

- **New configuration** makes an empty configuration, or a copy of one you already have.
- **Use this one** switches to a configuration. The switch happens when the MIDI service restarts, and the page offers to restart it for you. Close your music apps first, because anything using MIDI loses its connections.
- **More options** (**...**) on a configuration renames, duplicates, backs up, shares, or deletes it. Deleting backs it up first, so you can get it back from **Backups**.

A new, empty configuration doesn't have the default loopbacks. Make those again with the **Create default loopback** buttons in [Windows MIDI Loopback Setup]({{ site.baseurl }}/tools/midiloopbacksetup/#the-default-loopbacks).

### Backups

The **Backups** page lists every backup, newest first, with why it was made. Windows MIDI Settings makes one before every import, restore, and delete, and you can make one any time with **Back up now**. Each one keeps the configuration, its pictures, and the port names setting for older apps.

Windows MIDI Services also backs up a configuration before the first change to it each day. Those backups are marked **Configuration only**, because they don't have the pictures or the port names setting.

![The Backups page, with a backup selected and what's in it shown next to what's in use now]({{ site.baseurl }}/assets/images/midisettings-backups.png)

Select a backup to see what's in it next to what's in use now. **Restore** puts it back. What's there now is backed up first, so a restore can be undone too. If the configuration is in use, the restored version takes over when the MIDI service restarts.

Nothing deletes backups by itself. **Delete old backups...** deletes the ones older than the time you choose. It can also keep the newest few of each configuration, however old they are.

### Sharing a configuration

The **Share** page saves a package: one `.midiconfigpack` file with a configuration, its pictures, and the port names setting for older apps. Pick the configuration, tick what to include, add a note if you like, and select **Save package...**. Then take the file to the other PC, such as on a USB drive. Sharing doesn't need administrator rights.

### Importing a configuration

The **Import** page opens a package. You can also drop a package on the page, or double-click a `.midiconfigpack` file. Configuration files saved by older versions of Windows MIDI Settings work too.

Opening a package only shows what it would do. Nothing changes until you select **Import**.

![The Import page with a package open, showing what each part will do to this PC]({{ site.baseurl }}/assets/images/midisettings-import.png)

**Put it into** chooses where it goes. The configuration in use gets what's new and keeps what you have. A new configuration is kept apart, and you can switch to it when you like.

**What will happen** has a line for each part of the package, with a choice:

- When the package and this PC disagree, your version is kept unless you choose otherwise. That covers device names, the synthesizer mode, and the port names for older apps.
- A loopback or network host whose name is already used here is left out unless you tick it. Ticked, it gets a free name, such as "Max 2".
- A network host named after the other PC takes this PC's name, so the two PCs don't show up on the network with the same name.
- RTP-MIDI settings are saved even if RTP-MIDI isn't installed on this PC. They start working once it is.

Select **Import** to make the change. The configuration is backed up first, and the import starts working when the MIDI service restarts. If you change your mind, **Undo this import** puts it back the way it was.

A device that reports a serial number gets its name and picture as soon as it's plugged in. Other devices are matched by the port they're plugged into, so on a new PC their settings may not find them. Windows MIDI Settings then shows **Some saved settings have no matching device**. Select **Review them** to move the settings onto the right device.

## Global MIDI settings

These are shared by everyone who uses the PC, so they need administrator rights. If the app is not running elevated it says so and offers to restart itself with the rights it needs.

![The global MIDI settings, shared by everyone who uses the PC]({{ site.baseurl }}/assets/images/midisettings-global.png)

**Configuration** - the name of the configuration in use. **Manage configurations...** opens the [Configurations](#configurations) window, where you switch, back up, restore, share, and import.

**Default MIDI 1.0 port naming** - how ports are named for older applications which use WinMM or WinRT MIDI 1.0. *Compatible with older Windows* keeps the names those applications have always seen. *New style* names ports from the endpoint and block names, which is clearer but will make applications that remember port names by string ask you to pick the device again.

**MIDI service** - switching configurations or changing the port naming takes effect when the service restarts, and there's a button here to do that.

## Appearance

The palette button in the title bar sets the theme, the window material and a custom window color, and the pin next to it keeps the window on top. These are the same settings every Windows MIDI Services tool has, and they are remembered per app.
