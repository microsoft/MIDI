---
layout: kb
title: How to switch between MIDI class drivers
description: How to move between the older usbaudio.sys and the newer usbmidi2-acx.sys drivers
audience: everyone
categories:
  - Troubleshooting
---

Windows has two drivers that can run a class-compliant USB MIDI device.

- **USBMidi2-ACX** is the new one that came with Windows MIDI Services. It handles both MIDI 1.0 and MIDI 2.0 devices.
- **USB Audio Device** (`usbaudio.sys`) is the older MIDI 1.0 driver. We kept it because the new driver isn't compatible with every device.

If a device doesn't work well on one driver, you can move it to the other. There are two ways to do that, and the app is the easier one, so try it first.

> **Only switch the driver on a device you know is a MIDI device.** Windows offers the MIDI class driver for any USB audio device. Choosing it for something that isn't a MIDI device can stop that device from working, or make the PC stop responding until you restart it. If a device needs a driver from its manufacturer, neither of these two drivers will work for it.

Before you start, close your DAW and any other app that's using MIDI, and save your work. Changing a driver disconnects the device, and some apps don't recover from that well.

## Using the MIDI Troubleshooting and Repair app

The MIDI Troubleshooting and Repair app lists every USB MIDI device on the PC, shows which driver each one is using right now, and gives you a button to switch it to the other one. That's quicker than the steps below, and you don't have to pick the right device out of a long list of hardware. Open the app, go to the **Drivers** page, and choose the driver you want.

For more about the app, see [MIDI Troubleshooting and Repair]({{ site.baseurl }}/tools/miditroubleshooter/).

## Using Device Manager

Use this if you don't have the MIDI Troubleshooting and Repair app.

### Step 1: Open Device Manager

Right-click the Windows logo on the taskbar and choose "Device Manager". You can also search for "Device Manager" in the search box.

![Open Device Manager](switch-drivers-step-1.png)

### Step 2: Find the device under "Sound, video and game controllers"

The name of this section changes with your language settings. You want the section with the hardware devices in it, not "MIDI Endpoints" and not "Software Devices".

![Find the Hardware Device](switch-drivers-step-2-device-manager.png)

To see which driver a device is using now, right-click it and choose "Properties". On the "General" tab, the manufacturer is "Microsoft" when the new combined MIDI 1.0 and MIDI 2.0 class driver is in use, and "(Generic USB Audio)" when the old driver is in use.

### Step 3: Right-click the device and choose "Update Driver"

![Device context menu](switch-drivers-step-3-right-click-menu.png)

### Step 4: Choose "Browse my computer for drivers"

Read this step and the next one carefully. If you pick the wrong option, Windows tells you that you already have the best driver and stops. That's the choice we're deliberately overriding.

![Device context menu](switch-drivers-step-4-browse.png)

### Step 5: Choose "Let me pick from a list of available drivers on my computer"

![Pick from available drivers](switch-drivers-step-5-pick-from-available.png)

### Step 6: Choose the driver

- For the old MIDI 1.0 class driver, choose "USB Audio Device"
- For the new MIDI 1.0 and MIDI 2.0 class driver, choose "USBMidi2-ACX"

If you don't see both of these, then either the device needs a driver from its manufacturer, or you picked an audio endpoint instead of the MIDI device.

![Pick the driver you want](switch-drivers-step-6-choose-driver.png)

### Step 7: Finish up

Choose "Next". When it's done, unplug the device, **restart your PC**, and plug the device back in. Not every device needs this, but many do.

## Windows updates can undo your choice

There's a Plug and Play bug in Windows right now that **can put the old driver back after an update**. The Plug and Play team is looking into it. Until it's fixed, you may have to do this again after an update.

If that becomes a nuisance and you don't need the new MIDI features, you can switch the PC to the old API mode by [following the instructions here]({{ site.baseurl }}/kb/how-to-change-api-mode/). In that mode, the device is only allowed to use the older driver.

## Hubs and devices without a serial number

Some USB devices don't have a serial number, and some hubs don't either. When that's the case, plugging the device into a different USB port makes Windows build its information from scratch. Your driver choice is lost, and so are custom names and other settings for that device. You'll need to repeat the steps above.