---
layout: kb
title: How to change the API mode
description: How customers can change the API mode or revert to the older MIDI 1.0 stack.
audience: everyone
---

# API Modes

With the June 2026 Week "D" 30-day CFR in KB5095093, we've introduced the ability to roll back to the old MIDI stack in Windows. That means that "seekers" (people who go into Windows Update and look for updates) will have this feature enabled by the end of July 2026. Others will have it enabled by mid-August 2026.

You must be fully up to date with a healthy Windows 11 system to use this feature. If you have uninstalled any updates or used Vive tool or similar, please repair your PC using Windows Settings > System > Recovery > "Fix problems using Windows Update".

## Mode 0: Full Windows MIDI Services

This gives you multi-client support, MIDI loopbacks, Network MIDI 2.0, upcoming BLE MIDI 1.0 and 2.0, etc. If MIDI is working fine on your PC, and you are using the new multi-client features etc. then stick with this mode. It is the default and recommended mode.

## Mode 1: Legacy API Mode

This puts you back in the same mode before Windows MIDI Services, with the same limitations and behavior. Ports are not multi-client, new-style and custom port names are not available, and the MIDI Service and wdmaud2.drv are not used. You will have no access to the new built-in loopbacks, Network MIDI 2.0, upcoming Bluetooth LE MIDI, or the ability to use USB MIDI 2.0 devices. 

We've made this available primarily for two reasons:

- The inMusic driver bug. inMusic (AKAI, Rane, NuMark, Alesis, m-audio, etc.) isn't going to be able to fix their driver for lots of out-of-service controllers and devices, but we want those to function on Windows. If you rely on their devices and drivers, and they are not class-compliant MIDI devices (for example: DJ sets and firewire audio/midi interfaces) then this option eliminates the problem where disconnecting the device while in use will crash MIDI on the PC. There will still be hangs in apps when using these drivers, as there have been in Windows in the past. But these can usually be remedied by plugging and unplugging the device or restarting the app.
- Certain very old Hercules DJ controllers which use DirectMusic drivers, deprecated with Windows Vista. This provides an option to still use these devices on a modern PC vs sending them to the landfill.

Although this can be used by anyone, this mode is primarily for DJs who don't need advanced MIDI features, but do need their DJ controller not to lock up the PC if accidentally disconnected during a set.

## Mode 2: Hybrid API Mode

Use with caution. Devices using MIDI 1.0 drivers are available only through the old APIs. Devices using the new combined MIDI 1.0/MIDI 2.0 driver are available only through the new API. The two do not see each other at all. This will be confusing for many because the MIDI ports seen are going to depend on the driver used and the API used by an app.

In general, we don't recommend this mode unless you have a clear need and a full understanding of the implications.

# How to change the mode

Ensure you have first read the descriptions above, and not just skipped to this section.

This will eventually be in the MIDI Settings app, once the feature is fully enabled for everyone. However, for those who need this now, and are familiar with how to use regedit, the information is as follows.

1. Open `regedit.exe`
2. Set the location to `Computer\HKEY_LOCAL_MACHINE\SOFTWARE\Microsoft\Windows NT\CurrentVersion\Drivers32`
3. Right click in that location and add a new **DWORD (32-bit) Value** named: `UseLegacyMidi`
4. Right click on that value, and choose "Modify"
5. Set the value: one of the above modes: 0, 1, 2
6. Reboot for it to take effect.
