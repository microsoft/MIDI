---
layout: tools_page
title: MIDI Troubleshooting and Repair
tool: miditroubleshooter
description: Check the health of MIDI on this PC, collect diagnostics for support, and repair common problems
---

> This page covers information about a Windows MIDI Services feature and application that will be released to consumers in November 2026. It's currently available for developers.

MIDI Troubleshooting and Repair is where you go when MIDI on this PC isn't behaving. It shows you what
Windows MIDI Services thinks the state of the machine is, collects the reports support will ask for,
and fixes the handful of things that commonly get broken by other software.

Most of what it does needs administrator rights, so it asks for them when it starts. If you say no, it
still runs: the diagnostics and the informational pages work, and a banner across the top offers to
restart it with the rights it needs.

At the top of every page is a strip with the version of Windows, the architecture, your language and
region, the Windows App SDK version and the current API mode. That's the first thing support asks for,
so it stays visible wherever you are in the app.

![The MIDI Troubleshooting and Repair main window]({{ site.baseurl }}/assets/images/miditroubleshooter.png)

## API mode

Windows can run MIDI in one of three modes. This page shows which one this PC is using, explains what
each one gives you and takes away, and lets you change it.

The mode is a machine-wide setting and does not take effect until Windows restarts, so changing it here
writes the setting and then *asks* whether you want to restart now. Nothing reboots without you saying so.

- **Windows MIDI Services** is the default and what you want unless you have a specific reason not to.
  Multi-client access, loopback endpoints, Network MIDI 2.0, Bluetooth LE MIDI, MIDI 2.0 devices and
  the combined MIDI 1.0 and MIDI 2.0 class driver.
- **Legacy MIDI** puts the PC back on the MIDI stack as it was before Windows MIDI Services. Choose it
  if you have an application, driver or device that isn't compatible, or you need legacy WinMM
  user-mode drivers or DirectMusic drivers.
- **Hybrid** is not recommended for most customers. Devices on a MIDI 1.0 driver are visible only to
  the old APIs, devices on the new class driver only to the new API, and the two do not see each other.

There's more detail in [How to change the API mode]({{ site.baseurl }}/kb/how-to-change-api-mode/).

## Diagnostics

Two reports, each with a button to run it, a box showing the output, and **Copy all** and **Save as**
buttons.

![The two diagnostic reports, with output]({{ site.baseurl }}/assets/images/miditroubleshooter-diagnostics.png)

**mididiag** is the broad one. It walks the registry, the transports, every MIDI endpoint the service
knows about and the old MIDI 1.0 APIs, and reports what it found.

**midiksinfo** is the narrow one, for USB and other kernel streaming devices. If a device isn't showing
up, or is showing up with the wrong number of ports or the wrong names, this is the report that says why.

Both can take a minute or two on a machine with a lot of hardware attached. Paste the output into your
GitHub issue or your email to support.

## Capture repro log

This is the one to use when somebody asks you for "a trace". It replaces the older CollectMidiLogs
script, and does the whole job inside the app.

![Choosing what to include in a repro log capture]({{ site.baseurl }}/assets/images/miditroubleshooter-capture.png)

Choose what to include, select **Start capture**, reproduce whatever is going wrong, then select
**Stop and save**. You're asked where to put the zip file, and Explorer opens with it selected when
it's done.

By default the package contains:

- an ETW trace of the MIDI service and the MIDI components in Windows, recorded while you reproduced
  the problem
- a time travel trace of the MIDI service, which lets an engineer step backwards through what the
  service actually did
- the mididiag and midiksinfo reports
- ddodiag, dxdiag and an export of the plug and play state
- a summary of this PC and the state of the MIDI service

A time travel trace makes the package much larger, but it is very often the thing that lets us find a
problem from a single capture instead of asking you to reproduce it again, so it's on by default.

Time travel tracing isn't available on every build of Windows, and it can be refused if the service is
running with certain security mitigations. If that happens, the capture carries on without it and says
so in the log, rather than failing.

## Active sessions

Every application currently connected to the MIDI service, with the process it belongs to and when it
started. Expand one to see which endpoints and MIDI 1.0 ports it has open, how many times each is open,
and since when.

![The applications connected to the MIDI service and what each has open]({{ site.baseurl }}/assets/images/miditroubleshooter-sessions.png)

This page is purely informational; there is nothing to change here. It's for answering the question
"what has that device open?" when something reports a device as busy or in use.

## Enabled transports

Transports are the pieces of the MIDI service that talk to a particular kind of device: USB, Network
MIDI 2.0, Bluetooth LE, the loopbacks and so on.

This page lists everything registered on the PC, and marks each one as loaded, disabled or failed. A
transport that is registered but didn't load is the reason a whole class of device can go missing at
once, and it's shown with a clear icon rather than simply being absent from the list.

![Every registered transport and whether it loaded]({{ site.baseurl }}/assets/images/miditroubleshooter-transports.png)

If something here is not loading, see [Why a service plugin may not load correctly]({{ site.baseurl }}/kb/service-plugin-not-loaded/).
The usual cause for a third-party transport is that it isn't signed and Developer Mode is off.

## MIDI service

The state of the MIDI service itself: whether it's running, how it's set to start, the account it runs
under, its process id, and the version of the service on disk.

![The state of the MIDI service]({{ site.baseurl }}/assets/images/miditroubleshooter-service.png)

**Restart the MIDI service** stops and restarts it. Every application using MIDI loses its connections
and has to reconnect, so close or save your work first.

**Start with Windows** changes the service from starting on demand to starting with Windows. The
service normally starts when the first MIDI application asks for it, which adds several seconds to that first
application's startup. If you use MIDI regularly, starting it with Windows is the better choice.

## Registry

Windows MIDI Services relies on a small number of registry values, and third-party installers, driver
uninstallers and "MIDI port reordering" utilities are known to break them. When they're wrong, MIDI
either stops working entirely or silently falls back to the old stack.

This page shows what's actually there, with a green, amber or red marker against each value and a plain
description of what it's for. If anything is wrong, **Repair these entries** lists exactly what it would
change, asks you to confirm, and then makes only those changes. Windows has to be restarted afterwards,
because the audio and MIDI services read these values when they start.

![The registry values Windows MIDI Services depends on]({{ site.baseurl }}/assets/images/miditroubleshooter-registry.png)

If the PC is in Legacy MIDI mode, no changes are offered. Legacy mode deliberately does not use the
service, so "repairing" the values would undo the mode you chose.

There's a full description of what belongs here in
[Required registry entries]({{ site.baseurl }}/kb/required-registry-entries/).

## Drivers

Two jobs on this page.

![Removing vendor drivers and switching the MIDI class driver]({{ site.baseurl }}/assets/images/miditroubleshooter-drivers.png)

### Removing drivers you no longer need

Windows MIDI Services replaced the reasons most people installed a vendor MIDI driver. The KORG USB
MIDI driver and the KORG BLE-MIDI driver each have an entry here, showing whether the package is
installed and offering to remove it along with any device using it.

Bluetooth LE MIDI is now built into Windows MIDI Services, so the KORG BLE-MIDI driver is no longer
needed and can conflict with the in-box support. Bluetooth LE devices are set up in the
Windows MIDI Bluetooth Setup app afterwards.

### Switching between the MIDI class drivers

Windows has two class drivers a class-compliant USB MIDI device can use: the new combined MIDI 1.0 and
MIDI 2.0 driver, and the older USB Audio class driver. Each device is listed with the driver it's using
now and buttons to switch it to the other one, which saves the trip through Device Manager described in
[How to switch between MIDI class drivers]({{ site.baseurl }}/kb/how-to-switch-between-class-drivers/).

Devices that can only be audio are left out of the list, but the list is not a guarantee that everything
on it is a MIDI device. Windows offers the MIDI class driver for any USB audio device, and forcing it
onto one that can't use it may stop that device working or make the PC stop responding until it is
restarted. Only change the driver for a device you know is a MIDI device.

Close your MIDI applications before changing a driver. Afterwards, unplug the device, restart Windows,
and plug it back in; not every device needs this, but many do.

> There is a Plug and Play issue in Windows which can revert your choice after an update, so you may
> have to repeat this occasionally.
