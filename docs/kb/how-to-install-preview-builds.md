---
layout: kb
title: How to Install Preview/Development Builds
audience: everyone
description: How to install Windows MIDI Services preview builds.
---

# How to Install Preview Service builds

Windows MIDI Services has been designed from the start to be expandible with new transports, and in the future, with new transforms, by the developer community, all without having to write kernel drivers. These components are delivered as COM DLLs implementing specific interfaces, and then registered on the local computer.

## Evaluate if you need to do this

If you are a regular musician, we generally recommend you stick with what is installed in-box in Windows. If, however, you are technically astute, and safe in your computer practices, you can unlock Windows MIDI Services to enable in-development service plugins to be installed, and service settings to be changed.

## Required: Uninstall any previous developer packages

In Settings > Apps > Installed Apps uninstall any previously installed developer packages for Windows MIDI Services.

## Required: Turn on Developer Mode in Windows

Normally, the service will checked for signed binaries before loading them. If you install the service and then run an application, you will not see any endpoints because no transports have been loaded.

If Developer Mode is turned on in settings, the service skips the check for signing and loads the binaries.

Settings > System > For developers > Developer Mode

![Developer Mode]({{ site.baseurl }}/assets/images/settings-developer-mode.png)

# Easiest Way: Let the installer handle the rest

In the past, there were multiple steps involved in replacing the service and plugins. Now, the installer handles it all.

The Windows MIDI Services Service and Plugins installer handles setting permissions and overtaking the entries written by the default install with Windows. It is no longer necessary, as of Customer Preview 3, to take any additional steps unless you are also replacing wdmaud2.drv.

# If you need to replace wdmaud2.drv

After the Service and components installer completes, unzip the `wdmaud2` zip file for your processor architecture (Arm64 or x64) to a known location on your PC.

Then click the "Replace wdmaud2.drv ... " button on the developer page.

## Reboot

After the wdmaud2.drv replacement has completed, reboot your PC.

