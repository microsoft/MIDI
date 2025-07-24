---
layout: kb
title: How to Install Preview/Development Builds
audience: everyone
description: How to install Windows MIDI Services preview builds.
---

# How to Install Preview Service builds

Windows MIDI Services has been designed from the start to be expandible with new transports, and in the future, with new transforms, by the developer community, all without having to write kernel drivers. These components are delivered as COM DLLs implementing specific interfaces, and then registered on the local computer.

**This article covers the components which are normally delivered in-box in Windows: The Service and released Service Plugins in addition to preview versions of those components (e.g. Network MIDI 2.0 transport plugin).**

## Evaluate if you need to do this

If you are not a developer, we generally recommend you stick with the Service and Plugins already installed in-box in Windows. But if you are on a Canary Windows Insider build, we assume you are aware of the risks with running preview bits.

> IMPORTANT: The builds of the MIDI Service and Plugins available on GitHub are debug builds (this does not impact the SDK and runtime, just the service and service plugins). They will be slower than what is shipped in-box in Windows. And, importantly, **they capture MIDI data in any ETL trace you initiate**. Therefore if you submit a Feedback Hub entry with "reproduce my problem", or otherwise make the ETL trace available to others, it will contain the MIDI messages that were sent during recording. For privacy reasons, our in-box shipped version of the MIDI Service and Plugins does not capture this information.

## Required: Uninstall any previous developer packages

In `Settings > Apps > Installed Apps` uninstall any previously installed developer / preview packages for Windows MIDI Services.

## Required: Turn on Developer Mode in Windows

Normally, the service will checked for signed binaries before loading them. If you install the service and then run an application, you will not see any endpoints because no transports have been loaded.

If Developer Mode is turned on in settings, the service skips the check for signing and loads the binaries.

`Settings > System > For developers > Developer Mode`

![Developer Mode]({{ site.baseurl }}/assets/images/settings-developer-mode.png)

# Easiest Way: Let the installer handle the rest

In the past, there were multiple steps involved in replacing the service and plugins. Now, the installer handles it all.

The Windows MIDI Services Service and Plugins installer handles setting permissions and overtaking the entries written by the default install with Windows. It is no longer necessary, as of Customer Preview 3, to take any additional steps unless you are also replacing wdmaud2.drv.

# If you need to replace wdmaud2.drv

wdmaud2.drv is required only for WinMM (MIDI 1.0) applications. In most cases, you'll want to use the version that is shipped in the Insider Canary release, not the version from GitHub. See the release notes for what you are installing.

After the Service and components installer completes, unzip the `wdmaud2` zip file for your processor architecture (Arm64 or x64) to a known location on your PC.

There is a readme file in the zip with further instructions.

> NOTE: The development version of wdmaud2.drv we ship from GitHub is only 64 bit. 32 bit applications, like MIDI-OX and MidiClock, cannot use the version we ship. Windows itself includes both the 32 and 64 bit version.

## Reboot

After the wdmaud2.drv replacement has completed, reboot your PC.
