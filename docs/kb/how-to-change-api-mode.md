---
layout: kb
title: How to change the API mode
description: How customers can change the API mode or revert to the older MIDI 1.0 stack.
audience: everyone
categories:
  - Troubleshooting
---

## API modes

With the June 2026 Week "D" 30-day CFR (Controlled Feature Rollout) in KB5095093, we added the ability to roll back to the old MIDI stack in Windows. "Seekers", people who go into Windows Update and look for updates, had this by the end of July 2026. Everyone else had it by mid-August 2026.

Your PC needs to be fully up to date and healthy to use this. **If you have uninstalled updates, or used ViVeTool or something like it, repair your PC first using Windows Settings > System > Recovery > "Fix problems using Windows Update".**

> If your PC is joined to a domain instead of signing in with a Microsoft account, you probably don't have this yet. Domain policy blocks controlled feature rollouts by default.

> Some Insider builds don't get these updates. Insider Canary, for example, never received that KB, and without it you don't have this feature.

## Mode 0: Full Windows MIDI Services

This gives you multi-client support, MIDI loopbacks, Network MIDI 2.0, and Bluetooth LE MIDI. If MIDI is working on your PC, stay here. It's the default and it's what we recommend.

## Mode 1: Legacy API mode

This puts your PC back the way it was before Windows MIDI Services, with the same limits and the same behavior. Ports aren't multi-client, new-style and custom port names aren't available, and neither the MIDI service nor `wdmaud2.drv` is used. You lose the built-in loopbacks, Network MIDI 2.0, Bluetooth LE MIDI, and the ability to use USB MIDI 2.0 devices.

We made this available for two reasons:

- **The inMusic driver bug.** inMusic (AKAI, Rane, NuMark, Alesis, M-Audio and others) isn't able to fix their driver for a lot of controllers and devices that are out of service, and we want those to keep working on Windows. If you depend on one of their devices and drivers, and it isn't a class-compliant MIDI device (DJ sets and FireWire audio and MIDI interfaces, for example), this mode gets rid of the crash you see when the device is unplugged while it's in use. Apps can still hang with these drivers, as they always have on Windows, but unplugging and replugging the device or restarting the app usually clears it.
- **Very old Hercules DJ controllers** that use DirectMusic drivers, which Windows dropped back in Vista. This is a way to keep using them instead of throwing them out.

Anyone can use this mode, but it's mainly for DJs who don't need the newer MIDI features and do need their controller not to lock up the PC if it gets unplugged mid-set.

Here's what you give up:

- More than one application using the same MIDI device at the same time (multi-client)
- Built-in loopback ports (on x64, use something like loopMIDI or loopBE instead)
- Network MIDI 2.0, Bluetooth LE MIDI, and other new transports
- MIDI 2.0 devices such as the Montage M, Native Instruments Komplete Kontrol mk3, StudioLogic SL series, Waldorf Iridium family and Roland A88 when they're set to MIDI 2.0 mode. All of those have a MIDI 1.0 mode you can use instead.
- The new, faster combined MIDI 1.0 and MIDI 2.0 driver

## Mode 2: Hybrid API mode

Use this carefully. Devices on MIDI 1.0 drivers are visible only to the old APIs, and devices on the new combined MIDI 1.0 and MIDI 2.0 driver are visible only to the new API. Neither side sees the other. That confuses a lot of people, because which MIDI ports you see depends on both the driver and the app.

We don't recommend this mode unless you have a clear reason and you understand what it does.

## How to change the mode

Read the descriptions above first, rather than skipping straight here.

### Using the MIDI Troubleshooting and Repair app

> This app starts shipping in Windows near the end of 2026. Right now it's available to developers and technical users through our GitHub repo, [https://aka.ms/midirepo](https://aka.ms/midirepo).

The **API mode** page in the MIDI Troubleshooting and Repair app shows which mode this PC is using, describes what each mode gives you and takes away, and changes it for you without any registry editing. The setting is machine-wide and only takes effect after a restart, so the app writes the value and then asks whether you want to restart now.

![The API mode page in the MIDI Troubleshooting and Repair app]({{ site.baseurl }}/assets/images/miditroubleshooter.png)

For more about the app, see [MIDI Troubleshooting and Repair]({{ site.baseurl }}/tools/miditroubleshooter/).

### Using the registry

If you don't have the app, and you're comfortable with `regedit`:

1. Open `regedit.exe`
2. Go to `Computer\HKEY_LOCAL_MACHINE\SOFTWARE\Microsoft\Windows NT\CurrentVersion\Drivers32`
3. Right-click in that location and add a new **DWORD (32-bit) Value** named `UseLegacyMidi`
4. Right-click the value and choose "Modify"
5. Set it to one of the modes above: 0, 1 or 2
6. Restart the PC for it to take effect

## Important tips

- Don't use the MIDI tools (MIDI Console, MIDI Settings) while you're in Legacy mode. Some of them try to start the service.
- Don't use `midicheckservice`.
- To check that your PC really is in Legacy mode, restart, use a MIDI 1.0 app, and look for `midisrv` in the Services app or on the Details tab of Task Manager. If MIDI is working and `midisrv` is not running, you're in Legacy mode.

## Note for hardware and software manufacturers, and for support staff

If you want to give these instructions to your customers, please link to this article instead of copying the text. Changing the API mode is going to get friendlier, and this page will be updated when it does.

